#ifndef BPR_SRC_APP_SRC_REVIEW_SUBMISSION_WORKFLOW_HPP
#define BPR_SRC_APP_SRC_REVIEW_SUBMISSION_WORKFLOW_HPP

#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/review_preflight_observation.hpp"
#include "bpr/application/review_submission_application_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"

namespace bpr::app {
	/// Composes the PR-scoped core draft, asynchronous host and normal editor.
	/// All methods run on the decision lane; retain this object until quiescent.
	class review_submission_workflow final : public application::review_submission_projection_port
	{
	public:
		using grant_factory      = std::function<application::review_submission_execution_grant(core::application_session_id, core::request_generation, const application::review_submission_request&)>;
		using write_confirmation = std::function<bool(const application::review_submission_live_binding&, core::review_disposition)>;
		review_submission_workflow(application::effect_executor&, application::decision_executor&, qt::review_submission_editor&, grant_factory, write_confirmation = { });
		void               adopt(application::review_submission_live_binding, application::workbench_pull_request_authority, application::review_submission_provider_factory);
		void               invalidate(QString aReason = { });
		void               invalidate(const application::pull_request_authority_invalidation&);
		void               dispatch(qt::review_submission_editor_intent);
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const -> bool;
		void               publish(const application::review_submission_host_terminal&) override;

		/// [[BPR-1085]]. The published feedback of the accepted snapshot. It is
		/// context only: it never changes the draft, its disposition or its
		/// attempt, and never submits.
		void observe_preflight(application::review_preflight_observation);
		/// [[BPR-1085]]. My unsent comment drafts and where each is. Selected
		/// drafts bind a prepared decision.
		void observe_selection(std::vector<core::review_preflight_draft>, std::map<std::string, qt::review_submission_preflight_reference>);

	private:
		using preflight_key = std::pair<std::uint64_t, application::review_submission_live_binding>;
		struct selection
		{
			application::review_submission_live_binding     Binding;
			application::workbench_pull_request_authority   Authority;
			application::review_submission_provider_factory Factory;
		};
		void                                                                          retire();
		void                                                                          start_selection();
		void                                                                          new_draft();
		void                                                                          restore(application::review_submission_request);
		void                                                                          reconcile();
		void                                                                          present();
		void                                                                          complete(core::review_submission_command_kind);
		[[nodiscard]] auto                                                            decided_controls() const -> core::review_submission_controls;
		void                                                                          project_preflight();
		application::effect_executor&                                                 Effects;
		application::decision_executor&                                               Decisions;
		qt::review_submission_editor&                                                 Editor;
		grant_factory                                                                 IssueGrant;
		write_confirmation                                                            ConfirmWrite;
		std::unique_ptr<application::review_submission_application_host>              Host;
		std::vector<std::unique_ptr<application::review_submission_application_host>> Retired;
		std::optional<selection>                                                      Current;
		std::optional<selection>                                                      Next;
		std::optional<core::review_submission_draft>                                  Draft;
		std::optional<application::review_submission_live_binding>                    DraftBinding;
		std::optional<application::review_submission_request>                         Pending;
		std::optional<application::review_submission_host_context>                    Context;
		std::vector<application::review_submission_request>                           Unresolved;
		std::optional<application::review_preflight_observation>                      Observation;
		std::vector<core::review_preflight_draft>                                     Selection;
		std::map<std::string, qt::review_submission_preflight_reference>              SelectionReferences;
		std::optional<core::review_preflight>                                         Preflight;
		std::map<std::string, qt::review_submission_preflight_reference>              PreflightReferences;
		std::uint64_t                                                                 PreflightInputs = 0U;
		std::optional<preflight_key>                                                  PreflightProjected;
		core::request_generation                                                      Generation;
		std::uint64_t                                                                 NextDraft = 1U;
		QString                                                                       Status;
		bool                                                                          Ready   = false;
		bool                                                                          Closing = false;
	};
	/// Tour composition reuses the summary host, exact grants and journal session.
	class review_tour_publication_workflow final : public application::review_submission_projection_port
	{
	public:
		using grant_factory = std::function<application::review_submission_execution_grant(core::application_session_id, core::request_generation, const application::staged_review_plan&)>;
		using receipt_sink  = std::function<void(const application::staged_publication_record&)>;
		review_tour_publication_workflow(application::effect_executor&, application::decision_executor&, qt::review_tour_editor&, grant_factory, review_submission_workflow::write_confirmation = { }, receipt_sink = { });
		void               adopt(application::review_submission_live_binding, application::workbench_pull_request_authority, application::review_submission_provider_factory, std::string aDescription, bool aFixtureQualified = false);
		void               observe(const application::tour_presentation&);
		void               dispatch(qt::tour_editor_action);
		void               invalidate();
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const -> bool;
		void               publish(const application::review_submission_host_terminal&) override;

	private:
		void                                                                          prepare();
		void                                                                          present();
		application::effect_executor&                                                 Effects;
		application::decision_executor&                                               Decisions;
		qt::review_tour_editor&                                                       Editor;
		grant_factory                                                                 IssueGrant;
		review_submission_workflow::write_confirmation                                ConfirmWrite;
		std::unique_ptr<application::review_submission_application_host>              Host;
		std::vector<std::unique_ptr<application::review_submission_application_host>> Retired;
		std::optional<application::review_submission_live_binding>                    Binding;
		application::workbench_pull_request_authority                                 Authority;
		core::request_generation                                                      Generation;
		application::tour_presentation                                                Draft;
		std::optional<application::staged_review_plan>                                Plan;
		std::optional<application::staged_publication_record>                         Record;
		std::optional<application::review_submission_host_context>                    Context;
		application::staged_publication_state                                         State = application::staged_publication_state::unavailable;
		std::string                                                                   Description, Status = "Saved locally. Prepare a publication preview.";
		receipt_sink                                                                  ReceiptSink;
		bool                                                                          FixtureQualified = false, Closing = false, Previewed = false, Ready = false;
	};
}
#endif
