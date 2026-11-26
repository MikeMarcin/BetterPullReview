#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_WORKFLOW_AREA_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_WORKFLOW_AREA_HPP

#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/core/author_mode.hpp"

#include <QWidget>

#include <functional>
#include <optional>
#include <string>

class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief The gestures the Workflow tab sends to its host.
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

	/// \brief [[BPR-2196]], RFC-BPR-0112. Author's Workflow tab: the author's
	/// checklist, one row per step.
	///
	/// \details The owner chose this shape on 2026-11-15 (design
	/// `local-draft-workflow-v2`): a filled mark is something BetterPullReview
	/// saw itself; an outlined mark, worded Reported, is something a companion
	/// reported, and nothing a companion returns is drawn filled. Who runs a
	/// row is one glyph whose tooltip says the run model once; the agent
	/// accent is on agent rows alone (X11). The explanations live in tooltips,
	/// so a row is its name, a short state and one small button. The rows and
	/// their marks are the core's (`core::decide_author_checklist`); this
	/// presents them and decides nothing, and every gesture leaves through
	/// `local_draft_workflow_dispatch`.
	class local_draft_workflow_area final : public QWidget
	{
	public:
		explicit local_draft_workflow_area(QWidget* aParent = nullptr);

		void set_dispatch(local_draft_workflow_dispatch aDispatch);
		/// \brief The draft's workflow, actions and publishing view.
		void present(const application::local_draft_workbench_projection& aProjection);
		/// \brief The core's checklist for the window's case.
		void present_checklist(const core::author_checklist& aChecklist, core::author_case aCase);
		/// \brief Without a worktree: the one ask, and what Choose Worktree… does.
		void set_worktree_dispatch(std::function<void()> aChoose);

		[[nodiscard]] auto chooser() const noexcept -> QComboBox* { return Chooser; }
		[[nodiscard]] auto summary() const noexcept -> QLabel* { return Summary; }
		[[nodiscard]] auto headline() const noexcept -> QLabel* { return Headline; }
		[[nodiscard]] auto action_rows() const noexcept -> QWidget* { return Actions; }
		[[nodiscard]] auto built_in_publisher() const noexcept -> QRadioButton* { return BuiltIn; }
		[[nodiscard]] auto external_publisher() const noexcept -> QRadioButton* { return External; }
		/// \brief The core's checklist last presented.
		[[nodiscard]] auto checklist() const noexcept -> const core::author_checklist& { return Checklist; }
		/// \brief The tab's text, tooltips included, as one block for a course
		/// and an accessible reading.
		[[nodiscard]] auto text() const -> QString;

	private:
		void rebuild();
		void present_footer();

		local_draft_workflow_dispatch                 Dispatch;
		std::function<void()>                         ChooseWorktree;
		application::local_draft_workbench_projection Projection;
		core::author_checklist                        Checklist;
		core::author_case                             Case           = core::author_case::local_draft;
		QLabel*                                       HeadlineMark   = nullptr;
		QLabel*                                       Headline       = nullptr;
		QLabel*                                       HeadlineAside  = nullptr;
		QLabel*                                       Summary        = nullptr;
		QWidget*                                      Actions        = nullptr;
		QVBoxLayout*                                  Rows           = nullptr;
		QWidget*                                      PublisherRow   = nullptr;
		QButtonGroup*                                 PublisherGroup = nullptr;
		QRadioButton*                                 BuiltIn        = nullptr;
		QRadioButton*                                 External       = nullptr;
		QWidget*                                      Footer         = nullptr;
		QComboBox*                                    Chooser        = nullptr;
		QLabel*                                       Revision       = nullptr;
		QPushButton*                                  Update         = nullptr;
		bool                                          Presenting     = false;
		bool                                          PublisherShown = false;
	};

	/// \brief What an action's state says, from the author's side, in full
	/// (a row's tooltip).
	[[nodiscard]] auto workflow_state_text(const application::local_draft_workflow_action_view& aAction) -> QString;
} // namespace bpr::qt

#endif
