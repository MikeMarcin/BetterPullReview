#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_APPLICATION_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_submission_port.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <variant>

namespace bpr::application {
	class review_submission_execution_authority;
	using review_submission_execution_grant = std::shared_ptr<review_submission_execution_authority>;
	using staged_review_discovery           = std::expected<std::vector<staged_review_plan>, review_mutation_failure>;
	using review_submission_discovery       = std::expected<std::vector<review_submission_request>, review_mutation_failure>;
	/// \brief Creates and opens provider storage only on the effect executor.
	using review_submission_provider_factory = std::function<std::expected<std::unique_ptr<review_submission_port>, review_mutation_failure>()>;

	struct review_submission_host_context
	{
		core::request_id             Request;
		core::application_session_id Session;
		core::request_generation     Generation;
		auto                         operator<=>(const review_submission_host_context&) const = default;
	};
	/// \brief [[BPR-1345]]. What was admitted under one submission key.
	struct review_submission_lookup
	{
		std::string                                                                      Key;
		std::expected<std::optional<review_submission_request>, review_mutation_failure> Request = std::unexpected(review_mutation_failure::unavailable);
	};
	struct review_submission_host_terminal
	{
		review_submission_host_context                                                                                                                    Context;
		std::variant<review_submission_discovery, review_submission_result, staged_review_discovery, staged_publication_result, review_submission_lookup> Outcome;
		/// [[BPR-1541]]. With a discovery: why each discovered attempt was
		/// last left unknown, by its key, as its record keeps it.
		std::map<std::string, core::review_submission_unknown_diagnosis> Unknown = { };
	};
	class review_submission_projection_port
	{
	public:
		virtual ~review_submission_projection_port()                           = default;
		virtual void publish(const review_submission_host_terminal& aTerminal) = 0;
	};

#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
	[[nodiscard]] auto make_review_submission_testing_grant(
		core::application_session_id aSession, core::request_generation aGeneration,
		const review_submission_request&      aRequest,
		std::chrono::steady_clock::time_point aExpiresAt = std::chrono::steady_clock::time_point::max()) -> review_submission_execution_grant;
	[[nodiscard]] auto make_review_submission_testing_grant(core::application_session_id aSession, core::request_generation aGeneration, const staged_review_plan& aPlan, std::chrono::steady_clock::time_point aExpiresAt = std::chrono::steady_clock::time_point::max()) -> review_submission_execution_grant;
#endif

	/// \brief Owns serialized whole-review effects and exactly-once decision-lane terminals.
	/// \details Calls run on the decision lane. A returned context is accepted
	/// work and always reaches one terminal, even during cancellation/shutdown.
	/// A busy, closed or unauthorized call returns no context and queues nothing.
	/// Keep the projection and decision executor alive until quiescent. Provider
	/// construction, pending discovery and reconciliation all run off-lane.
	class review_submission_application_host final
	{
	public:
		review_submission_application_host(review_submission_provider_factory aFactory,
										   review_submission_live_binding aBinding, core::application_session_id aSession,
										   core::request_generation aGeneration, effect_executor& aEffects,
										   decision_executor& aDecisions, review_submission_projection_port& aProjection);
		~review_submission_application_host();
		review_submission_application_host(const review_submission_application_host&)                    = delete;
		auto operator=(const review_submission_application_host&) -> review_submission_application_host& = delete;

		[[nodiscard]] auto discover() -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto submit(const review_submission_request& aRequest, review_submission_execution_grant aGrant) -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto reconcile(const review_submission_request& aRequest) -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto discover_conversation_replies() -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto save_conversation_reply(const staged_review_plan& aPlan) -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto discover_tours() -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto publish_tour(const staged_review_plan& aPlan, review_submission_execution_grant aGrant) -> std::optional<review_submission_host_context>;
		[[nodiscard]] auto reconcile_tour(const staged_review_plan& aPlan) -> std::optional<review_submission_host_context>;
		/// \brief [[BPR-1345]]. Reads what was admitted under `aKey`; never
		/// submits. A found request is reconciled with `reconcile`.
		[[nodiscard]] auto lookup(std::string aKey) -> std::optional<review_submission_host_context>;
		void               cancel(review_submission_host_context aContext) noexcept;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
