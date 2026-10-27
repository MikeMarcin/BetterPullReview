#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_REVIEW_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_REVIEW_APPLICATION_HOST_HPP
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/private_review_store.hpp"
namespace bpr::application {
	/// \brief Exact user selections; no caller-supplied private body crosses this boundary.
	struct private_run_intent
	{
		core::private_review_binding                  Binding;
		std::optional<core::private_review_thread_id> Thread;
		std::uint64_t                                 ExpectedThreadVersion = 0;
		core::private_run_purpose                     Purpose               = core::private_run_purpose::conversation;
		std::vector<core::private_source_selection>   Sources;
		std::optional<core::review_composition_id>    Composition;
		std::uint64_t                                 ExpectedCompositionVersion                   = 0;
		auto                                          operator<=>(const private_run_intent&) const = default;
	};
	struct private_run_error
	{
		std::optional<core::private_review_refusal> Policy;
		std::optional<private_store_failure>        Store;
	};
	template <typename T>
	using private_run_result = std::expected<T, private_run_error>;
	/// \brief Changed identities only. Full transcripts are queried by bounded store pages.
	struct private_review_delta
	{
		core::private_review_scope                           Scope;
		std::optional<core::private_review_thread_id>        Thread;
		std::optional<core::private_review_turn_revision_id> Revision;
		core::agent_run_id                                   Run;
		std::vector<core::private_source_reference>          Findings;
		std::optional<core::private_source_reference>        Candidate;
		core::private_run_outcome                            Outcome = core::private_run_outcome::admitted;
	};
	/// \brief One comment-workshop press, recorded ([[BPR-1245]]).
	/// `Run` asks with the recorded revision as its only source; `Thread` is
	/// where Private history shows it.
	struct private_workshop_press
	{
		private_run_intent             Run;
		core::private_review_thread_id Thread;
	};
	struct private_run_admission
	{
		agent_launch_options                         Launch;
		std::optional<core::private_review_revision> Prompt;
		private_review_delta                         Delta;
		std::optional<private_composition_record>    Composition;
	};
	/// \brief Called only from the invocation's effect lane. It owns no process or mutation port.
	class private_agent_run_port
	{
	public:
		virtual ~private_agent_run_port()                                                                                                                                                                                                    = default;
		[[nodiscard]] virtual auto admit(const private_run_intent&, const core::agent_profile&, const core::agent_session&, const agent_context_observation&, const core::agent_setting_facts&) -> private_run_result<private_run_admission> = 0;
		[[nodiscard]] virtual auto claim(const private_run_admission&, std::stop_token) -> private_run_result<std::shared_ptr<agent_input_claim>>                                                                                            = 0;
		[[nodiscard]] virtual auto finish(const private_run_admission&, const agent_run_result&, const core::agent_finding_snapshot&) -> private_run_result<private_review_delta>                                                            = 0;
	};
	/// \brief Durable local conversation orchestration; the invocation host remains the sole launcher.
	/// Composition shares this run boundary; accepting/publishing belongs to its existing owners.
	class private_review_application_host final : public private_agent_run_port
	{
	public:
		explicit private_review_application_host(private_review_store_port& aStore)
			: Store(aStore)
		{ }
		[[nodiscard]] auto admit(const private_run_intent&, const core::agent_profile&, const core::agent_session&, const agent_context_observation&, const core::agent_setting_facts&) -> private_run_result<private_run_admission> override;
		[[nodiscard]] auto claim(const private_run_admission&, std::stop_token) -> private_run_result<std::shared_ptr<agent_input_claim>> override;
		[[nodiscard]] auto finish(const private_run_admission&, const agent_run_result&, const core::agent_finding_snapshot&) -> private_run_result<private_review_delta> override;
		/// \brief Explicit reviewer reply retains the exact finding as the thread subject.
		[[nodiscard]] auto reply_to_finding(const core::private_source_selection&, std::string aReviewerBody) -> private_run_result<private_review_delta>;
		/// \brief RFC-BPR-0012's workshop press under RFC-BPR-0023 ([[BPR-1245]]).
		/// Records the stored draft's text as one new immutable reviewer revision on a thread
		/// about the draft's line, and returns the findings run that asks about it alone. The
		/// draft is read from the store, so an edit committed before the press is the text
		/// recorded. A draft of another review, not drafting, or empty is refused.
		[[nodiscard]] auto record_workshop_press(const core::private_review_binding&, core::review_draft_id, const core::diff_semantic_index&) -> private_run_result<private_workshop_press>;
		/// \brief [[BPR-1773]]. Ask agent about a private note: its text as
		/// it is now becomes one immutable reviewer revision on a new private
		/// thread about the pull request, and the run's only source. The note
		/// itself, which can still change, is never sent; its file and line
		/// travel as the turn's conversation, whatever comparison the note was
		/// written against.
		[[nodiscard]] auto record_note_press(const core::private_review_binding&, std::string aBody) -> private_run_result<private_workshop_press>;
		/// \brief Call only after old producers are quiescent. One bounded recovery page, no rerun.
		[[nodiscard]] auto recover(std::optional<core::agent_run_id> aAfter = { }) -> private_run_result<private_run_page>;

	private:
		private_review_store_port& Store;
	};
}
#endif
