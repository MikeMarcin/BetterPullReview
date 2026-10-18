#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_VIEW_HPP

#include <QPointer>
#include <QWidget>

#include <functional>
#include <vector>

class QAbstractButton;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	class agent_launch_panel;
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

		/// \brief Set up an agent: Settings › Agents & context.
		void set_setup_dispatch(std::function<void()> aDispatch);
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
		void present_banner();

		agent_launch_panel&   Launch;
		side_chat_panel&      Chat;
		finding_queue_view&   Findings;
		QWidget&              HistoryPanel;
		std::function<void()> Setup;
		agent_tab_state       State = agent_tab_state::not_set_up;
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
		QVBoxLayout*          CardColumn   = nullptr;
		QWidget*              Start        = nullptr;
		QLabel*               Head         = nullptr;
		QToolButton*          History      = nullptr;
		QPushButton*          Ask          = nullptr;
		QPushButton*          Context      = nullptr;
		QLabel*               Chips        = nullptr;
		QLabel*               Log          = nullptr;
		bool                  LogShown     = false;
		std::vector<QPointer<QWidget>> CardWidgets;
	};
} // namespace bpr::qt

#endif
