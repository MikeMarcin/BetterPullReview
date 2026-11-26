#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP

#include <QDateTime>
#include <QElapsedTimer>
#include <QFont>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_draft_all.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_live_turn.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/quick_ask.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/agent_reference_links.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_session_menu.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/agent_tab_run.hpp"
#include "bpr/qt/chat_composer.hpp"
#include "bpr/qt/chat_header_view.hpp"
#include "bpr/qt/chat_history_menu.hpp"
#include "bpr/qt/chat_transcript_view.hpp"
#include "bpr/qt/explain_file_card.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/report_tally_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class QAbstractButton;
class QLabel;
class QPlainTextEdit;
class QMenu;
class QTimer;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;

namespace bpr::qt {
	class skill_completer;
	class agent_setup_view;
	class drawer_empty_state;
	class markdown_body_view;
	class appearance_profile;

	/// \brief What a card says once it was acted on.
	enum class agent_finding_card_state : std::uint8_t {
		open,
		working,
		drafted,
		/// RFC-BPR-0127. Kept as a Local comment (once "saved as a note").
		noted,
		dismissed,
		/// The action could not be done; the card says why and stays open.
		refused,
	};

	/// \brief Owner report 2026-11-10, after BPR-2052. One finding card of a
	/// stored answer, made again under its turn as the reviewer left it: the
	/// host read it from the answer's findings block and its kept disposition.
	struct agent_restored_finding_card
	{
		/// What its requests and reported states carry.
		std::uint64_t            Session = 0U;
		std::size_t              Ordinal = 0U;
		core::agent_finding      Finding;
		agent_finding_card_state State = agent_finding_card_state::open;
		QString                  Why;
		std::uint64_t            Note = 0U;

		auto operator==(const agent_restored_finding_card&) const -> bool = default;
	};

	/// \brief [[BPR-1753]], owner decision (b) 2026-10-23. One finished
	/// turn of the thread, as it is kept for its pull request and shown again.
	struct agent_thread_turn
	{
		QString Question;
		/// RFC-BPR-0125 (AC07). The suggested prompt it was sent as, when
		/// that is not its question; empty for a typed question.
		QString Name;
		/// [[BPR-2237]]. Sent from a prompt with its text changed: the text is
		/// shown as written, under the name.
		bool    Edited = false;
		QString Answer;
		QString Footer;
		/// When it was asked and answered; invalid when not known.
		QDateTime AskedAt;
		QDateTime AnsweredAt;
		/// [[BPR-1847]]. How it ended: an answer, or an error that offers
		/// no keep action.
		core::agent_turn_outcome Outcome = core::agent_turn_outcome::answered;
		/// The cards its stored answer's findings block makes, when the host
		/// read them; a turn whose cards this view still holds keeps those.
		std::vector<agent_restored_finding_card> Cards;
		/// [[BPR-2153]]. What the reviewer steered into it, shown as their own
		/// messages where they went in.
		std::vector<application::agent_steered_message> Steered;
		/// [[BPR-2241]]. How long its run took, when `Footer` is the stored
		/// outcome rather than the line it settled with.
		std::optional<qint64> Seconds;

		auto operator==(const agent_thread_turn&) const -> bool = default;
	};

	// agent_run_settings, agent_run_settings_dispatch and
	// agent_model_display_name live in agent_run_settings_picker.hpp ([[BPR-1878]]).

	/// \brief [[BPR-1753]], owner decision (a) 2026-10-23. What the
	/// suggested prompts may name: the open file's path, and the selected
	/// lines, one-based, first and last.
	struct agent_suggestion_context
	{
		QString                                                OpenFile;
		std::optional<std::pair<std::uint32_t, std::uint32_t>> Lines;
		/// RFC-BPR-0125. The pull request's names and the facts a suggested
		/// prompt's conditions read, as the window holds them; none is
		/// unknown, which no condition holds for.
		std::optional<core::prompt_pull_request> PullRequest;
		std::optional<std::string>               Repository;
		std::optional<bool>                      Draft;
		std::optional<bool>                      AuthorIsMe;
		std::optional<bool>                      ChecksFailing;
	};

	/// \brief [[BPR-1753]], owner decision 2026-10-23 ("Yep build it
	/// like that"). What a reviewer may do with one finding card; nothing is
	/// done by itself. RFC-BPR-0127: a card offers one keep action, Draft
	/// comment where its line takes a draft and Keep local where none does
	/// (`core::decide_keep`).
	enum class agent_finding_card_action : std::uint8_t {
		/// A private line draft at the finding's lines, from its suggested
		/// comment (or its claim).
		draft_comment,
		/// A Local comment on its file, or on the pull request; never sent.
		keep_local,
		dismiss,
		/// Takes a dismissal back.
		undo_dismiss,
	};

	struct agent_finding_card_request
	{
		/// The delivery the finding came in, and its place there.
		std::uint64_t             Session = 0U;
		std::size_t               Ordinal = 0U;
		core::agent_finding       Finding;
		agent_finding_card_action Action = agent_finding_card_action::draft_comment;
	};
	using agent_finding_card_dispatch = std::function<void(const agent_finding_card_request&)>;
	/// \brief [[BPR-2061]]. Draft all: the suggested comments of one reply
	/// to draft, in its cards' order, as one batch; `Planned` already counts
	/// those passed over before drafting.
	struct agent_draft_all_request
	{
		std::uint64_t                           Batch = 0U;
		std::vector<agent_finding_card_request> Cards;
		core::draft_all_outcome                 Planned;
	};
	/// \brief How the batch ended; `aUndoable` when it made a draft that
	/// one Undo can remove. Each card's own state still goes through
	/// `present_finding_card_state`.
	using agent_draft_all_done     = std::function<void(const core::draft_all_outcome& aOutcome, bool aUndoable)>;
	using agent_draft_all_dispatch = std::function<void(agent_draft_all_request aRequest, agent_draft_all_done aDone)>;
	/// \brief Undo of one batch: the drafts it made that are unchanged since
	/// are removed (`aRemoved`); a draft edited since is kept (`aKept`). The
	/// host opens each removed draft's card again through
	/// `present_finding_card_state`.
	using agent_draft_all_undo_done = std::function<void(std::size_t aRemoved, std::size_t aKept)>;
	using agent_draft_all_undo      = std::function<void(std::uint64_t aBatch, agent_draft_all_undo_done aDone)>;
	/// \brief Whether a finding's lines are in the pull request at the
	/// current head, as far as the host knows now.
	using agent_finding_placeability = std::function<bool(const core::agent_finding&)>;
	/// \brief Owner report 2026-11-10, after BPR-2052. A card under an
	/// answer whose findings block it came from took a state the host
	/// reported: the block as written, the card's place in it, and the state,
	/// so the host keeps it past a restart.
	struct agent_finding_card_change
	{
		std::string              FindingsBlock;
		std::size_t              Ordinal = 0U;
		agent_finding_card_state State   = agent_finding_card_state::open;
		QString                  Why;
		std::uint64_t            Note = 0U;
	};
	using agent_finding_card_observer = std::function<void(const agent_finding_card_change&)>;
	/// \brief Keep local on a plain answer (RFC-BPR-0127): the text (the
	/// whole reply, or the selected part), and where the host says how it
	/// went: the Local comment it made, which Go to comment opens, when it
	/// was kept.
	using agent_answer_note_done     = std::function<void(bool aSaved, const QString& aWhy, std::uint64_t aNote)>;
	using agent_answer_note_dispatch = std::function<void(const QString& aText, agent_answer_note_done aDone)>;
	/// \brief [[BPR-1773]]. The conversation the next questions are about,
	/// as the question field's chip shows it until it is removed.
	struct agent_conversation_chip
	{
		/// The composition's own name for the conversation.
		std::string Key;
		/// What the chip reads ("src/cache.cpp line 42 · 3 comments").
		QString Text;
		/// What the agent gets, as its tooltip says.
		QString Detail;
		/// The conversation takes replies (a review thread, not my own
		/// comment): its
		/// prompts offer to draft one.
		bool Replies = false;
		/// RFC-BPR-0116 ([[BPR-2074]]). I commented in it and commits followed:
		/// its prompts lead with Was this addressed?
		bool Addressed = false;

		auto operator==(const agent_conversation_chip&) const -> bool = default;
	};
	/// \brief [[BPR-1773]], owner look 2026-10-27. Where Draft comment on
	/// an answer writes it, as the host decided when the question was sent:
	/// its own key for the place, and what the action's tooltip says.
	struct agent_answer_draft_target
	{
		std::string Key;
		QString     Tip;
		/// RFC-BPR-0127 (`core::decide_keep`). No thread or line takes a
		/// draft: the answer's one keep action is Keep local, a Local
		/// comment on the pull request, not Draft comment.
		bool Local = false;

		auto operator==(const agent_answer_draft_target&) const -> bool = default;
	};
	/// \brief Draft comment on an answer: the place's key (empty for a turn
	/// whose place was not decided, which the host drafts on the pull
	/// request), the text (the whole answer, or the part selected), and
	/// where the host says how it went. Go to draft opens the key's draft.
	using agent_answer_draft_done       = std::function<void(bool aDrafted, const QString& aWhy)>;
	using agent_answer_draft_dispatch   = std::function<void(const std::string& aKey, const QString& aText, agent_answer_draft_done aDone)>;
	using agent_answer_draft_navigation = std::function<void(const std::string& aKey)>;

	/// \brief Opens a Local comment the tab kept, in Comments.
	using agent_note_navigation = std::function<void(std::uint64_t aNote)>;
	/// \brief A change to the model, the effort, or both; empty is the
	/// agent's own default. Answers whether the host took it.

	/// \brief [[BPR-2051]]. A running turn's status and its clock.
	using agent_tab_run_observer = std::function<void(const agent_run_status& aStatus, const QElapsedTimer& aStarted)>;

	enum class agent_tab_state : std::uint8_t {
		/// No agent is set up: the tab offers Set up an agent.
		not_set_up,
		/// Ready to ask.
		idle,
		/// An invocation runs: what it reads, and Stop.
		working,
		/// The session read an earlier head: Continue at the head it read, or
		/// New session, which opens an empty one at the current head at once
		/// as History's New session does.
		head_moved,
		/// The last invocation did not finish: what happened, the question
		/// kept, Resend and Show the log, on the question field. A question
		/// refused because its session read another head shows that session's
		/// card there instead, with the same two choices as `head_moved`.
		stopped,
		/// [[BPR-1753]]. The setup is in place of the conversation.
		setting_up,
	};

