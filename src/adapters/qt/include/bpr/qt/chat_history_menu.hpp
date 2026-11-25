#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_HISTORY_MENU_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_HISTORY_MENU_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/core/chat_history.hpp"
#include "bpr/qt/agent_session_menu.hpp"

#include <QObject>
#include <QString>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

class QAction;
class QMenu;
class QToolButton;
class QWidget;

/// \file
/// \brief The History menu of a chat with an agent (RFC-BPR-0130, header A):
/// New session first, the sessions one row each, Delete all sessions…, then
/// what the chat adds, and last the revision the open session reads.
/// \details One menu for the Agent tab and the tour's side chat (owner,
/// attended review 2026-11-24: "It looks like tour built a different session
/// selector from agent too for no reason."). Core's `plan_chat_history`
/// decides its items and whether New session can be chosen; a New session
/// that cannot is greyed and says why under its name, so it never does
/// nothing when pressed. The menu decides and performs nothing: it shows a
/// history projection and what its chat says of itself, and sends gestures.
namespace bpr::qt {
	class chat_history_menu final : public QObject
	{
	public:
		struct dispatch
		{
			/// The menu is about to open: read the sessions again.
			std::function<void()>                       AboutToShow;
			std::function<void()>                       NewSession;
			std::function<void(const std::string& aId)> Open;
			/// Continue the session `aId` from its saved transcript.
			std::function<void(const std::string& aId)> CarryOver;
			/// Read another part's chat.
			std::function<void(const std::string& aId)> ViewOther;
			/// Renames (`aName` set) or deletes `aId`; an empty id with no name
			/// deletes every session listed.
			std::function<void(const std::string& aId, std::optional<std::string> aName)> Edit;
			std::function<void()>                                                         CopyConversation;
			std::function<void()>                                                         ShowEveryRun;
			std::function<void()>                                                         ResetAgents;
		};

		/// \brief What the chat says of itself; the sessions come by `present`.
		struct chat_state
		{
			bool        AgentReady = true;
			bool        Closed     = false;
			bool        Running    = false;
			bool        Busy       = false;
			std::size_t ShownTurns = 0U;
			/// The chat shows the pinned-revision card: the open session
			/// reads a revision other than the one under review.
			bool Pinned      = false;
			bool EveryRun    = false;
			bool ResetAgents = false;
			/// What carrying a conversation over keeps and leaves out.
			QString CarryOverTip;
			auto    operator==(const chat_state&) const -> bool = default;
		};

		/// \brief Makes the menu of `aButton`, which opens it at once.
		/// Questions (Delete, Delete all) open over `aOwner`. The actions are
		/// named `<aPrefix>NewSession` and `<aPrefix>TranscriptReplay`, the
		/// menu `<aPrefix>HistoryMenu`.
		chat_history_menu(core::chat_history_kind aKind, QWidget& aOwner, QToolButton& aButton, const QString& aPrefix);
		void set_dispatch(dispatch aDispatch);
		/// \brief The sessions as the host read them. While they load the
		/// rows stay as they were; a read-only view is not this menu's.
		void present(const application::native_agent_history_projection& aHistory);
		void set_state(const chat_state& aState);
		/// \brief Where the menu reads its chat's state: as it opens, as New
		/// session is pressed, and at each `refresh`. A chat whose turns
		/// change without a word to the menu is still read right.
		void set_state_source(std::function<chat_state()> aSource);
		/// \brief The chat changed: decide the menu again.
		void refresh();
		/// \brief What the last rename or deletion did, or empty.
		void present_status(const QString& aStatus);

		[[nodiscard]] auto menu() const noexcept -> QMenu* { return Menu; }
		[[nodiscard]] auto rows() const noexcept -> agent_session_menu* { return Rows; }
		/// \brief New session: enabled exactly when core offers it.
		[[nodiscard]] auto new_session_action() const noexcept -> QAction*;
		[[nodiscard]] auto carry_over_action() const noexcept -> QAction* { return CarryOver; }
		[[nodiscard]] auto copy_action() const noexcept -> QAction* { return Copy; }
		[[nodiscard]] auto revision_action() const noexcept -> QAction* { return Revision; }
		/// \brief Why New session is greyed, as the menu says it; empty when
		/// it can be chosen.
		[[nodiscard]] auto new_session_reason() const -> QString;
		[[nodiscard]] auto plan() const noexcept -> const core::chat_history_plan& { return Plan; }
		[[nodiscard]] auto loading() const noexcept -> bool { return Loading; }

	private:
		class new_session_item;
		void               rebuild();
		[[nodiscard]] auto facts() const -> core::chat_history_facts;

		core::chat_history_kind                      Kind;
		dispatch                                     Dispatch;
		application::native_agent_history_projection History;
		chat_state                                   State;
		std::function<chat_state()>                  Source;
		core::chat_history_plan                      Plan;
		bool                                         Loading   = false;
		bool                                         Built     = false;
		QMenu*                                       Menu      = nullptr;
		QMenu*                                       Others    = nullptr;
		agent_session_menu*                          Rows      = nullptr;
		new_session_item*                            NewItem   = nullptr;
		QAction*                                     CarryOver = nullptr;
		QAction*                                     Copy      = nullptr;
		QAction*                                     EveryRun  = nullptr;
		QAction*                                     Reset     = nullptr;
		QAction*                                     Revision  = nullptr;
	};
}

#endif
