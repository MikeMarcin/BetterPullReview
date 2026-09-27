#ifndef BPR_SRC_APPLICATION_SRC_REVIEW_SUBMISSION_EXECUTION_AUTHORITY_HPP
#define BPR_SRC_APPLICATION_SRC_REVIEW_SUBMISSION_EXECUTION_AUTHORITY_HPP

#include "bpr/application/review_submission_application_host.hpp"

#include <atomic>

namespace bpr::application::detail {
	/// \brief Issues only the exact normal authenticated user-submission request.
	/// \details The composition invokes this after accepting the core Submit
	/// command for the displayed draft and current authenticated PR binding.
	/// Test builds always refuse production issuance. This does not consume or
	/// interpret any source-line attended approval or grant.
	class review_submission_user_authority_issuer final
	{
	public:
		review_submission_user_authority_issuer() = delete;
		[[nodiscard]] static auto issue(core::application_session_id aSession,
										core::request_generation aGeneration, const review_submission_request& aRequest)
			-> review_submission_execution_grant;
		[[nodiscard]] static auto issue(core::application_session_id aSession, core::request_generation aGeneration, const staged_review_plan& aPlan) -> review_submission_execution_grant;
	};
}
namespace bpr::application {
	class review_submission_execution_authority final
	{
	public:
		~review_submission_execution_authority()                                                               = default;
		review_submission_execution_authority(const review_submission_execution_authority&)                    = delete;
		auto operator=(const review_submission_execution_authority&) -> review_submission_execution_authority& = delete;
		void revoke() noexcept;

	private:
		review_submission_execution_authority(core::application_session_id aSession,
											  core::request_generation aGeneration, review_submission_request aRequest,
											  std::chrono::steady_clock::time_point aExpiresAt);
		[[nodiscard]] auto                    consume(core::application_session_id aSession,
													  core::request_generation aGeneration, const review_submission_request& aRequest) -> bool;
		[[nodiscard]] auto                    consume(core::application_session_id aSession, core::request_generation aGeneration, const staged_review_plan& aPlan) -> bool;
		[[nodiscard]] auto                    consume_child(const staged_publication_record& aRecord) -> bool;
		[[nodiscard]] auto                    active() const noexcept -> bool;
		core::application_session_id          Session;
		core::request_generation              Generation;
		review_submission_request             Request;
		std::optional<staged_review_plan>     Plan;
		std::atomic_uint64_t                  LastChild{ 0 };
		std::chrono::steady_clock::time_point ExpiresAt;
		std::atomic_bool                      Consumed{ false };
		std::atomic_bool                      Revoked{ false };
		friend class review_submission_application_host;
		friend class detail::review_submission_user_authority_issuer;
#if defined(BPR_APPLICATION_TESTING)
		friend auto make_review_submission_testing_grant(core::application_session_id,
														 core::request_generation, const review_submission_request&,
														 std::chrono::steady_clock::time_point) -> review_submission_execution_grant;
		friend auto make_review_submission_testing_grant(core::application_session_id, core::request_generation, const staged_review_plan&, std::chrono::steady_clock::time_point) -> review_submission_execution_grant;
#endif
	};
}
#endif
