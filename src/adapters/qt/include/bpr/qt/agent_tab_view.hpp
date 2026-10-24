#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP

#include <QDateTime>
#include <QElapsedTimer>
#include <QFont>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "bpr/core/agent_finding.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <cstddef>
#include <functional>
#include <map>
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
class QVBoxLayout;

namespace bpr::qt {
	class agent_launch_panel;
	class agent_setup_view;
	class drawer_empty_state;
	class finding_queue_view;
	class markdown_body_view;
	class appearance_profile;
	class side_chat_panel;

	/// \brief [[BPR-1594]]. What the Agent tab says about its agent.
	/// \brief [[BPR-1753]], owner look v002 ("I really have no clue
	/// what my agent will be actually doing"). What one agent runs with, for
	/// the tab's model and effort control.
	struct agent_run_settings
	{
		/// The profile these settings are for.
		std::uint64_t Profile = 0U;
		/// The agent's plain name and the program that runs.
		QString Agent;
		QString Program;
		bool    ModelChoosable  = false;
		bool    EffortChoosable = false;
		/// Empty: the agent's own default.
		std::string Model;
		std::string Effort;
		/// Models to offer by name (value, label), besides Other….
		std::vector<std::pair<std::string, QString>> Models;
		std::vector<std::string>                     Efforts;
		/// Why the settings cannot be chosen, when they cannot.
		QString Note;
		/// Owner look v006. The harness is being asked for its models.
		bool Listing = false;
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

		auto operator==(const agent_thread_turn&) const -> bool = default;
	};

	/// \brief Owner look v004 ("The models aren't listing their version
	/// numbers"). A model as a reviewer reads it: `claude-sonnet-5-5` is
	/// "Sonnet 5.5", a bare alias such as `sonnet` is "Sonnet (newest)", and
	/// any other name is shown as the agent reported it.
	[[nodiscard]] auto agent_model_display_name(const QString& aModel) -> QString;

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

	struct agent_finding_card_request
	{
		/// The delivery the finding came in, and its place there.
		std::uint64_t             Session = 0U;
		std::size_t               Ordinal = 0U;
		core::agent_finding       Finding;
		agent_finding_card_action Action = agent_finding_card_action::draft_comment;
	};
	using agent_finding_card_dispatch = std::function<void(const agent_finding_card_request&)>;
	/// \brief Save as note on a plain answer: the text (the whole reply, or
	/// the selected part), and where the host says how it went: the note it
	/// made, which Go to note opens (owner look v004), when it was saved.
	using agent_answer_note_done     = std::function<void(bool aSaved, const QString& aWhy, std::uint64_t aNote)>;
	using agent_answer_note_dispatch = std::function<void(const QString& aText, agent_answer_note_done aDone)>;
	/// \brief Opens a private note the tab saved, in the Notes area.
	using agent_note_navigation = std::function<void(std::uint64_t aNote)>;
	/// \brief Owner look v004 ("This should be a link to the file and line I
	/// can click"). A file of the pull request a reference names: the path
	/// it is shown under, or nothing when the pull request has no such file.
	using agent_file_resolver = std::function<std::optional<QString>(const QString& aPath)>;
	/// \brief Opens a file of the pull request in Review at a one-based line
	/// of its head (or, `aBefore`, of the change's base).
	using agent_line_navigation = std::function<void(const QString& aPath, std::uint32_t aLine, bool aBefore)>;

	/// \brief One place a text names a file and line of the pull request.
	struct agent_line_reference
	{
		/// Where in the text, in UTF-16 units.
		qsizetype Begin  = 0;
		qsizetype Length = 0;
		QString   Path;
		/// One-based, first and last.
		std::uint32_t First  = 0U;
		std::uint32_t Last   = 0U;
		bool          Before = false;

		auto operator==(const agent_line_reference&) const -> bool = default;
	};

	/// \brief Every reference to a file and line in `aText`: `path:34`,
	/// `path:34-36`, `path line 34`, `path lines 34–36` (either followed by
	/// "before the change" for the base), each only when `aResolve` knows
	/// the path; and, when `aContextFile` is not empty, a bare `line 34` or
	/// `lines 34–36`, which names that file.
	[[nodiscard]] auto agent_line_references(const QString& aText, const agent_file_resolver& aResolve, const QString& aContextFile = { }) -> std::vector<agent_line_reference>;
	/// \brief `aMarkdown` with each reference outside code made a link, and
	/// an inline code span that is wholly one made a link around it. Fenced
	/// code is left as written.
	[[nodiscard]] auto agent_markdown_with_line_links(const QString& aMarkdown, const agent_file_resolver& aResolve, const QString& aContextFile = { }) -> QString;
	/// \brief The link a reference is opened by, and what it names: a path,
	/// `#L` for the head or `#B` for the base, and the first line.
	[[nodiscard]] auto agent_line_link(const QString& aPath, std::uint32_t aLine, bool aBefore) -> QString;
	[[nodiscard]] auto agent_line_link_target(const QString& aLink) -> std::optional<std::pair<QString, std::pair<std::uint32_t, bool>>>;

