#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SESSION_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SESSION_LIST_HPP

#include "bpr/application/agent_conversation.hpp"

#include <QDialog>
#include <QPointer>
#include <QString>

#include <functional>
#include <optional>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

/// \file
/// \brief [[BPR-NNNN]]. History › Manage sessions…: the Agent tab's sessions
/// for this pull request, each renamed in place or deleted after a question.
/// Presentation only: core tidies a name and decides a deletion; the host
/// performs both and the history is shown again.
namespace bpr::qt {
	class destructive_question;

	/// \brief The session list of one chat.
	/// \details Not modal; deleted with its owner. A row's text is the
	/// session's title; editing it (double-click, F2 or Rename…) names the
	/// session, and an empty name restores its default title. Delete… asks a
	/// destructive question first. Both are refused while a turn runs.
	/// Children are named `agentSessionList`, `agentSessionRename`,
	/// `agentSessionDelete`, `agentSessionStatus` and, for the question,
	/// `deleteAgentSession…`.
	class agent_session_list final : public QDialog
	{
	public:
		/// \brief `aName` set: rename the session `aId`; none: delete it.
		using dispatch = std::function<void(const std::string& aId, std::optional<std::string> aName)>;
		explicit agent_session_list(QWidget* aParent);
		void set_dispatch(dispatch aDispatch);
		/// \brief The sessions to list, newest first, the selected one marked.
		void present(const application::native_agent_history_projection& aHistory);
		/// \brief A turn is running: rename and delete wait.
		void set_running(bool aRunning);
		/// \brief What the last rename or deletion did, or empty.
		void               present_status(const QString& aStatus);
		[[nodiscard]] auto list() const noexcept -> QListWidget*;
		[[nodiscard]] auto rename_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto delete_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief The question Delete… opened, while it is open.
		[[nodiscard]] auto question() const noexcept -> destructive_question*;
		/// \brief Asks to delete the selected session, as Delete… does.
		void ask_delete();

	private:
		void refresh_buttons();
		void edited(QListWidgetItem* aItem);

		dispatch                       Dispatch;
		QListWidget*                   List   = nullptr;
		QPushButton*                   Rename = nullptr;
		QPushButton*                   Delete = nullptr;
		QLabel*                        Status = nullptr;
		QPointer<destructive_question> Question;
		bool                           Running = false;
	};
}

#endif
