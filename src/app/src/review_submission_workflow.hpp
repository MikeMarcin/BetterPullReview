#ifndef BPR_SRC_APP_SRC_REVIEW_SUBMISSION_WORKFLOW_HPP
#define BPR_SRC_APP_SRC_REVIEW_SUBMISSION_WORKFLOW_HPP

#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/review_preflight_observation.hpp"
#include "bpr/application/review_submission_application_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <tuple>

namespace bpr::app {
	/// Composes the PR-scoped core draft, asynchronous host and normal editor.
	/// All methods run on the decision lane; retain this object until quiescent.
	class review_submission_workflow final : public application::review_submission_projection_port
	{
	public:
		using grant_factory         = std::function<application::review_submission_execution_grant(core::application_session_id, core::request_generation, const application::review_submission_request&)>;
		using publication_admission = std::function<std::expected<void, application::review_mutation_failure>(const application::review_submission_live_binding&)>;
		review_submission_workflow(application::effect_executor&, application::decision_executor&, qt::review_submission_editor&, grant_factory, publication_admission = { });
		void               adopt(application::review_submission_live_binding, application::workbench_pull_request_authority, application::review_submission_provider_factory, application::insights_review_capture = { });
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

		/// [[BPR-1345]]. Holds the carried drafts for one submission, durably,
		/// before anything is sent: each pending draft moves to publishing
		/// with the request's idempotency key. Answers true only when every
		/// one is held at the exact revision the request carries.
		using comment_lock = std::function<void(const application::review_submission_request&, std::function<void(bool)>)>;
		/// [[BPR-1345]]. What GitHub's answer means for the drafts a
		/// submission under this key holds: published completes them,
		/// definitely not published releases them to pending again. The
		/// request is absent when nothing was ever admitted under the key.
		using review_answer = std::function<void(const std::string&, application::review_submission_publication, const std::optional<application::review_submission_request>&)>;
		/// [[BPR-1345]] v002. Discard review: deletes the pending drafts the
		/// review carries. [[BPR-1715]]: only those, each at the revision
		/// carried and pending at the review's head.
		using review_discard = std::function<void(const std::vector<core::review_submission_comment>&, const core::git_commit_oid&)>;
		void set_review_comment_hooks(comment_lock, review_answer, review_discard);
		/// [[BPR-1345]]. The pending drafts this review carries, as the draft
		/// owner holds them now, for the review's account and head.
		void observe_review_comments(std::vector<core::review_submission_comment> aComments);
		/// [[BPR-1345]]. The keys of submissions that hold drafts. Each one
		/// this workflow is not already settling is looked up, and its answer
		/// settles its drafts.
		void resolve_held(std::vector<std::string> aKeys);
		/// \brief [[BPR-1735]]. The carried drafts the core placed outside
		/// GitHub's diff: GitHub refuses a review carrying one, so Submit is
		/// refused while any is carried.
		void hold_comments(std::vector<core::review_draft_id> aDrafts);
		/// [[BPR-1475]]. A review GitHub confirmed published, on its answer or
		/// on Check status. Called whether or not it carried comments, so the
		/// window can read the pull request again (owner, 2026-10-13).
		using publication_observer = std::function<void()>;
		void set_publication_observer(publication_observer);
		/// \brief [[BPR-1736]]. A review GitHub refused, settled as not
		/// published, whose refusal names a line GitHub can't place: the
		/// composition demotes a learned fact that let it carry a comment
		/// outside the diff, and the page then holds that comment.
		using unplaceable_refusal_observer = std::function<void(const application::review_submission_request&)>;
		void set_unplaceable_refusal_observer(unplaceable_refusal_observer);

