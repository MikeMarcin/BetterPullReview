#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_SUBMISSION_PROVIDER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_SUBMISSION_PROVIDER_HPP

#include "review_submission_session.hpp"

#include "bpr/application/review_mutation_port.hpp"

#include <expected>
#include <memory>
#include <stop_token>

namespace bpr::adapters::github {
	/// \brief Implements ordinary anchor-free whole-review submission.
	///
	/// \details The provider owns no endpoint, credential, or network manager.
	/// Its immutable configuration and narrow transport are supplied by the
	/// composition root; every submit re-observes the exact target and acting
	/// identity immediately before the transport may perform a write.
	class live_review_submission_provider final
		: public application::review_submission_port
	{
	public:
		~live_review_submission_provider() override = default;

		live_review_submission_provider(const live_review_submission_provider&) = delete;
		auto operator=(const live_review_submission_provider&)
			-> live_review_submission_provider&                            = delete;
		live_review_submission_provider(live_review_submission_provider&&) = delete;
		auto operator=(live_review_submission_provider&&)
			-> live_review_submission_provider& = delete;

		[[nodiscard]] auto pending(std::stop_token aStop = { }) -> std::expected<std::vector<application::review_submission_request>, application::review_mutation_failure> override;
		[[nodiscard]] auto submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop = { }) -> application::review_submission_result override;
		[[nodiscard]] auto reconcile(const application::review_submission_request& aRequest, std::stop_token aStop = { }) -> application::review_submission_result override;
		[[nodiscard]] auto admitted(const std::string& aKey, std::stop_token aStop = { }) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure> override;

		[[nodiscard]] auto pending_tours(std::stop_token aStop = { }) -> std::expected<std::vector<application::staged_review_plan>, application::review_mutation_failure> override;
		[[nodiscard]] auto publish_tour(const application::staged_review_plan& aPlan, std::stop_token aStop = { }, application::staged_child_authorizer aAuthorize = { }) -> application::staged_publication_result override;
		[[nodiscard]] auto reconcile_tour(const application::staged_review_plan& aPlan, std::stop_token aStop = { }) -> application::staged_publication_result override;

	private:
		friend auto make_live_review_submission_provider(
			application::review_mutation_transport       aLease,
			application::review_mutation_target_binding  aTarget,
			application::review_mutation_acting_identity aActingIdentity,
			std::unique_ptr<application::review_submission_recovery_port>
															  aRecovery,
			std::shared_ptr<application::diagnostic_log_port> aLog) noexcept
			-> std::expected<
				std::unique_ptr<live_review_submission_provider>,
				application::review_mutation_failure>;

		live_review_submission_provider(
			application::review_mutation_transport     aLease,
			detail::github_mutation_wire_transport&    aTransport,
			detail::live_review_mutation_configuration aConfiguration,
			std::unique_ptr<application::review_submission_recovery_port>
															  aRecovery,
			std::shared_ptr<application::diagnostic_log_port> aLog) noexcept;

		detail::review_submission_session Session;
	};

	/// \brief Constructs the contracted live provider for one fixed authority.
	/// `aLog`, when given, receives why a submission stays unknown
	/// ([[BPR-1540]]).
	[[nodiscard]] auto make_live_review_submission_provider(
		application::review_mutation_transport       aLease,
		application::review_mutation_target_binding  aTarget,
		application::review_mutation_acting_identity aActingIdentity,
		std::unique_ptr<application::review_submission_recovery_port>
														  aRecovery,
		std::shared_ptr<application::diagnostic_log_port> aLog = { }) noexcept
		-> std::expected<
			std::unique_ptr<live_review_submission_provider>,
			application::review_mutation_failure>;
} // namespace bpr::adapters::github

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_SUBMISSION_PROVIDER_HPP
