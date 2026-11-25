#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SESSION_MENU_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SESSION_MENU_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_session_edit.hpp"

#include <QPointer>
#include <QString>
#include <QWidgetAction>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QAction;
class QLabel;
class QLineEdit;
class QMenu;

/// \file
/// \brief [[BPR-2377]]. The session list of a chat's History menu: each
/// session a row with its title (open it), a pencil (rename it in place), a
/// trash can (delete it after a question) and an i (its turns, revision and
/// cost on hover), and Delete all sessions… beside it.
/// \details Owner, 2026-11-24 (agent panel review): "put trash can and pencil
/// on the session rows in the menu to delete or rename them. Put the cost and
/// turns in a info tooltip ... you can just have delete all sessions". One
/// component for every chat with sessions (the Agent tab and the tour side
/// chat; the agent panel's redesign drops it into its own header). It knows
/// only a history projection, whether a turn runs, and a dispatch; core tidies
/// names and decides deletions, the host performs them.
namespace bpr::qt {
	class destructive_question;

	/// \brief What a rename (`aRenaming`) or deletion did, in the list's words.
	[[nodiscard]] auto agent_session_edit_words(const application::agent_session_edit_receipt& aReceipt, bool aRenaming) -> QString;

	/// \brief The rows, as a menu item: add it to a menu, with
	/// `delete_all_action()` after it.
	/// \details Each menu that shows it gets its own rows widget, rebuilt when
	/// `present` changes the history; the rows' children are named
	/// `agentSessionRow`, `agentSessionTitle`, `agentSessionRenameField`,
	/// `agentSessionRename`, `agentSessionDelete`, `agentSessionInfo`, and the
	/// list's `agentSessionStatus`. Delete and Delete all are refused while a
	/// turn runs; rename is not. Questions open over `aOwner`.
	class agent_session_menu final : public QWidgetAction
	{
	public:
		struct dispatch
		{
			std::function<void(const std::string& aId)>                    Open;
			std::function<void(const std::string& aId, std::string aName)> Rename;
			std::function<void(const std::string& aId)>                    Delete;
			std::function<void()>                                          DeleteAll;
		};

		agent_session_menu(QWidget& aOwner, QObject* aParent);
		void set_dispatch(dispatch aDispatch);
		/// \brief The sessions, newest first, the selected one marked.
		void present(const application::native_agent_history_projection& aHistory);
		/// \brief A turn is running: delete waits.
		void set_running(bool aRunning);
		/// \brief What the last rename or deletion did, or empty.
		void present_status(const QString& aStatus);
		/// \brief Delete all sessions…, for the menu beside the rows.
		[[nodiscard]] auto delete_all_action() const noexcept -> QAction*;
		/// \brief The question Delete or Delete all opened, while it is open.
		[[nodiscard]] auto question() const noexcept -> destructive_question*;

		/// \brief For courses and the menus' own reads.
		[[nodiscard]] auto ids() const -> std::vector<std::string>;
		[[nodiscard]] auto selected() const -> std::string;
		[[nodiscard]] auto turns(const std::string& aId) const -> std::size_t;
		[[nodiscard]] auto replay(const std::string& aId) const -> bool;
		[[nodiscard]] auto status() const -> QString { return Status; }
		/// \brief Opens the session, as a click on its title does.
		void open(const std::string& aId);
		/// \brief Renames it, as the pencil and Return do.
		void rename(const std::string& aId, const QString& aTyped);
		/// \brief Asks to delete it, as the trash can does.
		void ask_delete(const std::string& aId);
		/// \brief Asks to delete them all, as Delete all sessions… does.
		void ask_delete_all();

	protected:
		auto createWidget(QWidget* aParent) -> QWidget* override;

	private:
		void               fill(QWidget& aRows);
		void               refresh();
		[[nodiscard]] auto entry(const std::string& aId) const -> const application::native_agent_history_entry*;
		static void        close_popups();

		QWidget&                                     Owner;
		dispatch                                     Dispatch;
		application::native_agent_history_projection History;
		bool                                         Running = false;
		QString                                      Status;
		QAction*                                     DeleteAll = nullptr;
		QPointer<destructive_question>               Question;
	};
}

#endif