	private:
		/// The inputs a projected preflight was read from: the selection and
		/// observation generation, the binding, and the review's carried
		/// comments ([[BPR-1605]]).
		using preflight_key = std::tuple<std::uint64_t, application::review_submission_live_binding, std::vector<core::review_submission_comment>>;
		struct selection
		{
			application::review_submission_live_binding     Binding;
			application::workbench_pull_request_authority   Authority;
			application::review_submission_provider_factory Factory;
			application::insights_review_capture            Insights;
		};
		void retire();
		void start_selection();
		void new_draft();
		/// [[BPR-1345]] v003. After a confirmed or discarded review: the next
		/// unfinished one if any, else a new draft. Nothing waits for New review.
		void                                                                          next_review();
		void                                                                          restore(application::review_submission_request);
		void                                                                          reconcile();
		void                                                                          present();
		void                                                                          complete(core::review_submission_command_kind);
		[[nodiscard]] auto                                                            decided_controls() const -> core::review_submission_controls;
		void                                                                          project_preflight();
		void                                                                          apply_review_comments();
		void                                                                          send(application::review_submission_request aRequest);
		void                                                                          answer(const application::review_submission_request& aRequest, application::review_submission_publication aPublication);
		void                                                                          next_lookup();
		application::effect_executor&                                                 Effects;
		application::decision_executor&                                               Decisions;
		qt::review_submission_editor&                                                 Editor;
		grant_factory                                                                 IssueGrant;
		publication_admission                                                         AdmitPublication;
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
		/// [[BPR-1345]]. Start a review.
		comment_lock                                          LockComments;
		review_answer                                         Answer;
		review_discard                                        DiscardReview;
		std::vector<core::review_submission_comment>          Comments;
		core::review_submission_rejection                     CommentsRefusal = core::review_submission_rejection::none;
		bool                                                  Locking         = false;
		std::vector<std::string>                              Lookups;
		std::vector<core::review_draft_id>                    UnplaceableDrafts;
		std::vector<std::string>                              LookedUp;
		std::optional<std::string>                            LookupKey;
		std::optional<application::review_submission_request> Recovering;
		publication_observer                                  PublicationObserved;
		unplaceable_refusal_observer                          UnplaceableRefusalObserved;
		/// [[BPR-1541]]. Why the shown submission is still unknown, from its
		/// last Check status or, after a restart, its record; and what each
		/// discovered attempt's record kept.
		std::optional<core::review_submission_unknown_diagnosis>         Unknown;
		std::map<std::string, core::review_submission_unknown_diagnosis> KeptUnknown;
	};
	/// Tour composition reuses the summary host, exact grants and journal session.
	class review_tour_publication_workflow final : public application::review_submission_projection_port
	{
	public:
		using grant_factory = std::function<application::review_submission_execution_grant(core::application_session_id, core::request_generation, const application::staged_review_plan&)>;
		using receipt_sink  = std::function<void(const application::staged_publication_record&)>;
		review_tour_publication_workflow(application::effect_executor&, application::decision_executor&, qt::review_tour_editor&, grant_factory, review_submission_workflow::publication_admission = { }, receipt_sink = { });
		void               adopt(application::review_submission_live_binding, application::workbench_pull_request_authority, application::review_submission_provider_factory, std::string aDescription, bool aQualified = false, application::insights_review_capture = { });
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
		review_submission_workflow::publication_admission                             AdmitPublication;
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
		bool                                                                          Qualified = false, Closing = false, Previewed = false, Ready = false;
		/// [[BPR-1782]]. The active effect is a Publish or Resume; a publication
		/// answered since the last fresh preview without being published.
		bool Publishing = false, Attempted = false;
	};
	/// PR-level replies share the existing witnessed publication host and journal.
	class conversation_reply_workflow final : public application::review_submission_projection_port
	{
	public:
		using grant_factory = review_tour_publication_workflow::grant_factory;
		conversation_reply_workflow(application::effect_executor&, application::decision_executor&, qt::workbench_window&, grant_factory, review_submission_workflow::publication_admission, std::function<void()>);
		void               adopt(application::review_submission_live_binding, application::workbench_pull_request_authority, core::bpr_profile_id, application::review_submission_provider_factory, application::insights_review_capture = { });
		void               dispatch(core::conversation_reply_action, core::conversation_reply_draft);
		void               invalidate();
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const -> bool;
		void               publish(const application::review_submission_host_terminal&) override;

	private:
		void                                                                          pump();
		void                                                                          present();
		application::effect_executor&                                                 Effects;
		application::decision_executor&                                               Decisions;
		qt::workbench_window&                                                         Window;
		grant_factory                                                                 IssueGrant;
		review_submission_workflow::publication_admission                             Admit;
		std::function<void()>                                                         Published;
		std::unique_ptr<application::review_submission_application_host>              Host;
		std::vector<std::unique_ptr<application::review_submission_application_host>> Retired;
		std::optional<application::review_submission_live_binding>                    Binding;
		application::workbench_pull_request_authority                                 Authority;
		core::bpr_profile_id                                                          Profile;
		core::request_generation                                                      Generation;
		std::optional<application::review_submission_host_context>                    Context;
		std::map<std::string, application::staged_review_plan>                        Plans;
		std::map<std::string, application::staged_publication_record>                 Records;
		std::optional<core::conversation_reply_draft>                                 Desired;
		std::map<std::string, core::conversation_reply_draft>                         PendingEdits;
		bool                                                                          ComposerOpen = false;
		std::optional<application::staged_review_plan>                                Saving;
		std::optional<std::string>                                                    SendText;
		QString                                                                       Status;
		bool                                                                          Ready = false, Closing = false, Sending = false, Failed = false;
	};
}
#endif
