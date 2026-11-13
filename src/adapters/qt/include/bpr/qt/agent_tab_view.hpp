#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP

#include <QDateTime>
#include <QElapsedTimer>
#include <QFont>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_draft_all.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_reference_links.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/explain_file_card.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

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
	class agent_launch_panel;
	class agent_setup_view;
	class drawer_empty_state;
	class finding_queue_view;
	class markdown_body_view;
	class appearance_profile;
	class side_chat_panel;

	/// \brief What a card says once it was acted on.
	enum class agent_finding_card_state : std::uint8_t {
		open,
		working,
		drafted,
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
		QString Answer;
		QString Footer;
		/// When it was asked and answered; invalid when not known.
		QDateTime AskedAt;
		QDateTime AnsweredAt;
		/// [[BPR-1847]]. How it ended: an answer, or an error that offers
		/// neither Save as note nor Draft comment.
		core::agent_turn_outcome Outcome = core::agent_turn_outcome::answered;
		/// The cards its stored answer's findings block makes, when the host
		/// read them; a turn whose cards this view still holds keeps those.
		std::vector<agent_restored_finding_card> Cards;

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
	};

	/// \brief [[BPR-1753]], owner decision 2026-10-23 ("Yep build it
	/// like that"). What a reviewer may do with one finding card; nothing is
	/// done by itself.
	enum class agent_finding_card_action : std::uint8_t {
		/// A private line draft at the finding's lines, from its suggested
		/// comment (or its claim).
		draft_comment,
		/// A private note on its lines, never published.
		save_note,
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
	/// \brief Save as note on a plain answer: the text (the whole reply, or
	/// the selected part), and where the host says how it went: the note it
	/// made, which Go to note opens (owner look v004), when it was saved.
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
		/// The conversation takes replies (a review thread, not a note): its
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

		auto operator==(const agent_answer_draft_target&) const -> bool = default;
	};
	/// \brief Draft comment on an answer: the place's key (empty for a turn
	/// whose place was not decided, which the host drafts on the pull
	/// request), the text (the whole answer, or the part selected), and
	/// where the host says how it went. Go to draft opens the key's draft.
	using agent_answer_draft_done       = std::function<void(bool aDrafted, const QString& aWhy)>;
	using agent_answer_draft_dispatch   = std::function<void(const std::string& aKey, const QString& aText, agent_answer_draft_done aDone)>;
	using agent_answer_draft_navigation = std::function<void(const std::string& aKey)>;

	/// \brief Opens a private note the tab saved, in the Notes area.
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
		/// The session read an earlier head: Start a new session, or Continue
		/// on the old head.
		head_moved,
		/// The last invocation did not finish: what happened, the question
		/// kept, Resend and Show the log.
		stopped,
		/// [[BPR-1753]]. The setup is in place of the conversation.
		setting_up,
	};

	/// \brief [[BPR-1594]]. RFC-BPR-0085 D5: the Agent tab, the right drawer's
	/// fourth. A context row (the agent, the head it reads, its scope, and
	/// History), the conversation with its finding cards, and the question
	/// field with its context chips. It presents the existing agent panels'
	/// controls, moved into this layout; every gesture is theirs, so the
	/// agent's authority and every intent are unchanged.
	class agent_tab_view final : public QWidget
	{
	public:
		agent_tab_view(agent_launch_panel& aLaunch, side_chat_panel& aChat, finding_queue_view& aFindings, QWidget& aHistory, QWidget* aParent = nullptr);
		/// \brief Gives the borrowed controls back to the panels that own them.
		~agent_tab_view() override;
		agent_tab_view(const agent_tab_view&)                    = delete;
		auto operator=(const agent_tab_view&) -> agent_tab_view& = delete;

		/// \brief Set up an agent: Settings › Agents & context.
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
		void               present_run_settings(std::vector<agent_run_settings> aSettings);
		void               set_run_settings_dispatch(agent_run_settings_dispatch aDispatch);
		[[nodiscard]] auto run_settings_button() const noexcept -> QToolButton* { return RunSettings; }
		/// \brief [[BPR-2051]]. The window's footer holds the one picker; the
		/// tab opens it for a bare /model or /effort and reads its words.
		void adopt_run_settings_button(agent_run_settings_picker* aPicker);
		/// \brief [[BPR-1924]]. The picker has its own row under the field's
		/// tools, because the one row cannot hold it whole.
		[[nodiscard]] auto run_settings_wrapped() const noexcept -> bool { return RunSettingsWrapped; }
		/// \brief [[BPR-1878]]. What the agent picker shows, for another
		/// picker (the Tour tab's) that is the same choice.
		[[nodiscard]] auto run_settings_source() const noexcept -> const std::shared_ptr<agent_run_settings_source>& { return RunSettingsSource; }
		/// \brief `/model NAME`, `/effort LEVEL` (or `default`), and `/model`
		/// or `/effort` alone to open the control. Answers whether the text
		/// was such a command, which is then not sent to the agent.
		auto               run_slash_command(const QString& aText) -> bool;
		[[nodiscard]] auto run_settings_text() const -> QString;
		/// \brief Owner look v003. The session's title in the header (the
		/// pull request's), a reply's Copy and its time, and the answers' code
		/// coloured as comments' code is (the shared Markdown code host).
		void               set_session_title(const QString& aTitle);
		[[nodiscard]] auto session_title() const noexcept -> QLabel* { return SessionTitle; }
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
		[[nodiscard]] auto conversation_thread() const noexcept -> QWidget* { return Thread; }
		[[nodiscard]] auto turn_count() const noexcept -> std::size_t { return Turns.size(); }
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
		/// with its history (none: not known), under the composer as "$2.37
		/// this session". A new thread starts with none; each turn that
		/// reports a cost adds to it.
		void present_session_cost(std::optional<core::agent_cost> aTotal);
		/// \brief [[BPR-2051]]. The window's footer hears the tab's run as it
		/// moves, how it ended, and the session's total.
		void               set_footer_observers(agent_tab_run_observer aRun, std::function<void(const QString&)> aEnded, std::function<void(std::optional<core::agent_cost>)> aCost);
		[[nodiscard]] auto session_cost() const noexcept -> std::optional<core::agent_cost> { return SessionTotal; }
		[[nodiscard]] auto session_cost_line() const noexcept -> QLabel* { return SessionCostLine; }
		void               set_thread_observer(std::function<void(std::vector<agent_thread_turn>)> aObserver);
		[[nodiscard]] auto thread_turns() const -> std::vector<agent_thread_turn>;
		/// \brief Clear session: asks `set_clear_confirmation`'s question when
		/// the thread has turns (they leave it), then empties the thread and
		/// tells the host, which starts the next question afresh.
		void               set_clear_session_dispatch(std::function<void()> aDispatch);
		void               set_clear_confirmation(std::function<bool(std::size_t aTurns)> aConfirm);
		void               clear_session();
		void               present_native_history(const application::native_agent_history_projection&);
		void               set_native_history_dispatch(std::function<void(std::optional<std::string>)> aDispatch) { NativeHistoryDispatch = std::move(aDispatch); }
		[[nodiscard]] auto clear_session_button() const noexcept -> QToolButton* { return ClearSession; }
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
		[[nodiscard]] auto finding_cards() const -> std::vector<QWidget*>;
		/// \brief [[BPR-1753]], owner decision 2026-10-23. Where a
		/// card's Add as draft comment, Save as note and Dismiss go, and the
		/// state the host reports back for the card (`aWhy` with a refusal).
		void set_finding_card_dispatch(agent_finding_card_dispatch aDispatch);
		void present_finding_card_state(std::uint64_t aSession, std::size_t aOrdinal, agent_finding_card_state aState, const QString& aWhy = { }, std::uint64_t aNote = 0U);
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
		/// offers now: a suggestion while offered, a quiet line saying why
		/// while unavailable, nothing otherwise.
		void               present_latest_upload(const core::latest_upload_offer& aOffer);
		[[nodiscard]] auto latest_upload_offer() const noexcept -> const core::latest_upload_offer& { return LatestUpload; }
		[[nodiscard]] auto latest_upload_note() const noexcept -> QLabel* { return LatestUploadNote; }
		/// RFC-BPR-0116. Asks a returning question, as its suggestion does.
		void ask_returning(core::returning_operation aOperation);
		/// [[BPR-2138]]. Ask more on a returning answer: the host keeps its
		/// question and answer for the next question here.
		void               set_ask_more_dispatch(std::function<void(const QString& aQuestion, const QString& aAnswer)> aDispatch) { AskMoreDispatch = std::move(aDispatch); }
		[[nodiscard]] auto turn_claim(std::size_t aTurn) const -> QLabel*;
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
		/// \brief The question field, whose tooltip says what the agent can
		/// read ([[BPR-2038]]: a tooltip, no longer a line under it).
		[[nodiscard]] auto question_field() const noexcept -> QWidget* { return Field; }
		/// \brief [[BPR-2002]]. The question the last send carried, as the
		/// core decided it; none before any.
		[[nodiscard]] auto last_question() const noexcept -> const std::optional<core::agent_question>& { return LastQuestion; }
		/// \brief [[BPR-1773]], owner look 2026-10-27. Draft comment on every
		/// answer, beside Save as note: where a sent question's answer would
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
		/// \brief Save as note on a plain answer.
		void set_answer_note_dispatch(agent_answer_note_dispatch aDispatch);
		/// \brief Owner look v004 ("After save as note it should turn into Go
		/// to note"): where Go to note goes.
		void set_note_navigation(agent_note_navigation aNavigate);
		/// \brief Owner look v006. Where opening the agent menu asks the
		/// agent's harness what it offers, by the agent's profile.
		void set_catalog_dispatch(std::function<void(std::uint64_t aProfile)> aDispatch);
		/// \brief Owner look v006. History's **Reset agents…**: `aConfirm`
		/// asks first (a warning dialog when none is given), then `aReset`
		/// clears the agents' settings and the setup starts again at Agent.
		void set_reset_dispatch(std::function<void()> aReset, std::function<bool()> aConfirm = { });
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
		[[nodiscard]] auto        head_chip() const noexcept -> QLabel* { return Head; }

	protected:
		void showEvent(QShowEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		std::function<void(std::optional<std::string>)> NativeHistoryDispatch;
		application::native_agent_history_projection    NativeHistoryView;
		QMenu*                                          NativeHistoryMenu = nullptr;
		QLabel*                                         ContinuationMode  = nullptr;
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
			/// The note Save as note made; 0 for none.
			std::uint64_t Note = 0U;
			/// The turn it is under; none for a card no turn brought.
			std::optional<std::size_t> Turn;
			QPointer<QWidget>          Widget;
			/// [[BPR-2061]]. Its claim, evidence and suggested comment as
			/// Markdown, each fitted to the card's width.
			std::vector<QPointer<markdown_body_view>> Texts;
			/// [[BPR-2061]]. Found not at the head when it was drafted.
			bool Gone = false;
		};
		[[nodiscard]] auto make_card(card_entry& aEntry, QWidget* aParent) -> QWidget*;
		/// [[BPR-2059]]. Sets one of a card's Markdown texts to the height its width needs.
		void          fit_card_text(markdown_body_view& aView) const;
		void          show_card_state(card_entry& aEntry);
		void          act_on_card(std::uint64_t aCard, agent_finding_card_action aAction);
		std::uint64_t NextCardId = 1U;
		/// The findings that arrived during the turn that just ended go
		/// under its answer.
		void                    claim_findings(std::size_t aTurn);
		std::vector<card_entry> CardEntries;
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
		agent_reference_files_source       LineResolve;
		agent_reference_navigation         LineNavigate;
		agent_reference_chooser            ReferenceChooser;
		QString                            InlineCodeFile;
		void                               follow_link(const QString& aLink);
		void                               present_banner();

		agent_launch_panel&                        Launch;
		side_chat_panel&                           Chat;
		finding_queue_view&                        Findings;
		QWidget&                                   HistoryPanel;
		std::function<void()>                      Setup;
		agent_tab_state                            State        = agent_tab_state::not_set_up;
		QStackedWidget*                            Pages        = nullptr;
		drawer_empty_state*                        Empty        = nullptr;
		QWidget*                                   Conversation = nullptr;
		QWidget*                                   HistoryPage  = nullptr;
		QWidget*                                   Banner       = nullptr;
		QLabel*                                    BannerTitle  = nullptr;
		QLabel*                                    BannerText   = nullptr;
		QPushButton*                               BannerFirst  = nullptr;
		QPushButton*                               BannerSecond = nullptr;
		QWidget*                                   Cards        = nullptr;
		QWidget*                                   Viewport     = nullptr;
		QVBoxLayout*                               CardColumn   = nullptr;
		QLabel*                                    Head         = nullptr;
		QToolButton*                               History      = nullptr;
		QMenu*                                     HistoryMenu  = nullptr;
		agent_setup_view*                          SetupView    = nullptr;
		std::function<void()>                      FirstShown;
		agent_run_settings_dispatch                RunSettingsDispatch;
		std::shared_ptr<agent_run_settings_source> RunSettingsSource;
		agent_run_settings_picker*                 RunSettings = nullptr;
		/// [[BPR-1924]]. The field's tool row, and the row the picker wraps to.
		QHBoxLayout* AskRow             = nullptr;
		QHBoxLayout* PickerRow          = nullptr;
		bool         RunSettingsWrapped = false;
		QLabel*      Notice             = nullptr;
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
			QString           Question;
			bool              Review = false;
			bool              Done   = false;
			QPointer<QWidget> Widget;
			/// The answer as written, shown as Markdown.
			QPointer<markdown_body_view> Answer;
			QString                      AnswerText;
			/// The findings block's JSON as the agent wrote it, when the
			/// answer ended with a complete one: what its cards came from.
			std::string                     FindingsBlock;
			QPointer<agent_run_status_line> Footer;
			/// [[BPR-2005]]. Its helpers and the quiet notice while it runs;
			/// the folded line after.
			QPointer<agent_activity_view> Activity;
			QPointer<QPushButton>         SaveNote;
			QPointer<QVBoxLayout>         CardColumn;
			QPointer<QToolButton>         Copy;
			QPointer<QLabel>              AskedTime;
			QPointer<QLabel>              AnsweredTime;
			QDateTime                     AskedAt;
			QDateTime                     AnsweredAt;
			/// Names the answer's body, so its code is coloured as its own.
			std::uint64_t Serial = 0U;
			/// The note Save as note made; 0 for none.
			std::uint64_t    Note = 0U;
			QPointer<QLabel> QuestionLabel;
			/// The one file the question names, which a bare "line 34" in the
			/// answer is in.
			QString ContextFile;
			/// [[BPR-1773]]. The conversation it was asked about, if any.
			std::optional<agent_conversation_chip> About;
			/// Where Draft comment writes the answer; none: the host decides.
			std::optional<agent_answer_draft_target> DraftTarget;
			QPointer<QPushButton>                    Draft;
			bool                                     Drafted = false;
			/// RFC-BPR-0116. A returning question's fresh first answer: the
			/// claim it leads with, and Ask more.
			std::optional<core::returning_operation> Returning;
			/// RFC-BPR-0117. The file Explain this file asked about; empty for
			/// every other turn.
			QString               Explain;
			QPointer<QLabel>      Claim;
			QPointer<QPushButton> AskMore;
			QPointer<QWidget>     Asked;
			/// [[BPR-1847]]. How it ended, its reply card, and the one way
			/// back an error offers.
			core::agent_turn_outcome  Outcome = core::agent_turn_outcome::answered;
			QPointer<QWidget>         Reply;
			QPointer<QPushButton>     Recovery;
			core::agent_turn_recovery RecoveryKind = core::agent_turn_recovery::none;
			/// [[BPR-1876]]. The check its cause names was seen not passed
			/// (running, failed or not run) after it ended, so a pass now is
			/// one made since; and the line that says it is fixed since.
			bool             CauseRechecked = false;
			QPointer<QLabel> FixedNote;
			/// [[BPR-1876]], [[BPR-1880]]. Delete: an error turn leaves the
			/// thread.
			QPointer<QPushButton> Delete;
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
		void         show_question(turn& aTurn);
		void         show_answer(turn& aTurn);
		QWidget*     Thread       = nullptr;
		QVBoxLayout* ThreadColumn = nullptr;
		/// Each turn's widgets, made for a new or restored turn.
		auto add_turn_widgets(turn& aTurn) -> void;
		void set_answer(turn& aTurn, const QString& aText);
		void settle_reply_footer(turn& aTurn);
		/// [[BPR-1847]]. Each ended turn's actions, as the core decides them.
		void present_turn_actions();
		void recover(const turn& aTurn);
		/// [[BPR-1876]]. Removes a turn whose message never reached the agent
		/// from the thread, and keeps the thread without it.
		void delete_turn(std::size_t aTurn);
		/// [[BPR-1847]]. Why the suggestions are not shown now, if not.
		[[nodiscard]] auto                                                          suggestions_verdict() const -> core::agent_suggestions_verdict;
		void                                                                        refresh_times();
		void                                                                        dress_answer(markdown_body_view& aAnswer) const;
		void                                                                        fit_run_settings();
		QLabel*                                                                     SessionTitle = nullptr;
		QWidget*                                                                    EmptyIntro   = nullptr;
		QWidget*                                                                    Field        = nullptr;
		std::optional<QFont>                                                        CodeFont;
		std::optional<markdown_code_theme>                                          CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		std::uint64_t                                                               NextSerial = 1U;
		/// The answer is as tall as its text at the thread's width.
		void                                                fit_answer(markdown_body_view& aAnswer) const;
		std::function<void(std::vector<agent_thread_turn>)> ThreadObserver;
		std::function<void()>                               ClearDispatch;
		std::function<bool(std::size_t)>                    ClearConfirm;
		QToolButton*                                        ClearSession = nullptr;
		std::function<agent_suggestion_context()>           SuggestionContext;
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
		int                                      LastPhase = 0;
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
		/// [[BPR-1999]]. The session's cost under the composer.
		QLabel*                                              SessionCostLine = nullptr;
		std::optional<core::agent_cost>                      SessionTotal;
		agent_tab_run_observer                               RunObserver;
		std::function<void(const QString&)>                  EndedObserver;
		std::function<void(std::optional<core::agent_cost>)> CostObserver;
		/// [[BPR-1999]]. An input method is composing in the question field.
		bool                                                                       Composing = false;
		[[nodiscard]] auto                                                         send_tip() const -> QString;
		std::optional<std::pair<QString, std::pair<std::uint32_t, std::uint32_t>>> LinesSeen;
		bool                                                                       LinesDetached = false;
		/// The subject each suggested prompt carries, by its words.
		std::map<QString, std::pair<core::agent_suggested_subject, bool>> SuggestionSubjects;
		/// The suggestion being sent, while it is.
		std::optional<std::pair<core::agent_suggested_subject, bool>> SendingSuggestion;
		/// RFC-BPR-0116. The returning prompts by their words, the one being
		/// sent, the one a sent question was, and what the host offers.
		std::map<QString, core::returning_operation> ReturningPrompts;
		std::optional<core::returning_operation>     SendingReturning;
		std::optional<core::returning_operation>     PendingReturning;
		/// RFC-BPR-0117. The file Explain this file is being sent for, the
		/// one a sent question was for, and who is told of its turns.
		QString                                             SendingExplain;
		QString                                             PendingExplain;
		std::function<void(const explain_file_progress&)>   ExplainObserver;
		void                                                publish_explain(std::size_t aTurn, std::optional<agent_run_status> aRunning, const QString& aEnding, bool aAnswered, qint64 aSeconds);
		core::latest_upload_offer                           LatestUpload;
		QLabel*                                             LatestUploadNote = nullptr;
		std::function<void(const QString&, const QString&)> AskMoreDispatch;
		std::optional<core::agent_question>                 LastQuestion;
		[[nodiscard]] auto                                  current_suggestion_context() const -> agent_suggestion_context;
		void                                                present_lines_chip(const agent_suggestion_context& aContext);
		std::function<void()>                               ConversationDismiss;
		std::function<agent_answer_draft_target()>          DraftTargetSource;
		agent_answer_draft_dispatch                         DraftDispatch;
		agent_answer_draft_navigation                       DraftNavigate;
		QLabel*                                             Log      = nullptr;
		bool                                                LogShown = false;
		/// [[BPR-1876]]. The latest turn says its cause is fixed since, so
		/// the stopped banner stands down.
		bool LatestFixedSince = false;
		/// [[BPR-1876]]. The stopped run's turn was deleted, so its banner
		/// stands down until the next run.
		bool StoppedDismissed = false;
		/// The panels' controls this tab shows, and where each came from.
		std::vector<std::pair<QPointer<QWidget>, QPointer<QWidget>>> Borrowed;
		/// The panels may go first when the window closes.
		QPointer<QObject>  LaunchAlive;
		QPointer<QObject>  ChatAlive;
		QPointer<QObject>  FindingsAlive;
		bool               Dying = false;
		[[nodiscard]] auto panels_alive() const noexcept -> bool
		{
			if (Dying || LaunchAlive == nullptr || ChatAlive == nullptr || FindingsAlive == nullptr) {
				return false;
			}
			for (const auto& [widget, owner] : Borrowed) {
				if (widget == nullptr) {
					return false;
				}
			}
			return true;
		}
	};
} // namespace bpr::qt

#endif