	/// \brief [[BPR-1594]]. RFC-BPR-0085 D5: the Agent tab, the right drawer's
	/// fourth. A context row (the agent, the head it reads, its scope, and
	/// History), the conversation with its finding cards, and the question
	/// field with its context chips. RFC-BPR-0130 stage 5 (AC07): its
	/// controls are its own; it presents the run, the session's chat and the
	/// findings the window hands it, and each gesture is an intent the window
	/// maps to the host it already reached, so the agent's authority is
	/// unchanged.
	class agent_tab_view final : public QWidget
	{
	public:
		/// \brief RFC-BPR-0130 stage 5 (AC07). The tab holds no other widget's
		/// control and reads none: the window hands it its run
		/// (`present_run`), its session's chat (`present_chat`) and the
		/// findings delivered (`present_findings`), and maps its gestures
		/// (`set_intents`). `aRunSettings` is the one agent, model and effort
		/// choice the footer's picker shows.
		explicit agent_tab_view(std::shared_ptr<agent_run_settings_source> aRunSettings, QWidget* aParent = nullptr);
		~agent_tab_view() override;
		agent_tab_view(const agent_tab_view&)                    = delete;
		auto operator=(const agent_tab_view&) -> agent_tab_view& = delete;

		/// \brief The run as its presenter says it now, and what moved it: a
		/// run that progressed is followed as it goes; any other change is
		/// presented again, and a quiet one only kept for what reads it next.
		void               present_run(agent_tab_run aRun, agent_tab_run_change aChange);
		[[nodiscard]] auto run() const noexcept -> const agent_tab_run& { return Run; }
		/// \brief The session's chat as the host projects it: its session,
		/// whether its moved head waits on a choice, its added context and
		/// its replies.
		void present_chat(const side_chat_projection& aChat);
		/// \brief The findings the host delivered, which the turn's cards are
		/// made of.
		void present_findings(const application::agent_finding_delivery& aDelivery);
		/// \brief Where the tab's gestures go.
		void set_intents(agent_tab_intents aIntents);
		/// \brief The private history Show every run… shows, with the way
		/// back to the conversation. The tab owns it from then on.
		void adopt_every_run(QWidget& aRuns);
		/// \brief The question field's words.
		[[nodiscard]] auto question_editor() const noexcept -> QPlainTextEdit* { return Composer; }
		/// \brief Set up an agent: Settings › Agents.
		void set_setup_dispatch(std::function<void()> aDispatch);
		/// \brief [[BPR-1753]], RFC-BPR-0092. Shows `aSetup` in place
		/// of the conversation while it is needed and adds **Reset agents…** to
		/// History. [[BPR-1876]]: an Ask that ends for a setup reason is asked
		/// again, not checked again, and says when its check has passed since.
		void adopt_setup(agent_setup_view& aSetup);
		/// \brief [[BPR-1881]]. Whether the setup has finished, from the
		/// same core verdict that holds the suggested prompts back: the
		/// setup's own state, not a second one.
		[[nodiscard]] auto setup_readiness() const -> agent_setup_readiness;
		/// \brief [[BPR-1881]]. Told when `setup_readiness()` changes.
		void set_readiness_observer(std::function<void()> aObserver);
		/// \brief [[BPR-1881]]. Shows the setup at its first unfinished
		/// step, as **Finish setup** does. Only a reviewer's click calls it;
		/// the setup never opens by itself.
		void open_setup();
		/// \brief [[BPR-1881]]. Another surface that presents the setup's
		/// way in (the Tour tab) is shown: the setup may look for agents, as
		/// it does the first time this tab shows. Once only, whichever first.
		void look_for_agents();
		/// \brief The ready line once setup finishes, and what the agent gets.
		void present_ready(const QString& aTitle, const QString& aText, const QString& aGets);
		/// \brief Called once, the first time the tab is shown: when setup
		/// may show, the host looks for agents (RFC-BPR-0092 Q2).
		void set_first_shown(std::function<void()> aObserver);
		/// \brief The model and effort each agent runs with, and where a change
		/// goes. Owner decision (c) 2026-10-23 ("A chip in the message box like
		/// VS Code"): the chip in the question field shows the chosen agent's.
		void present_run_settings(std::vector<agent_run_settings> aSettings);
		void set_run_settings_dispatch(agent_run_settings_dispatch aDispatch);
		/// \brief [[BPR-1878]]. What the agent picker shows, for another
		/// picker (the Tour tab's) that is the same choice.
		[[nodiscard]] auto run_settings_source() const noexcept -> const std::shared_ptr<agent_run_settings_source>& { return RunSettingsSource; }
		/// \brief `/model NAME`, `/effort LEVEL` (or `default`), and `/model`
		/// or `/effort` alone to open the control. Answers whether the text
		/// was such a command, which is then not sent to the agent.
		auto run_slash_command(const QString& aText) -> bool;
		/// \brief Owner look v003. The session's title in the header (the
		/// pull request's), a reply's Copy and its time, and the answers' code
		/// coloured as comments' code is (the shared Markdown code host).
		/// [[BPR-2377]]. A session the reviewer named shows its name instead;
		/// `aTitle` is its default.
		void               set_session_title(const QString& aTitle);
		[[nodiscard]] auto session_title() const noexcept -> QLabel* { return SessionTitle; }
		/// \brief [[BPR-2377]]. Renames (`aName` set) or deletes a session of
		/// the selected agent on this pull request; an empty `aId` without a
		/// name deletes them all.
		using session_edit_dispatch = std::function<void(const std::string& aId, std::optional<std::string> aName)>;
		void set_session_edit_dispatch(session_edit_dispatch aDispatch);
		/// \brief [[BPR-2377]]. History's session rows (pencil, trash, i) and
		/// Delete all sessions…: the shared `agent_session_menu`.
		[[nodiscard]] auto session_menu() const noexcept -> agent_session_menu* { return SessionMenu; }
		/// \brief RFC-BPR-0130. The header's History menu, the tour chat's
		/// own: New session first, the sessions, carrying the open one over,
		/// Delete all sessions…, Show every run…, Reset agents…, the revision
		/// read.
		[[nodiscard]] auto history_menu() const noexcept -> chat_history_menu* { return HistoryItems; }
		/// \brief New session, History's first item: enabled exactly when
		/// core offers it, and greyed with its reason otherwise.
		[[nodiscard]] auto new_session_action() const noexcept -> QAction*;
		/// \brief [[BPR-2377]]. What the last rename or deletion did.
		void               present_session_edit(const QString& aStatus);
		[[nodiscard]] auto turn_copy_button(std::size_t aTurn) const -> QToolButton*;
		[[nodiscard]] auto turn_time(std::size_t aTurn) const -> QLabel*;
		void               set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void               apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		void               adopt_appearance(const appearance_profile& aProfile);
		/// \brief Owner amendment 2026-10-23: switches to another added agent,
		/// as its row in the chip's menu does. Refused while one runs.
		auto               choose_agent(std::uint64_t aProfile) -> bool;
		[[nodiscard]] auto ready_card() const noexcept -> QWidget* { return ReadyCard; }
		[[nodiscard]] auto setup_view() const noexcept -> agent_setup_view* { return SetupView; }
		/// \brief [[BPR-1753]], owner look v002 ("Is it not meant to
		/// work like the claude code vscode plugin where it's like a chat
		/// thread?"). The conversation's turns, oldest first: what was asked
		/// (or "Review this pull request"), the agent's answer as it streams,
		/// and how the turn ended.
		[[nodiscard]] auto conversation_thread() const noexcept -> QWidget* { return Transcript != nullptr ? Transcript->turns_column() : nullptr; }
		[[nodiscard]] auto turn_count() const noexcept -> std::size_t { return Turns.size(); }
		/// \brief RFC-BPR-0128. The facts the suggested prompts read now, for
		/// the Ask menus.
		[[nodiscard]] auto suggestion_context_now() const -> agent_suggestion_context { return current_suggestion_context(); }
		[[nodiscard]] auto turn_answer(std::size_t aTurn) const -> QString;
		[[nodiscard]] auto turn_question(std::size_t aTurn) const -> QString;
		[[nodiscard]] auto turn_footer(std::size_t aTurn) const -> QString;
		/// \brief [[BPR-2005]]. A turn's running block, or its folded line;
		/// null past the last turn.
		[[nodiscard]] auto turn_activity(std::size_t aTurn) const -> agent_activity_view*;
		/// \brief [[BPR-1753]], owner decision (b). Shows a pull
		/// request's kept thread in place of the one shown (none: an empty
		/// thread), and says each finished turn to `set_thread_observer`'s
		/// observer with the whole thread, so the host keeps it.
		void present_thread(const std::vector<agent_thread_turn>& aTurns);
		/// \brief [[BPR-1999]], RFC-BPR-0101. The session's total, as kept
		/// with its history (none: not known). A new thread starts with none;
		/// each turn that reports a cost adds to it. RFC-BPR-0130: it is not
		/// on the chat's face. A session's cost is said by its row of History
		/// (the row's i), and the window's footer keeps the pull request's
		/// total, which this feeds.
		void present_session_cost(std::optional<core::agent_cost> aTotal);
		/// \brief [[BPR-2051]]. The window's footer hears the tab's run as it
		/// moves, how it ended, and the session's total.
		void               set_footer_observers(agent_tab_run_observer aRun, std::function<void(const QString&)> aEnded, std::function<void(std::optional<core::agent_cost>)> aCost);
		[[nodiscard]] auto session_cost() const noexcept -> std::optional<core::agent_cost> { return SessionTotal; }
		void               set_thread_observer(std::function<void(std::vector<agent_thread_turn>)> aObserver);
		[[nodiscard]] auto thread_turns() const -> std::vector<agent_thread_turn>;
		/// \brief New session: tells the host, which keeps the conversation in
		/// History and opens an empty one; nothing while a turn runs.
		void set_clear_session_dispatch(std::function<void()> aDispatch);
		void set_clear_confirmation(std::function<bool(std::size_t aTurns)> aConfirm);
		void clear_session();
		void present_native_history(const application::native_agent_history_projection&);
		void set_native_history_dispatch(std::function<void(std::optional<std::string>)> aDispatch) { NativeHistoryDispatch = std::move(aDispatch); }
		/// \brief Owner, 2026-11-24. Where the stopped card's pinned
		/// conversation choices go: continue the conversation at its own
		/// revision, or New session at the current one. The host
		/// answers whether it took the choice. Continue resends the kept
		/// message at once, under the transcript it belongs to. New session
		/// is History's New session: the host opens an empty session at the
		/// head under review, the tab shows it, and the kept message is sent
		/// there when the host says it is open (`send_kept_message`). The card
		/// goes as the choice is taken, so the press is seen to do something.
		void set_pinned_conversation_dispatch(std::function<bool(core::agent_continuation)> aDispatch) { PinnedDispatch = std::move(aDispatch); }
		/// \brief The new session the stopped card's New session asked for is
		/// open and shown: sends the message kept for it there, as its first
		/// turn. A message that cannot leave stays in the question field,
		/// which says why. Nothing without a kept message.
		void send_kept_message();
		/// \brief The new session could not be opened: the session that asked
		/// is still shown, and its card comes back with the message kept.
		void abandon_kept_message();
		/// \brief Whether a message waits for the new session to open.
		[[nodiscard]] auto keeps_message_for_new_session() const noexcept -> bool { return KeptForNewSession.has_value(); }
		/// \brief The state card's two buttons, for courses and tests. On the
		/// card of a session that read an earlier head, before a question and
		/// after a refused one alike, the first is Continue at that head and
		/// the second New session, on one line.
		[[nodiscard]] auto banner_first_button() const noexcept -> quiet_push_button* { return BannerFirst; }
		[[nodiscard]] auto banner_second_button() const noexcept -> quiet_push_button* { return BannerSecond; }
		/// \brief Whether the state card stands on the question field, outside
		/// the scrolled thread: it does while it asks which revision to go on
		/// with, and while it says a run stopped (the thread keeps itself at
		/// its end, where the stopped turn is, so a card at its head was out
		/// of view). A run working or finished heads the thread.
		[[nodiscard]] auto banner_docked() const noexcept -> bool { return BannerDocked; }
		/// \brief What the stopped run wrote, on the stopped card under its
		/// choices while Show the log is on: as tall as its lines up to
		/// `stopped_log_lines`, and scrolled inside the card past that, so
		/// Resend and Hide the log stay where they were.
		[[nodiscard]] auto stopped_log() const noexcept -> QPlainTextEdit* { return Log; }
		/// \brief The most lines of the log the stopped card shows at once.
		static constexpr int stopped_log_lines = 9;
		/// \brief RFC-BPR-0096, owner continue-at-latest amendment, 2026-11-24.
		/// Where the stopped card's reconcile choices go when work stopped the
		/// conversation's workspace from moving: Rebase onto the new head, Keep
		/// my changes on a branch and move, or Ask the agent to reconcile. The
		/// host answers whether it took the choice; when it did, the tab
		/// resends the kept message.
		void set_move_reconcile_dispatch(std::function<bool(core::conversation_reconcile)> aDispatch) { ReconcileDispatch = std::move(aDispatch); }
		/// \brief The stopped card's reconcile buttons, in that order.
		[[nodiscard]] auto banner_reconcile_buttons() const -> std::array<QPushButton*, 3U> { return { BannerRebase, BannerSave, BannerAsk }; }
		/// \brief The quiet line a turn shows when its conversation moved to
		/// the latest head before it ran; hidden otherwise.
		[[nodiscard]] auto turn_move_notice(std::size_t aTurn) const -> QLabel*;
		/// \brief The quiet line an opened conversation shows when its shared
		/// checkout moved while another conversation asked; hidden otherwise.
		[[nodiscard]] auto conversation_move_notice() const noexcept -> QLabel* { return ConversationMoved; }
		/// \brief The quiet line of a session that does not go on in the
		/// ordinary way: one carried over from a saved transcript, or one the
		/// chosen agent cannot pick up. It names the chosen agent, stands on
		/// the question field with the other notices, and is hidden and
		/// empty for every other session.
		[[nodiscard]] auto session_notice() const noexcept -> QLabel* { return SessionNotice; }
		[[nodiscard]] auto banner_text() const -> QString;
		/// \brief [[BPR-1753]], owner decision (a) 2026-10-23
		/// ("Suggested prompts described that way sound great"). Where the
		/// suggestions read the open file and the selected lines, and a call
		/// to read them again when either changes.
		void set_suggestion_context(std::function<agent_suggestion_context()> aContext);
		void refresh_suggestions();
		/// \brief The suggested prompts shown now, in order; each is sent as
		/// written. Owner look v004: they stand above the question field
		/// whether or not the thread has turns, and the field's Suggestions
		/// toggle shows or hides them.
		[[nodiscard]] auto suggestions() const -> QStringList;
		[[nodiscard]] auto suggestion_buttons() const -> std::vector<QPushButton*>;
		[[nodiscard]] auto suggestions_toggle() const noexcept -> QToolButton* { return SuggestionsMenu; }
		/// \brief The toggle as the host kept it, said to nobody; and where a
		/// reviewer's press goes, so the host keeps it.
		void               set_suggestions_shown(bool aShown);
		void               set_suggestions_observer(std::function<void(bool aShown)> aObserver);
		[[nodiscard]] auto suggestions_shown() const -> bool;
		/// \brief Sends `aPrompt` as a chat turn, as if typed and asked; words
		/// the reviewer had in the field stay there ([[BPR-1872]]).
		void send_prompt(const QString& aPrompt);
		/// \brief [[BPR-1872]]. A press on a suggested prompt: sent, or with
		/// `aEditFirst` (a Shift-click or Shift-Return) put in the field to
		/// edit before sending. The core decides which.
		void choose_prompt(const QString& aPrompt, bool aEditFirst);
		/// \brief A press on one decided offer.
		void press_offer(const core::prompt_offer& aChosen, bool aEditFirst);
		/// \brief Reads the panels again and redraws.
		void refresh();
		/// \brief The question field takes the keyboard.
		void focus_question();
		/// \brief Shows the private history (true) or the conversation.
		void show_history(bool aShown);