	/// \brief A change to the model, the effort, or both; empty is the
	/// agent's own default. Answers whether the host took it.
	using agent_run_settings_dispatch = std::function<bool(std::uint64_t aProfile, std::optional<std::string> aModel, std::optional<std::string> aEffort)>;

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
		/// kept, Ask again and Show the log.
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
		/// of the conversation while it is needed, adds **Set up agent…** and
		/// **Change repository folder…** to History, and offers **Check
		/// again** when an Ask ends for a setup reason (`aCheckAgain`).
		void adopt_setup(agent_setup_view& aSetup, std::function<void()> aCheckAgain);
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
		/// \brief [[BPR-1753]], owner decision (b). Shows a pull
		/// request's kept thread in place of the one shown (none: an empty
		/// thread), and says each finished turn to `set_thread_observer`'s
		/// observer with the whole thread, so the host keeps it.
		void               present_thread(const std::vector<agent_thread_turn>& aTurns);
		void               set_thread_observer(std::function<void(std::vector<agent_thread_turn>)> aObserver);
		[[nodiscard]] auto thread_turns() const -> std::vector<agent_thread_turn>;
		/// \brief Clear session: asks `set_clear_confirmation`'s question when
		/// the thread has turns (they leave it), then empties the thread and
		/// tells the host, which starts the next question afresh.
		void               set_clear_session_dispatch(std::function<void()> aDispatch);
		void               set_clear_confirmation(std::function<bool(std::size_t aTurns)> aConfirm);
		void               clear_session();
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
		/// \brief Sends `aPrompt` as a chat turn, as if typed and asked.
		void send_prompt(const QString& aPrompt);
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
		void               set_finding_card_dispatch(agent_finding_card_dispatch aDispatch);
		void               present_finding_card_state(std::uint64_t aSession, std::size_t aOrdinal, agent_finding_card_state aState, const QString& aWhy = { }, std::uint64_t aNote = 0U);
		[[nodiscard]] auto finding_card_state(std::size_t aCard) const -> agent_finding_card_state;
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
		/// \brief Which files references may name, and where a link to one
		/// goes; references are links only when both are set.
		void set_line_links(agent_file_resolver aResolve, agent_line_navigation aNavigate);
		/// \brief Owner look v004 ("Not drawing inline comment with syntax
		/// highlighing"): the file name the answers' inline code is
		/// coloured as, such as `code.cpp` for a C++ pull request; empty
		/// leaves it plain.
		void               set_inline_code_file(const QString& aFileName);
		[[nodiscard]] auto turn_question_label(std::size_t aTurn) const -> QLabel*;
		[[nodiscard]] auto turn_answer_view(std::size_t aTurn) const -> markdown_body_view*;
		[[nodiscard]] auto turn_note_button(std::size_t aTurn) const -> QPushButton*;
		/// \brief The cards a turn brought, under its answer.
		[[nodiscard]] auto turn_cards(std::size_t aTurn) const -> std::vector<QWidget*>;
		[[nodiscard]] auto ask_button() const noexcept -> QPushButton* { return Ask; }
		[[nodiscard]] auto head_chip() const noexcept -> QLabel* { return Head; }

	protected:
		void showEvent(QShowEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
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
		};
		[[nodiscard]] auto make_card(card_entry& aEntry, QWidget* aParent) -> QWidget*;
		void               show_card_state(card_entry& aEntry);
		void               act_on_card(std::uint64_t aCard, agent_finding_card_action aAction);
		std::uint64_t      NextCardId = 1U;
		/// The findings that arrived during the turn that just ended go
		/// under its answer.
		void                               claim_findings(std::size_t aTurn);
		std::vector<card_entry>            CardEntries;
		std::optional<std::uint64_t>       ClaimedSession;
		agent_finding_card_dispatch        CardDispatch;
		agent_answer_note_dispatch         NoteDispatch;
		agent_note_navigation              NoteNavigate;
		std::function<void(std::uint64_t)> CatalogDispatch;
		bool                               MenuStale = false;
		std::function<void()>              ResetDispatch;
		std::function<bool()>              ResetConfirm;
		agent_file_resolver                LineResolve;
		agent_line_navigation              LineNavigate;
		QString                            InlineCodeFile;
		void                               follow_link(const QString& aLink);
		void                               present_banner();

