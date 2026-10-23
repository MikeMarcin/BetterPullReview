#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP

#include <QElapsedTimer>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <cstddef>
#include <functional>
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
	class side_chat_panel;

	/// \brief [[BPR-1594]]. What the Agent tab says about its agent.
	/// \brief [[BPR-PENDING-AQ6]], owner look v002 ("I really have no clue
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
	};

	/// \brief [[BPR-PENDING-AQ6]], owner decision (b) 2026-10-23. One finished
	/// turn of the thread, as it is kept for its pull request and shown again.
	struct agent_thread_turn
	{
		QString Question;
		QString Answer;
		QString Footer;

		auto operator==(const agent_thread_turn&) const -> bool = default;
	};

	/// \brief [[BPR-PENDING-AQ6]], owner decision (a) 2026-10-23. What the
	/// suggested prompts may name: the open file's path, and the selected
	/// lines, one-based, first and last.
	struct agent_suggestion_context
	{
		QString                                                OpenFile;
		std::optional<std::pair<std::uint32_t, std::uint32_t>> Lines;
	};

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
		/// [[BPR-PENDING-AQ6]]. The setup is in place of the conversation.
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
		/// \brief [[BPR-PENDING-AQ6]], RFC-BPR-0092. Shows `aSetup` in place
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
		[[nodiscard]] auto ready_card() const noexcept -> QWidget* { return ReadyCard; }
		[[nodiscard]] auto setup_view() const noexcept -> agent_setup_view* { return SetupView; }
		/// \brief [[BPR-PENDING-AQ6]], owner look v002 ("Is it not meant to
		/// work like the claude code vscode plugin where it's like a chat
		/// thread?"). The conversation's turns, oldest first: what was asked
		/// (or "Review this pull request"), the agent's answer as it streams,
		/// and how the turn ended.
		[[nodiscard]] auto conversation_thread() const noexcept -> QWidget* { return Thread; }
		[[nodiscard]] auto turn_count() const noexcept -> std::size_t { return Turns.size(); }
		[[nodiscard]] auto turn_answer(std::size_t aTurn) const -> QString;
		[[nodiscard]] auto turn_question(std::size_t aTurn) const -> QString;
		[[nodiscard]] auto turn_footer(std::size_t aTurn) const -> QString;
		/// \brief [[BPR-PENDING-AQ6]], owner decision (b). Shows a pull
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
		/// \brief [[BPR-PENDING-AQ6]], owner decision (a) 2026-10-23
		/// ("Suggested prompts described that way sound great"). Where the
		/// suggestions read the open file and the selected lines, and a call
		/// to read them again when either changes.
		void set_suggestion_context(std::function<agent_suggestion_context()> aContext);
		void refresh_suggestions();
		/// \brief The suggested prompts shown now, in order; each is sent as
		/// written. With an empty thread they stand above the question field,
		/// and once it has turns they are the Suggestions menu in the field.
		[[nodiscard]] auto suggestions() const -> QStringList;
		[[nodiscard]] auto suggestion_buttons() const -> std::vector<QPushButton*>;
		[[nodiscard]] auto suggestions_menu_button() const noexcept -> QToolButton* { return SuggestionsMenu; }
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
		[[nodiscard]] auto ask_button() const noexcept -> QPushButton* { return Ask; }
		[[nodiscard]] auto head_chip() const noexcept -> QLabel* { return Head; }

	protected:
		void showEvent(QShowEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void rebuild_findings();
		/// \brief Folds a card's actions into More when the drawer is narrow.
		static void        fit_card_actions(QWidget& aCard, int aRoom);
		[[nodiscard]] auto card_room() const -> int;
		void               present_banner();

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
		agent_run_settings_dispatch     RunSettingsDispatch;
		QToolButton*                    RunSettings = nullptr;
		QLabel*                         Notice      = nullptr;
		[[nodiscard]] auto              current_run_settings() const -> const agent_run_settings*;
		void                            present_run_settings_button();
		void                            ask();
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
		};
		std::vector<turn> Turns;
		QWidget*          Thread       = nullptr;
		QVBoxLayout*      ThreadColumn = nullptr;
		/// Each turn's widgets, made for a new or restored turn.
		auto add_turn_widgets(turn& aTurn) -> void;
		void set_answer(turn& aTurn, const QString& aText);
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
		bool                           Finished   = false;
		QWidget*                       ReadyCard  = nullptr;
		QLabel*                        ReadyTitle = nullptr;
		QLabel*                        ReadyText  = nullptr;
		QLabel*                        ReadyGets  = nullptr;
		QPushButton*                   Ask        = nullptr;
		QPushButton*                   Context    = nullptr;
		QLabel*                        Chips      = nullptr;
		QLabel*                        Log        = nullptr;
		bool                           LogShown   = false;
		std::vector<QPointer<QWidget>> CardWidgets;
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