		[[nodiscard]] auto state() const noexcept -> agent_tab_state { return State; }
		[[nodiscard]] auto banner() const noexcept -> QWidget* { return Banner; }
		[[nodiscard]] auto empty_state() const noexcept -> drawer_empty_state* { return Empty; }
		[[nodiscard]] auto history_button() const noexcept -> QToolButton* { return History; }
		/// \brief [[BPR-2295]]. Whether the last run stopped for a reason a
		/// resend repeats.
		[[nodiscard]] auto stopped_for_good() const noexcept -> bool;
		/// \brief [[BPR-2295]]. The close on a "stopped before answering" block.
		[[nodiscard]] auto stopped_dismiss_button() const noexcept -> QToolButton* { return BannerDismiss; }
		[[nodiscard]] auto finding_cards() const -> std::vector<QWidget*>;
		/// \brief [[BPR-1753]], owner decision 2026-10-23. Where a
		/// card's keep action (Draft comment or Keep local) and Dismiss go, and the
		/// state the host reports back for the card (`aWhy` with a refusal).
		void set_finding_card_dispatch(agent_finding_card_dispatch aDispatch);
		/// \brief [[BPR-2286]]. Ask follow-up on a finding card: the card's
		/// delivery, place and finding (its `Action` is unused). Without it
		/// the cards offer no Ask follow-up.
		void set_follow_up_dispatch(agent_finding_card_dispatch aDispatch);
		/// \brief [[BPR-2286]]. A follow-up updated the suggested comment of
		/// the card at `aOrdinal` of delivery `aSession`: the card shows it and
		/// Draft drafts it. \returns Whether a card was there.
		auto revise_finding(std::uint64_t aSession, std::size_t aOrdinal, const QString& aComment) -> bool;
		/// \brief The review draft the card at `aOrdinal` of delivery
		/// `aSession` made, while that card says Drafted; 0 for none.
		[[nodiscard]] auto finding_card_draft(std::uint64_t aSession, std::size_t aOrdinal) const -> std::uint64_t;
		/// \brief A follow-up updated a drafted card's suggested comment and
		/// its draft, which the reviewer had changed, was kept: while
		/// `aOutdated`, the card says the draft differs from the updated
		/// comment and offers Update draft, as a report's finding does. A
		/// card that stops saying Drafted stops offering it.
		void present_finding_draft_outdated(std::uint64_t aSession, std::size_t aOrdinal, bool aOutdated);
		/// \brief Where a card's Update draft goes: the card's delivery and
		/// place. Without it no card offers Update draft.
		void set_draft_update_dispatch(std::function<void(std::uint64_t aSession, std::size_t aOrdinal)> aDispatch);
		/// \brief `aNote` is what the action made: the note when `noted`, the
		/// review draft when `drafted` ([[BPR-2325]]).
		void present_finding_card_state(std::uint64_t aSession, std::size_t aOrdinal, agent_finding_card_state aState, const QString& aWhy = { }, std::uint64_t aNote = 0U);
		/// \brief [[BPR-2325]]. The card at `aOrdinal` of the answer whose
		/// findings block is `aFindings`, if it says Drafted, is open again:
		/// its draft was deleted. \returns Whether a card was reopened.
		auto reopen_drafted_finding(const core::agent_findings_digest& aFindings, std::size_t aOrdinal) -> bool;
		/// \brief Owner report 2026-11-10, after BPR-2052. Where each state
		/// `present_finding_card_state` gives a card under an answer with a
		/// findings block is said, so the host keeps it.
		void               set_finding_card_observer(agent_finding_card_observer aObserver) { CardObserver = std::move(aObserver); }
		[[nodiscard]] auto finding_card_state(std::size_t aCard) const -> agent_finding_card_state;
		/// \brief [[BPR-2061]]. Where a reply's Draft all and its Undo go, and
		/// what says whether a suggestion's lines are still in the diff (none:
		/// the anchor the finding arrived with).
		void set_draft_all_dispatch(agent_draft_all_dispatch aDispatch, agent_draft_all_undo aUndo);
		void set_finding_placeability(agent_finding_placeability aPlaceable);
		/// \brief The host found this finding's file or lines not in the pull
		/// request at the head: Draft all no longer counts it.
		void mark_finding_gone(std::uint64_t aSession, std::size_t aOrdinal);
		/// \brief Draft all's button, its sentence and its Undo, on one turn.
		[[nodiscard]] auto turn_draft_all_button(std::size_t aTurn) const -> QPushButton*;
		[[nodiscard]] auto turn_draft_all_state(std::size_t aTurn) const -> QLabel*;
		[[nodiscard]] auto turn_draft_all_undo(std::size_t aTurn) const -> QPushButton*;
		/// \brief Draft all's sentence for one outcome, and for one Undo.
		[[nodiscard]] static auto draft_all_words(const core::draft_all_outcome& aOutcome) -> QString;
		[[nodiscard]] static auto draft_all_undo_words(std::size_t aRemoved, std::size_t aKept) -> QString;
		/// \brief [[BPR-1773]]. Shows the conversation chip (none: removes
		/// it), with conversation prompts in place of the usual ones while it
		/// is shown. Its close button calls `set_conversation_dismiss`'s
		/// callback; nothing else removes it.
		void set_conversation_chip(std::optional<agent_conversation_chip> aChip);
		/// RFC-BPR-0116 ([[BPR-2073]]). What What changed since my review
		/// offers now: a suggestion while offered, nothing otherwise.
		void               present_latest_upload(const core::latest_upload_offer& aOffer);
		[[nodiscard]] auto latest_upload_offer() const noexcept -> const core::latest_upload_offer& { return LatestUpload; }
		/// [[BPR-2251]]. Its suggestion's press: the host reads the change
		/// since the review, then asks through `ask_returning`. Unset, the
		/// press asks at once.
		void set_latest_upload_dispatch(std::function<void()> aDispatch) { LatestUploadDispatch = std::move(aDispatch); }
		/// RFC-BPR-0116. Asks a returning question, as its suggestion does.
		/// \brief `aThreads`: how many threads an `addressed_threads` ask carries.
		void ask_returning(core::returning_operation aOperation, std::size_t aThreads = 0U);
		/// [[BPR-2138]]. Ask more on a returning answer: the host keeps its
		/// question and answer for the next question here.
		void               set_ask_more_dispatch(std::function<void(const QString& aQuestion, const QString& aAnswer)> aDispatch) { AskMoreDispatch = std::move(aDispatch); }
		[[nodiscard]] auto turn_claim(std::size_t aTurn) const -> QLabel*;
		/// \brief [[BPR-2236]]. A report's verdict, above its summary, in its
		/// tone's colour; hidden when the report gives none.
		[[nodiscard]] auto turn_verdict(std::size_t aTurn) const -> QLabel*;
		/// \brief [[BPR-2236]]. The three tones the finding cards and the
		/// verdict wear: the theme's (`adopt_appearance`), or the built-in
		/// Light's or Dark's by the palette until a theme reaches the tab.
		[[nodiscard]] auto tones() const -> tone_colours;
		[[nodiscard]] auto turn_ask_more_button(std::size_t aTurn) const -> QPushButton*;
		/// Whether a turn's question was a returning one, and which.
		[[nodiscard]] auto turn_returning(std::size_t aTurn) const -> std::optional<core::returning_operation>;
		/// \brief RFC-BPR-0117 ([[BPR-2076]]). Asks Explain this file of
		/// `aPath` as a turn here, as a suggestion is sent, without showing the
		/// tab. Answers the refusal's words when nothing was sent, else empty.
		auto ask_explain_file(const QString& aPath) -> QString;
		/// \brief RFC-BPR-0117. Each Explain this file turn as it starts, runs
		/// and ends, or a refusal of one, for the card at the file's header.
		void set_explain_observer(std::function<void(const explain_file_progress&)> aObserver) { ExplainObserver = std::move(aObserver); }
		/// \brief RFC-BPR-0128 ([[BPR-2165]]). A Was this addressed? answer's
		/// claim, by the thread it was asked about and its turn.
		using claim_observer = std::function<void(const std::string& aThread, core::addressed_claim aClaim, std::size_t aTurn)>;
		void set_claim_observer(claim_observer aObserver) { ClaimObserver = std::move(aObserver); }
		/// \brief RFC-BPR-0128 ([[BPR-2166]]). A report card's row: the report
		/// to open as a tab (or go to, when it is open).
		using report_dispatch = std::function<void(const agent_report_document& aReport)>;
		void set_report_dispatch(report_dispatch aDispatch) { ReportDispatch = std::move(aDispatch); }
		/// \brief docs/design/report-card-v4: while a report's tab is active,
		/// its card's row is selected; none clears it.
		void mark_open_report(std::optional<std::uint64_t> aTurn);
		/// \brief [[BPR-2184]] (review N5). Where the turn with stable identity
		/// `aTurn` is in the list now; none once it is gone.
		[[nodiscard]] auto turn_of(std::uint64_t aTurn) const -> std::optional<std::size_t>;
		/// \brief RFC-BPR-0128 with RFC-BPR-0126's queue. Told when the agent
		/// may take more: setup finished, or a turn settled.
		/// `aSettled` is true when a turn has just settled.
		void set_availability_observer(std::function<void(bool aSettled)> aObserver) { AvailabilityObserver = std::move(aObserver); }
		/// \brief Shift+click on a quick ask: its message in the field, sent by
		/// nobody until the reviewer sends it.
		void draft_prompt(const QString& aPrompt);
		/// \brief Whether a question can be sent now (not answering, Send on).
		[[nodiscard]] auto ready_to_send() const -> bool;
		/// \brief Puts `aText` in the field for the reviewer to finish.
		void draft_text(const QString& aText);
		/// \brief What waits its turn in this session's queue.
		void present_queue(const QStringList& aWaiting, const QString& aState);
		/// \brief RFC-BPR-0126 ([[BPR-2151]]). One waiting message as the
		/// pending list shows it ([[BPR-2154]]: the list is shared).
		using queue_row = agent_pending_row;
		/// \brief What the pending list's controls ask of the session's queue.
		using queue_dispatch = agent_pending_dispatch;
		void set_queue_dispatch(queue_dispatch aDispatch);
		/// \brief The session's waiting messages, in order, with the queue's
		/// state ("waiting its turn", "paused: Stop") and whether it is paused.
		void               present_queue_rows(std::vector<queue_row> aRows, const QString& aState, bool aPaused);
		[[nodiscard]] auto queue_list() const noexcept -> QWidget* { return QueueList; }
		/// \brief A waiting card dropped at `aAt` among the waiting cards,
		/// as the pending list's drop does.
		void drop_queued(std::uint64_t aId, int aAt);
		/// \brief RFC-BPR-0126 Q4/Q5 as answered: while a turn runs or messages
		/// wait, a typed message is a follow-up; Queue (the default) or Steer
		/// by the setting, Cmd+Return (Ctrl+Return off macOS) the other for
		/// one message. Returns whether it was taken.
		using follow_up = std::function<bool(const QString& aText, bool aSteer)>;
		void set_follow_up(follow_up aFollowUp) { FollowUp = std::move(aFollowUp); }
		void set_follow_up_steer(bool aSteer);
		/// \brief [[BPR-2181]] (AC05). A message the queue could not keep
		/// comes back to the composer as typed, ahead of anything typed since,
		/// and the refusal says why.
		void return_unqueued(const QString& aText, const QString& aWhy);
		/// \brief RFC-BPR-0126 AC03 ([[BPR-2181]]). What the tab did with a
		/// released message, as its own control flow decided it: never read
		/// from the refusal note, which an earlier send may have left.
		enum class release_outcome : std::uint8_t {
			/// Handed to the send action: whether a turn started is the
			/// runtime's to say.
			sent,
			/// A slash command, answered with no turn.
			answered,
			/// Refused before the send action.
			refused,
		};
		/// \brief A queued typed message released: sent as typed, bypassing
		/// the follow-up.
		[[nodiscard]] auto send_released(const QString& aText) -> release_outcome;
		[[nodiscard]] auto queue_note() const noexcept -> QLabel* { return QueueNote; }
		/// \brief Opens turn `aTurn`'s report, as its card's Open does; at
		/// menu row `aHeading` when it is not 0 ([[BPR-2331]]: a summary row).
		void               open_report(std::size_t aTurn, std::uint32_t aHeading = 0U);
		[[nodiscard]] auto turn_report_card(std::size_t aTurn) const -> QWidget*;
		/// \brief [[BPR-2331]]. The report's summary, over its attachment.
		[[nodiscard]] auto turn_report_tally(std::size_t aTurn) const -> report_tally_view*;
		/// \brief [[BPR-2331]]. Whether turn `aTurn`'s report holds the finding
		/// card `aCard` (an index into `finding_cards()`), so it is not shown.
		[[nodiscard]] auto finding_card_held(std::size_t aCard) const -> bool;
		/// \brief RFC-BPR-0118 ([[BPR-2078]]). One Suggest the change turn:
		/// its index, and once it ends whether it answered, the answer as
		/// written and the ending line; a refused send has no turn.
		struct suggest_change_turn
		{
			std::size_t Turn     = 0U;
			bool        Ended    = false;
			bool        Answered = false;
			QString     AnswerText;
			QString     Ending;
		};
		/// \brief RFC-BPR-0118. Asks Suggest the change on a draft comment's
		/// lines as a turn of its own ("Suggest the change on lines 3–5 of
		/// path"), sent alone; what the reviewer had typed stays.
		/// \returns Why it was not sent, or empty.
		auto ask_suggest_change(const QString& aPath, std::uint32_t aFirst, std::uint32_t aLast) -> QString;
		void set_suggest_change_observer(std::function<void(const suggest_change_turn&)> aObserver) { SuggestObserver = std::move(aObserver); }
		/// \brief RFC-BPR-0118. The file a turn suggested a change in; empty for any other.
		[[nodiscard]] auto turn_suggest_change(std::size_t aTurn) const -> QString;
		/// \brief RFC-BPR-0117. The file a turn explained; empty for any other.
		[[nodiscard]] auto turn_explain(std::size_t aTurn) const -> QString;
		/// \brief RFC-BPR-0117. Continue in Agent tab: brings the turn into view.
		void reveal_turn(std::size_t aTurn);
		/// \brief An answer's reference, followed as the tab's own are.
		void open_reference(const QString& aLink) { follow_link(aLink); }
		/// \brief Sets another surface's answer as the tab's answers are set.
		void               dress_answer_view(markdown_body_view& aView) const { dress_answer(aView); }
		void               set_conversation_dismiss(std::function<void()> aDismiss);
		[[nodiscard]] auto conversation_chip() const noexcept -> QWidget* { return ConversationChip; }
		[[nodiscard]] auto conversation_chip_text() const -> QString;
		[[nodiscard]] auto conversation_chip_close() const noexcept -> QToolButton* { return ConversationClose; }
		/// \brief [[BPR-2002]], RFC-BPR-0102. The selected lines, as the chip
		/// in the question field shows them while they are attached. The
		/// chip's close button detaches them from the questions that follow
		/// and leaves the diff's selection as it is; a new selection, or
		/// `attach_lines`, attaches again.
		[[nodiscard]] auto lines_chip() const noexcept -> QWidget* { return LinesChip; }
		[[nodiscard]] auto lines_chip_text() const -> QString;
		[[nodiscard]] auto lines_chip_close() const noexcept -> QToolButton* { return LinesClose; }
		[[nodiscard]] auto lines_attached() const -> bool;
		/// \brief Attaches the selected lines again (Ask the agent about
		/// these lines).
		void attach_lines();
		/// \brief RFC-BPR-0125 (Q2, AC10). The suggested prompt the question
		/// field holds as a draft, beside the lines chip: sending keeps its
		/// subject and kind. Its close button, or emptying the field, makes
		/// the text a typed question.
		[[nodiscard]] auto macro_chip() const noexcept -> QWidget* { return MacroChip; }
		[[nodiscard]] auto macro_chip_text() const -> QString;
		[[nodiscard]] auto macro_chip_close() const noexcept -> QToolButton* { return MacroClose; }
		/// \brief RFC-BPR-0125; [[BPR-2181]] (AC13, Prompts review B1). Where
		/// the tab's suggested prompts are decided: handed what the tab shows,
		/// it answers the decided offers. The tab assembles no facts and
		/// decides no offer; with no source it offers none.
		using prompt_source = std::function<core::chat_prompts(const core::prompt_chat_view&)>;
		void set_prompt_source(prompt_source aSource);
		/// \brief What the tab shows that its prompts are decided on.
		[[nodiscard]] auto prompt_view() const -> core::prompt_chat_view;
		/// \brief [[BPR-2181]] (AC13). A queued suggested prompt's release:
		/// the offer decided at its press, sent (or put in the field, for one
		/// with `{cursor}`) as it is, never decided again.
		[[nodiscard]] auto send_decided_prompt(const core::prompt_offer& aOffer) -> release_outcome;
		[[nodiscard]] auto suggestion_list() const noexcept -> agent_suggestion_list* { return Suggestions; }
		/// \brief RFC-BPR-0125 (AC07). The name a turn was sent under, shown
		/// above its question; empty when none differs from it.
		[[nodiscard]] auto turn_name(std::size_t aTurn) const -> QString;
		/// \brief Show all, on a question folded past six lines; none when
		/// the question is not folded.
		[[nodiscard]] auto turn_show_all(std::size_t aTurn) const -> QPushButton*;
		/// \brief The question field, whose tooltip says what the agent can
		/// read ([[BPR-2038]]: a tooltip, no longer a line under it).
		[[nodiscard]] auto question_field() const noexcept -> QWidget* { return Field; }
		/// \brief [[BPR-2002]]. The question the last send carried, as the
		/// core decided it; none before any.
		[[nodiscard]] auto last_question() const noexcept -> const std::optional<core::agent_question>& { return LastQuestion; }
		/// \brief [[BPR-1773]], owner look 2026-10-27. Draft comment on every
		/// answer, or its Keep local: where a sent question's answer would
		/// be drafted (asked when it is sent), where a press goes, and where
		/// Go to draft goes once it is drafted.
		void set_answer_draft_target(std::function<agent_answer_draft_target()> aTarget);
		void set_answer_draft_dispatch(agent_answer_draft_dispatch aDispatch);
		void set_draft_navigation(agent_answer_draft_navigation aNavigate);
		/// \brief [[BPR-1862]]. The draft the turn with this key wrote is gone:
		/// its Go to draft becomes Draft comment again, and pressing it makes
		/// a new draft.
		void               release_answer_draft(const std::string& aKey);
		[[nodiscard]] auto turn_draft_button(std::size_t aTurn) const -> QPushButton*;
		/// \brief What a turn was asked about, as the line above its question
		/// names it; empty for a turn about the pull request.
		[[nodiscard]] auto turn_about(std::size_t aTurn) const -> QString;
		/// \brief The turn's asked line as read: "About <conversation>:
		/// <question>" for a turn about a conversation, else the question.
		[[nodiscard]] auto turn_asked(std::size_t aTurn) const -> QString;
		/// \brief Keep local on a plain answer (RFC-BPR-0127).
		void set_answer_note_dispatch(agent_answer_note_dispatch aDispatch);
		/// \brief Owner look v004 ("After save as note it should turn into Go
		/// to note"): where Go to comment goes, the Local comment kept.
		void set_note_navigation(agent_note_navigation aNavigate);
		/// \brief Owner look v006. Where opening the agent menu asks the
		/// agent's harness what it offers, by the agent's profile.
		void set_catalog_dispatch(std::function<void(std::uint64_t aProfile)> aDispatch);
		/// \brief Owner look v006. History's **Reset agents…**: `aConfirm`
		/// asks first (a warning dialog when none is given), then `aReset`
		/// clears the agents' settings and the setup starts again at Agent.
		void set_reset_dispatch(std::function<void()> aReset, std::function<bool()> aConfirm = { });
		/// \brief [[BPR-2377]], owner 2026-11-24. When set, Reset agents… goes
		/// here instead (Clear local data with Agent setup checked, which asks
		/// and resets), and neither asks nor opens the setup itself.
		void set_reset_route(std::function<void()> aRoute) { ResetRoute = std::move(aRoute); }
		void reset_agents();
		/// \brief [[BPR-1998]]. Which files references may name, and where a
		/// link to one goes; references are links only when both are set. A
		/// shared name is offered by `set_reference_chooser`'s chooser, or a
		/// menu.
		void set_line_links(agent_reference_files_source aFiles, agent_reference_navigation aNavigate);
		void set_reference_chooser(agent_reference_chooser aChoose) { ReferenceChooser = std::move(aChoose); }
		/// \brief Owner look v004 ("Not drawing inline comment with syntax
		/// highlighing"): the file name the answers' inline code is
		/// coloured as, such as `code.cpp` for a C++ pull request; empty
		/// leaves it plain.
		void               set_inline_code_file(const QString& aFileName);
		[[nodiscard]] auto turn_question_label(std::size_t aTurn) const -> QLabel*;
		[[nodiscard]] auto turn_answer_view(std::size_t aTurn) const -> markdown_body_view*;
		[[nodiscard]] auto turn_note_button(std::size_t aTurn) const -> QPushButton*;
		/// \brief [[BPR-1847]]. An error turn's one way back (Resend or
		/// Agent settings…), its reply card, and how it ended.
		[[nodiscard]] auto turn_recovery_button(std::size_t aTurn) const -> QPushButton*;
		/// \brief [[BPR-1876]], [[BPR-1880]]. Delete, on every error turn.
		[[nodiscard]] auto turn_delete_button(std::size_t aTurn) const -> QPushButton*;
		[[nodiscard]] auto turn_reply(std::size_t aTurn) const -> QWidget*;
		[[nodiscard]] auto turn_outcome(std::size_t aTurn) const -> std::optional<core::agent_turn_outcome>;
		/// \brief [[BPR-1847]]. What stands in for the suggestions while the
		/// setup is unfinished, and whether the suggestions show.
		[[nodiscard]] auto setup_prompt() const noexcept -> QWidget* { return SetupPrompt; }
		[[nodiscard]] auto suggestions_widget() const noexcept -> QWidget* { return Suggestions; }
		/// \brief The cards a turn brought, under its answer.
		[[nodiscard]] auto turn_cards(std::size_t aTurn) const -> std::vector<QWidget*>;
		[[nodiscard]] auto ask_button() const noexcept -> QPushButton* { return Ask; }
		/// \brief [[BPR-1916]]. The skills `/name` completion offers in the
		/// question field.
		void               present_skills(std::vector<core::skill_entry> aSkills);
		[[nodiscard]] auto skill_completion() const noexcept -> skill_completer* { return SkillCompletion; }
		/// \brief [[BPR-1817]]. Says beside the composer why a send produced
		/// no turn. `aDetail` is the host's own words for a refusal the core
		/// did not name, shown after the composer's sentence.
		void refuse_send(core::agent_send_refusal aRefusal, const QString& aDetail = { });
		/// The refusal shown beside the composer, or `none`.
		[[nodiscard]] auto send_refusal() const noexcept -> core::agent_send_refusal { return ShownRefusal; }
		[[nodiscard]] auto send_refusal_text() const -> QString;
		[[nodiscard]] auto send_refusal_strip() const noexcept -> QWidget* { return Refusal; }
		/// Why Send cannot be pressed now, in the core's vocabulary, or `none`.
		[[nodiscard]] auto send_blocked() const -> core::agent_send_refusal;
		/// \brief The composer's sentence for one refusal, naming `aAgent`.
		[[nodiscard]] static auto send_refusal_words(core::agent_send_refusal aRefusal, const QString& aAgent) -> QString;
		/// \brief [[BPR-1892]]. Brings the stopped run's card into view and
		/// puts the keyboard on its first action; another tab's notice of
		/// the failure sends the person here.
		void reveal_run_failure();
		/// \brief What a run that ended without an answer amounts to, in words.
		[[nodiscard]] static auto run_failure_words(application::agent_run_outcome aOutcome, const QString& aAgent) -> QString;
		/// \brief RFC-BPR-0130. The one header row every chat wears: here the
		/// pull request's title (or the open session's own name) and History.
		[[nodiscard]] auto header() const noexcept -> chat_header_view* { return Header; }
		/// \brief The whole conversation as text, as History's Copy
		/// conversation puts it on the clipboard.
		[[nodiscard]] auto chat_text() const -> QString;

