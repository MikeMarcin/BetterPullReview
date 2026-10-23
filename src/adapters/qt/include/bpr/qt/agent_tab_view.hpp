#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP

#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>

#include <functional>
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
	class side_chat_panel;

	/// \brief [[BPR-1594]]. What the Agent tab says about its agent.
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
		void               set_first_shown(std::function<void()> aObserver);
		[[nodiscard]] auto ready_card() const noexcept -> QWidget* { return ReadyCard; }
		[[nodiscard]] auto setup_view() const noexcept -> agent_setup_view* { return SetupView; }
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

		agent_launch_panel&   Launch;
		side_chat_panel&      Chat;
		finding_queue_view&   Findings;
		QWidget&              HistoryPanel;
		std::function<void()> Setup;
		agent_tab_state       State        = agent_tab_state::not_set_up;
		QStackedWidget*       Pages        = nullptr;
		drawer_empty_state*   Empty        = nullptr;
		QWidget*              Conversation = nullptr;
		QWidget*              HistoryPage  = nullptr;
		QWidget*              Banner       = nullptr;
		QLabel*               BannerTitle  = nullptr;
		QLabel*               BannerText   = nullptr;
		QPushButton*          BannerFirst  = nullptr;
		QPushButton*          BannerSecond = nullptr;
		QWidget*              Cards        = nullptr;
		QWidget*              Viewport     = nullptr;
		QVBoxLayout*          CardColumn   = nullptr;
		QWidget*              Start        = nullptr;
		QLabel*               Head         = nullptr;
		QToolButton*          History      = nullptr;
		QMenu*                HistoryMenu  = nullptr;
		agent_setup_view*     SetupView    = nullptr;
		std::function<void()> CheckAgain;
		std::function<void()> FirstShown;
		QTimer*               Ticker = nullptr;
		QElapsedTimer         Started;
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