		agent_launch_panel&             Launch;
		side_chat_panel&                Chat;
		finding_queue_view&             Findings;
		QWidget&                        HistoryPanel;
		std::function<void()>           Setup;
		agent_tab_state                 State        = agent_tab_state::not_set_up;
		QStackedWidget*                 Pages        = nullptr;
		drawer_empty_state*             Empty        = nullptr;
		QWidget*                        Conversation = nullptr;
		QWidget*                        HistoryPage  = nullptr;
		QWidget*                        Banner       = nullptr;
		QLabel*                         BannerTitle  = nullptr;
		QLabel*                         BannerText   = nullptr;
		QPushButton*                    BannerFirst  = nullptr;
		QPushButton*                    BannerSecond = nullptr;
		QWidget*                        Cards        = nullptr;
		QWidget*                        Viewport     = nullptr;
		QVBoxLayout*                    CardColumn   = nullptr;
		QLabel*                         Head         = nullptr;
		QToolButton*                    History      = nullptr;
		QMenu*                          HistoryMenu  = nullptr;
		agent_setup_view*               SetupView    = nullptr;
		std::function<void()>           CheckAgain;
		std::function<void()>           FirstShown;
		std::vector<agent_run_settings> RunSettingsShown;
		/// The model each agent last reported running, by its profile.
		std::map<std::uint64_t, QString> ReportedModels;
		agent_run_settings_dispatch      RunSettingsDispatch;
		QToolButton*                     RunSettings = nullptr;
		QLabel*                          Notice      = nullptr;
		[[nodiscard]] auto               current_run_settings() const -> const agent_run_settings*;
		void                             present_run_settings_button();
		void                             ask();
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
			QPointer<QLabel>             Footer;
			QPointer<QPushButton>        SaveNote;
			QPointer<QVBoxLayout>        CardColumn;
			QPointer<QToolButton>        Copy;
			QPointer<QLabel>             AskedTime;
			QPointer<QLabel>             AnsweredTime;
			QDateTime                    AskedAt;
			QDateTime                    AnsweredAt;
			/// Names the answer's body, so its code is coloured as its own.
			std::uint64_t Serial = 0U;
			/// The note Save as note made; 0 for none.
			std::uint64_t    Note = 0U;
			QPointer<QLabel> QuestionLabel;
			/// The one file the question names, which a bare "line 34" in the
			/// answer is in.
			QString ContextFile;
		};
		std::vector<turn> Turns;
		/// The question and the answer as shown, their references links.
		void         show_question(turn& aTurn);
		void         show_answer(turn& aTurn);
		QWidget*     Thread       = nullptr;
		QVBoxLayout* ThreadColumn = nullptr;
		/// Each turn's widgets, made for a new or restored turn.
		auto                                                                        add_turn_widgets(turn& aTurn) -> void;
		void                                                                        set_answer(turn& aTurn, const QString& aText);
		void                                                                        settle_reply_footer(turn& aTurn, bool aSucceeded);
		void                                                                        refresh_times();
		void                                                                        dress_answer(markdown_body_view& aAnswer) const;
		void                                                                        fit_run_settings();
		QLabel*                                                                     SessionTitle = nullptr;
		QWidget*                                                                    EmptyIntro   = nullptr;
		QWidget*                                                                    Field        = nullptr;
		QString                                                                     RunSettingsTip;
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
		QWidget*                                            Suggestions      = nullptr;
		QVBoxLayout*                                        SuggestionColumn = nullptr;
		QToolButton*                                        SuggestionsMenu  = nullptr;
		QLabel*                                             SuggestionsLine  = nullptr;
		std::function<void(bool)>                           SuggestionsObserver;
		QStringList                                         Prompts;
		std::vector<QPointer<QPushButton>>                  PromptButtons;
		/// What the next admitted request is: a question typed, or a review.
		std::optional<QString> PendingQuestion;
		bool                   PendingReview = false;
		int                    LastPhase     = 0;
		/// How many times findings were presented, and the count when the
		/// running turn began: a turn names only the findings it brought.
		std::uint64_t FindingsPresented = 0U;
		std::uint64_t FindingsAtTurn    = 0U;
		void          begin_turn();
		void          update_turn();
		void          progress();
		QTimer*       Ticker = nullptr;
		QElapsedTimer Started;
		/// The last request ended well and nothing has run since.
		bool         Finished   = false;
		QWidget*     ReadyCard  = nullptr;
		QLabel*      ReadyTitle = nullptr;
		QLabel*      ReadyText  = nullptr;
		QLabel*      ReadyGets  = nullptr;
		QPushButton* Ask        = nullptr;
		QToolButton* Context    = nullptr;
		QLabel*      Chips      = nullptr;
		QLabel*      Log        = nullptr;
		bool         LogShown   = false;
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