	protected:
		void showEvent(QShowEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		std::function<void(std::optional<std::string>)> NativeHistoryDispatch;
		/// [[BPR-2377]]
		session_edit_dispatch SessionEditDispatch;
		agent_session_menu*   SessionMenu  = nullptr;
		chat_history_menu*    HistoryItems = nullptr;
		void                  refresh_history();
		void                  present_session_notice();
		QString               DefaultSessionTitle;
		void                  show_session_title();
		void                  copy_chat() const;
		/// The revision the open session reads, seven characters; empty
		/// while it is not known.
		[[nodiscard]] auto                           read_revision() const -> QString;
		chat_header_view*                            Header = nullptr;
		application::native_agent_history_projection NativeHistoryView;
		QLabel*                                      SessionNotice     = nullptr;
		QLabel*                                      ConversationMoved = nullptr;
		/// Paints each glyph again in the current palette's ink.
		void repaint_glyphs();
		void rebuild_findings();
		/// One finding card: where it came from, what it says, its state.
		struct card_entry
		{
			std::uint64_t            Id      = 0U;
			std::uint64_t            Session = 0U;
			std::size_t              Ordinal = 0U;
			core::agent_finding      Finding;
			bool                     Anchored = false;
			agent_finding_card_state State    = agent_finding_card_state::open;
			QString                  Why;
			/// The Local comment Keep local made; 0 for none.
			std::uint64_t Note = 0U;
			/// The turn it is under; none for a card no turn brought.
			std::optional<std::size_t> Turn;
			QPointer<QWidget>          Widget;
			/// [[BPR-2061]]. Its claim, evidence and suggested comment as
			/// Markdown, each fitted to the card's width.
			std::vector<QPointer<markdown_body_view>> Texts;
			/// [[BPR-2061]]. Found not at the head when it was drafted.
			bool Gone = false;
			/// [[BPR-2331]]. Its turn's report holds it: the report shows it
			/// and its actions, and the Agent tab does not.
			bool Held = false;
			/// Drafted, and its draft differs from the comment a follow-up
			/// updated since: it offers Update draft.
			bool Outdated = false;
		};
		[[nodiscard]] auto make_card(card_entry& aEntry, QWidget* aParent) -> QWidget*;
		/// [[BPR-2059]]. Sets one of a card's Markdown texts to the height its width needs.
		void                                            fit_card_text(markdown_body_view& aView) const;
		void                                            show_card_state(card_entry& aEntry);
		void                                            show_card_suggestion(card_entry& aEntry, QWidget& aInset);
		agent_finding_card_dispatch                     FollowUpDispatch;
		std::function<void(std::uint64_t, std::size_t)> DraftUpdateDispatch;
		/// RFC-BPR-0127. Shows the card's one keep action, Draft comment or Keep local.
		void          show_card_keep(const card_entry& aEntry) const;
		void          act_on_card(std::uint64_t aCard, agent_finding_card_action aAction);
		std::uint64_t NextCardId = 1U;
		/// The findings that arrived during the turn that just ended go
		/// under its answer.
		void                    claim_findings(std::size_t aTurn);
		std::vector<card_entry> CardEntries;
		/// [[BPR-2331]]. A report turn's cards that its report holds are
		/// hidden, the others shown, and its summary set from the report.
		void place_report_findings(std::size_t aTurn);
		/// The report as its tab shows it, its references links.
		[[nodiscard]] auto report_markdown(std::size_t aTurn) const -> QString;
		/// [[BPR-2219]]. The comments the agent suggested for the turn's
		/// cards, by place, in the order of its cards in `CardEntries`.
		[[nodiscard]] auto report_suggestions(std::size_t aTurn) const -> std::vector<core::report_suggested_comment>;
		/// [[BPR-2331]]. A report turn that ended at the conversation's end
		/// leaves its attachment in view.
		void keep_report_in_view(std::uint64_t aSerial);
		/// \brief Owner report 2026-11-10 ("I added one and after a while I
		/// went back and all the other suggestions were gone"). The cards of
		/// a turn the thread stopped showing, with their states, kept by the
		/// answer they came under.
		/// \details The host shows the thread again whenever it reads the
		/// conversation back (after a turn ends, on another pull request or
		/// agent and back), and nothing it reads carries the cards. A turn
		/// shown again with the same findings block, or failing that the same
		/// answer, gets its cards back as they were: Drafted stays Drafted
		/// and the rest stay to be added.
		struct retained_cards
		{
			/// The answer as shown, trimmed.
			QString Answer;
			/// The answer's findings block as written; empty when it had none.
			std::string FindingsBlock;
			/// Without widgets or turn.
			std::vector<card_entry> Cards;
		};
		/// The most turns whose cards are kept while not shown.
		static constexpr std::size_t maximum_retained_card_turns = 50U;
		std::vector<retained_cards>  RetainedCards;
		/// Moves every card under a turn into `RetainedCards`.
		void retain_turn_cards();
		/// Makes again, under \p aTurn, the cards kept for its answer.
		/// \returns Whether any were kept for it.
		auto restore_turn_cards(std::size_t aTurn) -> bool;
		/// Owner report 2026-11-10, after BPR-2052. Makes, under \p aTurn,
		/// the cards the host read from its stored answer.
		void make_restored_cards(std::size_t aTurn, const std::vector<agent_restored_finding_card>& aCards);
		/// Says one card's state to `CardObserver`, when its answer had a block.
		void                               observe_card_state(const std::string& aFindingsBlock, const card_entry& aEntry) const;
		agent_finding_card_observer        CardObserver;
		std::optional<std::uint64_t>       ClaimedSession;
		agent_finding_card_dispatch        CardDispatch;
		agent_answer_note_dispatch         NoteDispatch;
		agent_note_navigation              NoteNavigate;
		std::function<void(std::uint64_t)> CatalogDispatch;
		std::function<void()>              ResetDispatch;
		std::function<bool()>              ResetConfirm;
		std::function<void()>              ResetRoute;
		agent_reference_files_source       LineResolve;
		agent_reference_navigation         LineNavigate;
		agent_reference_chooser            ReferenceChooser;
		QString                            InlineCodeFile;
		void                               follow_link(const QString& aLink);
		void                               present_banner();
		/// \brief Stands the state card on the question field (true) or at
		/// the head of the thread.
		void dock_banner(bool aDocked);
		/// \brief The title of the card of a session that read \p aRead.
		[[nodiscard]] static auto pinned_card_title(const QString& aRead) -> QString;

		/// RFC-BPR-0130 stage 5 (AC07). What the window last handed the tab,
		/// and where its gestures go.
		agent_tab_run                                      Run;
		std::optional<side_chat_projection>                ChatShown;
		std::optional<application::agent_finding_delivery> FindingsShown;
		agent_tab_intents                                  Intents;
		/// The question field's words, and what Send says while a turn runs.
		QPlainTextEdit* Composer = nullptr;
		QString         SendWords;
		QVBoxLayout*    EveryRun = nullptr;
		/// What the session's chat says, as its own panel said it.
		[[nodiscard]] auto    chat_sendable() const -> bool;
		[[nodiscard]] auto    continuation_required() const -> bool;
		[[nodiscard]] auto    chat_head_text() const -> QString;
		void                  start_review();
		void                  stop_run();
		void                  send_question(const QString& aText);
		void                  choose_continuation(core::agent_continuation aChoice);
		void                  act_on_finding(std::size_t aOrdinal, finding_queue_action aAction);
		std::function<void()> Setup;
		agent_tab_state       State        = agent_tab_state::not_set_up;
		QStackedWidget*       Pages        = nullptr;
		drawer_empty_state*   Empty        = nullptr;
		QWidget*              Conversation = nullptr;
		QWidget*              HistoryPage  = nullptr;
		QWidget*              Banner       = nullptr;
		QLabel*               BannerTitle  = nullptr;
		QLabel*               BannerText   = nullptr;
		quiet_push_button*    BannerFirst  = nullptr;
		quiet_push_button*    BannerSecond = nullptr;
		/// The two columns the card stands in: the thread's, which it heads,
		/// and the question field's, which it tops.
		QVBoxLayout* BannerThread = nullptr;
		QVBoxLayout* BannerDock   = nullptr;
		bool         BannerDocked = false;
		/// RFC-BPR-0096, owner continue-at-latest amendment. The reconcile
		/// choices, on their own wrapping row above the first two.
		QWidget*     BannerReconcile = nullptr;
		QPushButton* BannerRebase    = nullptr;
		QPushButton* BannerSave      = nullptr;
		QPushButton* BannerAsk       = nullptr;
		/// [[BPR-2295]]. Closes a stop's block, as the ready box's close does.
		QToolButton*                               BannerDismiss = nullptr;
		QWidget*                                   Cards         = nullptr;
		QWidget*                                   Viewport      = nullptr;
		QVBoxLayout*                               CardColumn    = nullptr;
		QToolButton*                               History       = nullptr;
		QMenu*                                     HistoryMenu   = nullptr;
		agent_setup_view*                          SetupView     = nullptr;
		std::function<void()>                      FirstShown;
		agent_run_settings_dispatch                RunSettingsDispatch;
		std::shared_ptr<agent_run_settings_source> RunSettingsSource;
		/// [[BPR-1924]]. The field's tool row, and the row the picker wraps to.
		QHBoxLayout* AskRow    = nullptr;
		QHBoxLayout* PickerRow = nullptr;
		QLabel*      Notice    = nullptr;
		/// [[BPR-1817]]. The refusal beside the composer.
		QWidget*                 Refusal      = nullptr;
		QLabel*                  RefusalText  = nullptr;
		core::agent_send_refusal ShownRefusal = core::agent_send_refusal::none;
		std::uint64_t            RefusalsSeen = 0U;
		void                     hide_send_refusal();
		[[nodiscard]] auto       send_facts(bool aMessageEmpty) const -> core::agent_send_facts;
		/// [[BPR-1823]]. The agent's name as the composer says it: the menu's,
		/// or the chosen agent's that is no longer offered.
		[[nodiscard]] auto agent_name() const -> QString;
		/// [[BPR-1823]]. The pill when the chosen agent is unavailable.
		[[nodiscard]] auto current_run_settings() const -> const agent_run_settings*;
		void               present_run_settings_button();
		void               ask();
		/// One question and its answer.
		struct turn
		{
			QString Question;
			/// RFC-BPR-0125. The suggested prompt's name, when it differs.
			QString               Name;
			QPointer<QPushButton> ShowAll;
			bool                  Unfolded = false;
			/// [[BPR-2237]]. The prompt's text was changed before sending.
			bool Edited = false;
			bool Review = false;
			bool Done   = false;
			/// RFC-BPR-0130 stage 5. The turn is drawn by the one transcript
			/// (`chat_transcript_view`, the card look) from
			/// `core::project_agent_thread`: its bubble, reader, ending line,
			/// activity, Copy, recovery and Delete are the view's. What the
			/// reader shows (the answer with its references as links, or a
			/// report's summary), under which identity, and whether that was
			/// empty.
			QString     Body;
			std::string BodyKey;
			bool        BodyEmpty = true;
			bool        AnswerSet = false;
			/// The answer as written, shown as Markdown.
			QString AnswerText;
			/// The findings block's JSON as the agent wrote it, when the
			/// answer ended with a complete one: what its cards came from.
			std::string FindingsBlock;
			/// Its ending line as it was said, and the line's tooltip.
			QString Line;
			QString LineTip;
			/// [[BPR-2005]]. What its run did, where this window saw it end.
			std::optional<core::chat_activity> Ran;
			QPointer<QPushButton>              SaveNote;
			/// The attachment its finding cards stand in.
			QPointer<QWidget> Findings;
			QPointer<QLabel>  AskedTime;
			QPointer<QLabel>  AnsweredTime;
			QDateTime         AskedAt;
			QDateTime         AnsweredAt;
			/// Names the answer's body, so its code is coloured as its own; the
			/// turn's stable identity, which its report is keyed by.
			std::uint64_t Serial = 0U;
			/// [[BPR-2184]] (review N5). How it ended, as its footer first says
			/// it: its report's byline, as data.
			QString Ending;
			/// The Local comment Keep local made; 0 for none.
			std::uint64_t    Note = 0U;
			QPointer<QLabel> QuestionLabel;
			/// [[BPR-2153]]. The reviewer's steered messages and where they show.
			std::vector<application::agent_steered_message> Steered;
			/// The one file the question names, which a bare "line 34" in the
			/// answer is in.
			QString ContextFile;
			/// [[BPR-1773]]. The conversation it was asked about, if any.
			std::optional<agent_conversation_chip> About;
			/// Where Draft comment writes the answer; none: the host decides.
			std::optional<agent_answer_draft_target> DraftTarget;
			QPointer<QPushButton>                    Draft;
			bool                                     Drafted = false;
			/// RFC-BPR-0127, owner look v006. Whether the answer offers its
			/// one keep action, Draft.
			bool KeepOffered = false;
			/// RFC-BPR-0116. A returning question's fresh first answer: the
			/// claim it leads with, and Ask more.
			std::optional<core::returning_operation> Returning;
			/// RFC-BPR-0117. The file Explain this file asked about; empty for
			/// every other turn.
			QString Explain;
			/// RFC-BPR-0118. The file Suggest the change asked about; empty
			/// for every other turn.
			QString          SuggestChange;
			QPointer<QLabel> Claim;
			/// [[BPR-2236]]. A report's verdict and its tone.
			QPointer<QLabel>  Verdict;
			std::string       VerdictWord;
			core::report_tone VerdictTone = core::report_tone::neutral;
			/// RFC-BPR-0128 ([[BPR-2166]]). A long answer is a report card
			/// here, opened as a file tab: what its row names it, its
			/// sections and the summary's counts.
			QString                          ReportTitleText;
			std::size_t                      ReportSections = 0U;
			std::vector<core::dressed_count> ReportTally;
			QPointer<QWidget>                ReportCard;
			QPointer<QLabel>                 ReportTitle;
			QPointer<QLabel>                 ReportMeta;
			QPointer<QPushButton>            ReportOpen;
			/// [[BPR-2331]]. The report's summary over its attachment, and
			/// whether the attachment shows.
			QPointer<report_tally_view> Tally;
			bool                        ReportShown = false;
			QPointer<QPushButton>       AskMore;
			QPointer<QWidget>           Asked;
			/// [[BPR-1847]]. How it ended, and the one way back an error
			/// offers, as the projector decided it.
			core::agent_turn_outcome  Outcome      = core::agent_turn_outcome::answered;
			core::agent_turn_recovery RecoveryKind = core::agent_turn_recovery::none;
			/// [[BPR-1876]]. The check its cause names was seen not passed
			/// (running, failed or not run) after it ended, so a pass now is
			/// one made since; and the line that says it is fixed since.
			bool             CauseRechecked = false;
			QPointer<QLabel> FixedNote;
			/// RFC-BPR-0096, owner continue-at-latest amendment: "maybe drop a
			/// notice in the transcript that it happened".
			QPointer<QLabel> MoveNotice;
			/// [[BPR-2061]]. Draft all over its cards, what the last press
			/// did, and the Undo of that batch.
			QPointer<QWidget>          DraftAllRow;
			QPointer<QPushButton>      DraftAll;
			QPointer<QLabel>           DraftAllState;
			QPointer<QPushButton>      DraftAllUndo;
			std::uint64_t              DraftAllBatch = 0U;
			bool                       DraftAllBusy  = false;
			std::vector<std::uint64_t> DraftAllCards;
		};
		std::vector<turn> Turns;
		/// [[BPR-2061]].
		void                       refresh_draft_all(std::size_t aTurn);
		void                       draft_all(std::size_t aTurn);
		void                       undo_draft_all(std::size_t aTurn);
		[[nodiscard]] auto         turn_of_widget(const QObject* aWidget) const -> std::optional<std::size_t>;
		agent_draft_all_dispatch   DraftAllDispatch;
		agent_draft_all_undo       DraftAllUndo;
		agent_finding_placeability FindingPlaceable;
		std::uint64_t              NextDraftAllBatch = 1U;
		/// [[BPR-2005]]. How the turns this window saw end: their line and
		/// what they did, so a thread drawn again from its saved history
		/// keeps them. Not saved with the history; the last 32.
		struct ended_turn
		{
			QString Question;
			/// What the turn said when it ended without an answer: the text
			/// that arrived and how it ended, which its history does not keep.
			QString                   Ending;
			QString                   Footer;
			core::agent_turn_activity Activity;
			qint64                    Seconds = 0;
		};
		std::vector<ended_turn> EndedTurns;
		/// The question and the answer as shown, their references links.
		void show_question(turn& aTurn);
		void show_answer(turn& aTurn);
		/// RFC-BPR-0127, owner look v006. Shows the answer's Draft, struck
		/// where nothing takes a draft, or Go to draft once drafted.
		void present_keep(turn& aTurn);
		/// RFC-BPR-0130 stage 5. The conversation, drawn by the one
		/// transcript in the card look.
		chat_transcript_view* Transcript = nullptr;
		/// The turns as the core's projector reads them.
		[[nodiscard]] auto entries() const -> std::vector<core::agent_thread_entry>;
		/// Draws the thread as it stands: only the turns that changed.
		void present_transcript();
		/// The turn a transcript identity names, and its place.
		[[nodiscard]] auto record_of(const std::string& aId) -> turn*;
		[[nodiscard]] auto place_of(const turn& aTurn) const -> std::size_t;
		/// The reader the transcript shows a turn's answer in.
		[[nodiscard]] auto reader_of(const turn& aTurn) const -> markdown_body_view*;
		/// The Agent tab's own parts of a turn, made with it by the
		/// transcript: the question's words, what it was about and the prompt
		/// it was sent from; the notes over the answer; the keep action and
		/// Ask more.
		void dress_question(turn& aTurn, QWidget& aBubble, QVBoxLayout& aColumn, QLabel& aQuestion);
		void dress_card(turn& aTurn, const chat_transcript_view::card_slots& aSlots);
		void dress_foot(turn& aTurn, QWidget& aCard, QHBoxLayout& aActions);
		/// RFC-BPR-0130 stage 5. What the tab hangs on a turn, as the
		/// projector says it holds it (the verdict over the reply; the
		/// report's row, Draft all and the finding cards under it), made by
		/// the transcript when it does. The cards are made from their records,
		/// as they stand, each time the turn's cards change.
		[[nodiscard]] auto make_attachment(turn& aTurn, const core::chat_attachment& aAttachment, QWidget* aParent) -> QWidget*;
		[[nodiscard]] auto make_report_row(turn& aTurn, const core::chat_agent_report& aReport, QWidget* aParent) -> QWidget*;
		[[nodiscard]] auto make_draft_all(turn& aTurn, QWidget* aParent) -> QWidget*;
		[[nodiscard]] auto make_turn_cards(turn& aTurn, const core::chat_agent_findings& aFindings, QWidget* aParent) -> QWidget*;
		/// The turn's card holder shows while any of its cards does: hidden
		/// cards alone would still take the card's spacing.
		static void show_turn_cards(const turn& aTurn);
		/// The report the tab shows open, whose row reads selected.
		std::optional<std::uint64_t> OpenReport;
		void                         act_on_turn(const std::string& aId, chat_turn_gesture aGesture);
		void                         set_answer(turn& aTurn, const QString& aText);
		void                         settle_reply_footer(turn& aTurn);
		/// [[BPR-1847]]. Each ended turn's actions, as the core decides them.
		void present_turn_actions();
		void recover(const turn& aTurn);
		/// Sends a turn's message again, as it was asked.
		void resend(const turn& aTurn);
		/// [[BPR-1876]]. Removes a turn whose message never reached the agent
		/// from the thread, and keeps the thread without it.
		void delete_turn(std::size_t aTurn);
		/// [[BPR-1847]]. Why the suggestions are not shown now, if not.
		[[nodiscard]] auto                 suggestions_verdict() const -> core::agent_suggestions_verdict;
		void                               refresh_times();
		void                               dress_answer(markdown_body_view& aAnswer) const;
		QLabel*                            SessionTitle = nullptr;
		QWidget*                           EmptyIntro   = nullptr;
		chat_composer*                     Field        = nullptr;
		std::optional<QFont>               CodeFont;
		std::optional<markdown_code_theme> CodeTheme;
		/// [[BPR-2236]]. The theme's three tones, once one reached the tab.
		std::optional<tone_colours> Tones;
		/// [[BPR-2236]]. Draws a turn's verdict, and a card's severity and
		/// label, in the current tones.
		void                                                                        show_verdict(const turn& aTurn) const;
		void                                                                        colour_card(const card_entry& aEntry) const;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		std::uint64_t                                                               NextSerial = 1U;
		std::function<void(std::vector<agent_thread_turn>)>                         ThreadObserver;
		std::function<void()>                                                       ClearDispatch;
		std::function<bool(core::agent_continuation)>                               PinnedDispatch;
		std::function<bool(core::conversation_reconcile)>                           ReconcileDispatch;
		/// Takes one pinned conversation choice: Continue resends the kept
		/// message; New session keeps it for the session the host opens.
		void choose_pinned(core::agent_continuation aChoice);
		void choose_reconcile(core::conversation_reconcile aChoice);
		/// What the stopped card's New session keeps while the host opens the
		/// new session: the refused turn's message, or that it was a review.
		struct kept_message
		{
			QString Question;
			bool    Review = false;
		};
		std::optional<kept_message> KeptForNewSession;
		/// A choice on the card is being taken: its own resend is not held
		/// back as a question typed while the card waits is.
		bool ChoosingRevision = false;
		/// \brief Whether the card asks which revision to go on with: before
		/// a question (`head_moved`) or after a refused one. While it does a
		/// typed question is refused in place and the suggested prompts wait.
		[[nodiscard]] auto revision_choice_waits() const -> bool;
		/// \brief What to choose first, as the greyed prompts say it.
		[[nodiscard]] auto revision_choice_words() const -> QString;
		/// The log as tall as its lines, up to `stopped_log_lines`.
		void                                      fit_log();
		std::function<bool(std::size_t)>          ClearConfirm;
		std::function<agent_suggestion_context()> SuggestionContext;
		/// [[BPR-1934]]. The suggestion component the tour chats share.
		agent_suggestion_list* Suggestions     = nullptr;
		quiet_tool_button*     SuggestionsMenu = nullptr;
		/// [[BPR-1847]]. In place of the suggestions while setup is unfinished.
		QWidget* SetupPrompt = nullptr;
		/// [[BPR-1881]]. The readiness last told, and who is told.
		std::optional<agent_setup_readiness> ToldReadiness;
		std::function<void()>                ReadinessObserver;
		std::function<void(bool)>            SuggestionsObserver;
		/// What the next admitted request is: a question typed, or a review.
		std::optional<QString> PendingQuestion;
		bool                   PendingReview = false;
		/// [[BPR-1872]]. A suggested prompt was sent past the reviewer's own
		/// words, which stay in the field when its turn begins.
		bool PendingKeepsField = false;
		bool KeepingDraft      = false;
		/// [[BPR-1773]]. The conversation chip shown when it was sent.
		std::optional<agent_conversation_chip>   PendingAbout;
		std::optional<agent_answer_draft_target> PendingDraftTarget;
		/// RFC-BPR-0130 stage 5. The running turn as the core folds what the
		/// run is seen doing (`core::reduce_agent_live_turn`): its phase as
		/// last seen when the run reported progress, which says when a turn
		/// begins and ends.
		core::agent_live_turn Live;
		/// What the run is seen doing now, as the core reads it.
		[[nodiscard]] auto observe_run() const -> core::agent_live_observation;
		/// How many times findings were presented, and the count when the
		/// running turn began: a turn names only the findings it brought.
		std::uint64_t FindingsPresented = 0U;
		std::uint64_t FindingsAtTurn    = 0U;
		void          begin_turn();
		void          update_turn();
		void          progress();
		QTimer*       Ticker = nullptr;
		/// [[BPR-1869]]. Output lines re-render the running answer at most
		/// once per interval: the first at once, the last when it ends. Each
		/// render is the whole answer, so one per line was quadratic.
		QTimer*       AnswerPacer   = nullptr;
		bool          AnswerPending = false;
		QElapsedTimer Started;
		/// The last request ended well and nothing has run since.
		bool               Finished        = false;
		QWidget*           ReadyCard       = nullptr;
		QLabel*            ReadyTitle      = nullptr;
		QLabel*            ReadyText       = nullptr;
		QLabel*            ReadyGets       = nullptr;
		QPushButton*       Ask             = nullptr;
		skill_completer*   SkillCompletion = nullptr;
		quiet_tool_button* Context         = nullptr;
		QLabel*            Chips           = nullptr;
		/// [[BPR-1773]]. The conversation chip and what it is now.
		QWidget*                               ConversationChip  = nullptr;
		QLabel*                                ConversationText  = nullptr;
		quiet_tool_button*                     ConversationClose = nullptr;
		std::optional<agent_conversation_chip> ConversationShown;
		/// [[BPR-2002]]. The lines chip, the lines it last showed, and
		/// whether the reviewer detached them.
		QWidget*     LinesChip  = nullptr;
		QLabel*      LinesText  = nullptr;
		QToolButton* LinesClose = nullptr;
		/// [[BPR-1999]]. What the open session has cost, for the footer.
		std::optional<core::agent_cost>                      SessionTotal;
		agent_tab_run_observer                               RunObserver;
		std::function<void(const QString&)>                  EndedObserver;
		std::function<void(std::optional<core::agent_cost>)> CostObserver;
		/// [[BPR-1999]]. An input method is composing in the question field.
		[[nodiscard]] auto                                                         send_tip() const -> QString;
		std::optional<std::pair<QString, std::pair<std::uint32_t, std::uint32_t>>> LinesSeen;
		bool                                                                       LinesDetached = false;
		/// RFC-BPR-0125. The macros offered from, and the core's offers shown
		/// now, chips then More.
		prompt_source                   PromptSource;
		std::vector<core::prompt_offer> Offered;
		/// What a sent or drafted suggested prompt carries: its name, its
		/// subject and kind for `decide_agent_question`, and, for a direct
		/// send, the only text skill references are read from.
		struct macro_send
		{
			QString                                      Key;
			QString                                      Name;
			std::optional<core::agent_suggested_subject> Subject;
			bool                                         Review = false;
			std::optional<std::string>                   SkillText;
			/// [[BPR-2237]]. The text the prompt put in the field.
			QString Expansion;
		};
		/// The suggestion being sent, while it is.
		std::optional<macro_send> SendingMacro;
		/// (Q2) The suggestion the field holds as a draft, and its chip.
		std::optional<macro_send> DraftedMacro;
		QWidget*                  MacroChip  = nullptr;
		QLabel*                   MacroText  = nullptr;
		QToolButton*              MacroClose = nullptr;
		void                      present_macro_chip();
		[[nodiscard]] auto        offered(const QString& aKeyOrLabel) const -> const core::prompt_offer*;
		/// The name the next admitted turn was sent under.
		QString PendingName;
		bool    PendingEdited = false;
		/// RFC-BPR-0116. The returning prompts by their keys, the one being
		/// sent, the one a sent question was, and what the host offers.
		std::map<QString, core::returning_operation> ReturningPrompts;
		claim_observer                               ClaimObserver;
		report_dispatch                              ReportDispatch;
		std::function<void(bool)>                    AvailabilityObserver;
		QLabel*                                      QueueNote = nullptr;
		agent_pending_list*                          QueueList = nullptr;
		follow_up                                    FollowUp;
		bool                                         FollowUpSteer = false;
		bool                                         QueueOccupied = false;
		bool                                         Opposite      = false;
		bool                                         Releasing     = false;
		release_outcome                              Released      = release_outcome::refused;
		void                                         refresh_send_label();
		/// \brief RFC-BPR-0130 stage 2. Send and Stop, as `core::decide_chat_send` decides.
		void                                     present_send();
		std::optional<core::returning_operation> SendingReturning;
		std::optional<core::returning_operation> PendingReturning;
		/// RFC-BPR-0117. The file Explain this file is being sent for, the
		/// one a sent question was for, and who is told of its turns.
		QString                                           SendingExplain;
		QString                                           PendingExplain;
		std::function<void(const explain_file_progress&)> ExplainObserver;
		/// RFC-BPR-0118. The file Suggest the change is being sent for, the
		/// one the next turn takes, and who hears of it.
		QString                                             SendingSuggest;
		QString                                             PendingSuggest;
		std::function<void(const suggest_change_turn&)>     SuggestObserver;
		void                                                publish_explain(std::size_t aTurn, std::optional<agent_run_status> aRunning, const QString& aEnding, bool aAnswered, qint64 aSeconds);
		core::latest_upload_offer                           LatestUpload;
		std::function<void()>                               LatestUploadDispatch;
		std::function<void(const QString&, const QString&)> AskMoreDispatch;
		std::optional<core::agent_question>                 LastQuestion;
		[[nodiscard]] auto                                  current_suggestion_context() const -> agent_suggestion_context;
		void                                                present_lines_chip(const agent_suggestion_context& aContext);
		std::function<void()>                               ConversationDismiss;
		std::function<agent_answer_draft_target()>          DraftTargetSource;
		agent_answer_draft_dispatch                         DraftDispatch;
		agent_answer_draft_navigation                       DraftNavigate;
		QPlainTextEdit*                                     Log = nullptr;
		/// The log and the room over it, shown and hidden as one.
		QWidget* LogBox   = nullptr;
		bool     LogShown = false;
		/// [[BPR-1876]]. The latest turn says its cause is fixed since, so
		/// the stopped banner stands down.
		bool LatestFixedSince = false;
		/// [[BPR-1876]]. The stopped run's turn was deleted, so its banner
		/// stands down until the next run.
		bool               StoppedDismissed = false;
		bool               Dying            = false;
		[[nodiscard]] auto alive() const noexcept -> bool { return !Dying; }
	};
} // namespace bpr::qt

#endif
