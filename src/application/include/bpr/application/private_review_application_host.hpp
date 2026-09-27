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
		core::private_run_outcome                            Outcome = core::private_run_outcome::admitted;
	};
	struct private_run_admission
	{
		agent_launch_options                         Launch;
		std::optional<core::private_review_revision> Prompt;
		private_review_delta                         Delta;
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
		/// \brief Call only after old producers are quiescent. One bounded recovery page, no rerun.
		[[nodiscard]] auto recover(std::optional<core::agent_run_id> aAfter = { }) -> private_run_result<private_run_page>;

	private:
		private_review_store_port& Store;
	};
}
#endif
