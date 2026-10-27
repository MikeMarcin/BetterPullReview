#ifndef BPR_SRC_APP_SRC_PRIVATE_REVIEW_WORKFLOW_HPP
#define BPR_SRC_APP_SRC_PRIVATE_REVIEW_WORKFLOW_HPP
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/private_publication_application_host.hpp"
#include "bpr/application/private_review_application_host.hpp"
#include "bpr/qt/private_composition_editor.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include <QPointer>
#include <mutex>
#include <vector>
namespace bpr::app {
	/// Shared by profile windows on their serialized private-store effect lane.
	struct private_review_recovery_registry
	{
		std::vector<core::private_review_scope> Scopes;
	};
	/// Composes private storage on the mutation effect lane and returns typed results to decisions.
	/// Keep alive until the invocation host, mutation providers and pending operations are quiescent.
	/// Every store operation holds the profile's review-ledger file exclusion, the one its ledger
	/// store tasks hold, because both open and use the same SQLite file ([[BPR-1254]]).
	class private_review_workflow final : public application::private_agent_run_port, public application::review_publication_lineage_port
	{
	public:
		using draft_sink       = std::function<void(const application::durable_draft_commit&, bool)>;
		using run_sink         = std::function<bool(core::agent_profile_id, application::private_run_intent, core::agent_setting_facts, std::string)>;
		using draft_completion = std::function<void(application::private_run_result<application::durable_draft_commit>)>;
		using press_completion = std::function<void(application::private_run_result<application::private_workshop_press>)>;
		private_review_workflow(application::effect_executor&, application::decision_executor&, std::mutex& aLedgerFile, private_review_recovery_registry&, qt::private_review_panel&, QWidget*, draft_sink, run_sink);
		~private_review_workflow() override;
		void open(adapters::sqlite::private_review_configuration, std::function<void()>);
		void observe(core::private_review_binding, std::shared_ptr<const core::diff_semantic_index>, std::optional<core::source_line_ref>);
		void profiles(std::span<const core::agent_profile>);
		void dispatch(application::private_conversation_intent);
		void compose(application::private_composition_intent);
		void changed(const application::private_review_delta&);
		void invalidate();
		void shutdown();
		auto is_quiescent() const noexcept -> bool;
		auto binding() const -> std::optional<core::private_review_binding>;
		auto default_run(const core::agent_revision_binding&, bool aConversation) const -> std::optional<application::private_run_intent>;
		void restore(draft_completion);
		void select_draft(core::source_line_ref, draft_completion);
		void select_reply(core::source_line_ref, std::string, draft_completion);
		/// [[BPR-1745]]. Opens the outdated thread's reply, bound to the
		/// thread, under the bound comparison.
		void select_thread_reply(std::string, draft_completion);
		void change_draft(core::revisioned_review_draft_command, draft_completion);
		void materialize_draft(core::revisioned_review_draft, core::revisioned_review_draft_command, draft_completion);
		/// [[BPR-1345]]. Every command in one store transaction, or none.
		void change_drafts(std::vector<core::revisioned_review_draft_command>, draft_completion);
		/// \brief [[BPR-1733]]. A kept suggestion written on another head of
		/// this pull request than the one bound, and that head.
		struct earlier_suggestion
		{
			core::revisioned_review_draft Draft;
			core::git_commit_oid          Head;
		};
		using earlier_completion = std::function<void(std::vector<earlier_suggestion>)>;
		/// \brief [[BPR-1733]]. Every kept suggestion of this pull request on
		/// another head than the one bound: a draft still being written whose
		/// body is one suggestion (`core::decide_suggestion_head_move`). Read on
		/// the store's lane; answers on the decision lane, with none when the
		/// store refuses or the binding moved meanwhile.
		void earlier_suggestions(earlier_completion);
		/// \brief [[BPR-1733]]. Deletes one of them at the revision shown, once
		/// it is carried. Answers on the decision lane whether it was deleted.
		void discard_earlier_suggestion(core::review_draft_id, std::uint64_t aRevision, std::function<void(bool)>);
		void settle_review_comments(std::string aKey, bool aPublished, std::optional<core::review_submission_proposal>);
		/// [[BPR-1245]]. Records the draft as it stands on this lane, after every draft change
		/// already queued, as one reviewer revision that History then shows; answers on the decision lane.
		void record_workshop_press(core::review_draft_id, press_completion);
		/// \brief [[BPR-1773]]. Records a note's text as the run's source.
		void record_note_press(std::string, press_completion);
		auto admit(const application::private_run_intent&, const core::agent_profile&, const core::agent_session&, const application::agent_context_observation&, const core::agent_setting_facts&) -> application::private_run_result<application::private_run_admission> override;
		auto claim(const application::private_run_admission&, std::stop_token) -> application::private_run_result<std::shared_ptr<application::agent_input_claim>> override;
		auto finish(const application::private_run_admission&, const application::agent_run_result&, const core::agent_finding_snapshot&) -> application::private_run_result<application::private_review_delta> override;
		auto prepare(const application::review_mutation_request&, std::string_view, std::string_view) -> bool override;
		auto complete(const application::review_mutation_request&, std::string_view, std::string_view, const application::review_mutation_receipt&) -> bool override;
		auto pending(const core::review_mutation_target&, core::github_account_id) -> std::vector<application::private_publication_record> override;
		auto complete_observed(const application::private_publication_record&, std::string_view, const application::review_mutation_receipt&) -> bool override;

	private:
		struct context;
		struct result;
		using operation = std::function<result(context&)>;
		void                                                        post(operation, draft_completion = { });
		void                                                        refresh();
		auto                                                        find(const core::private_review_scope&) -> std::shared_ptr<context>;
		auto                                                        publication_context(const application::review_mutation_request&) -> std::shared_ptr<context>;
		application::effect_executor&                               Effects;
		application::decision_executor&                             Decisions;
		private_review_recovery_registry&                           Recovery;
		qt::private_review_panel&                                   Panel;
		QPointer<qt::private_composition_editor>                    Editor;
		draft_sink                                                  Drafts;
		run_sink                                                    Launch;
		std::mutex&                                                 Gate;
		std::vector<std::shared_ptr<context>>                       Contexts;
		std::shared_ptr<context>                                    Current;
		std::optional<core::private_review_binding>                 Binding;
		std::vector<core::private_review_binding>                   KnownBindings;
		std::shared_ptr<const core::diff_semantic_index>            Index;
		std::optional<core::source_line_ref>                        SelectedLine;
		std::vector<std::pair<core::agent_profile_id, std::string>> Profiles;
		std::uint64_t                                               Generation = 0;
		std::size_t                                                 Pending    = 0;
		bool                                                        Closing    = false;
	};
}
#endif
