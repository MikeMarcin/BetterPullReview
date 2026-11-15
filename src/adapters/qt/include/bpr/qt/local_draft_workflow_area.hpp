#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_WORKFLOW_AREA_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_WORKFLOW_AREA_HPP

#include "bpr/application/local_draft_workbench.hpp"

#include <QWidget>

#include <functional>
#include <optional>
#include <string>

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief The gestures the Workflow area sends to its host.
	struct local_draft_workflow_dispatch
	{
		/// A workflow by ID, or none.
		std::function<void(std::optional<std::string>)> Select;
		std::function<void()>                           AcceptUpdate;
		std::function<void(core::workflow_publisher)>   Publisher;
		/// A preparation action handed off, with the author's answer when continuing.
		std::function<void(const std::string&, const std::string&)> HandOff;
		/// An agent action asked of its agent.
		std::function<void(const std::string&)> Ask;
		std::function<void(const std::string&)> ReadAnswer;
		std::function<void(const std::string&)> Lost;
		/// The recorded plan handed to the workflow's publisher, by its digest.
		std::function<void(const std::string&, const std::string&)> Publish;
		std::function<void(const std::string&)>                     Reconcile;
		std::function<void(const std::string&)>                     FollowUp;
		std::function<void(const std::string&)>                     ReconcileFollowUp;
		/// Copies a handoff's command for the author's terminal.
		std::function<void(const std::string&)> Copy;
	};

	/// \brief [[BPR-2033]], [[BPR-2034]], RFC-BPR-0112. A local draft's
	/// optional workflow in Overview's Status tab: the explicit selection,
	/// each action's origin, who acts, its declared effect scope and its
	/// current outcome, the required checks, and the handoff commands.
	/// \details It decides nothing and runs nothing: a command action's
	/// program is the author's to run, and every result reads as reported
	/// by the companion, never as observed by BPR. An action bound to an
	/// agent wears the agent accent (X11); one bound to a program does not.
	class local_draft_workflow_area final : public QWidget
	{
	public:
		explicit local_draft_workflow_area(QWidget* aParent = nullptr);

		void set_dispatch(local_draft_workflow_dispatch aDispatch);
		void present(const application::local_draft_workbench_projection& aProjection);

		[[nodiscard]] auto chooser() const noexcept -> QComboBox* { return Chooser; }
		[[nodiscard]] auto summary() const noexcept -> QLabel* { return Summary; }
		[[nodiscard]] auto actions() const noexcept -> QWidget* { return Actions; }
		/// \brief The area's text as one block, for a course and an accessible reading.
		[[nodiscard]] auto text() const -> QString;

	private:
		void                                          rebuild();
		local_draft_workflow_dispatch                 Dispatch;
		application::local_draft_workbench_projection Projection;
		QComboBox*                                    Chooser      = nullptr;
		QLabel*                                       Summary      = nullptr;
		QPushButton*                                  Update       = nullptr;
		QComboBox*                                    Publisher    = nullptr;
		QWidget*                                      PublisherRow = nullptr;
		QWidget*                                      Actions      = nullptr;
		QVBoxLayout*                                  Rows         = nullptr;
		bool                                          Presenting   = false;
	};

	/// \brief What an action's state says, from the author's side.
	[[nodiscard]] auto workflow_state_text(const application::local_draft_workflow_action_view& aAction) -> QString;
} // namespace bpr::qt

#endif
