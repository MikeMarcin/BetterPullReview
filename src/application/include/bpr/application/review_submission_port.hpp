#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_PORT_HPP

#include "bpr/application/review_submission_contract.hpp"

namespace bpr::application {

	/// \brief Distinguishes confirmed publication from safe refusal and unresolved transmission.
	enum class review_submission_publication : std::uint8_t { definitely_not_published,
															  indeterminate,
															  published };

	/// \brief Retains complete request correlation even when no receipt is available.
	struct review_submission_result
	{
		review_submission_request                Request;
		review_submission_publication            Publication = review_submission_publication::indeterminate;
		std::optional<review_submission_receipt> Receipt;
		review_mutation_failure                  Failure   = review_mutation_failure::unavailable;
		bool                                     Duplicate = false;
	};

	/// \brief Executes or reconciles one immutable whole-review attempt.
	class review_submission_port
	{
	public:
		review_submission_port()          = default;
		virtual ~review_submission_port() = default;

		review_submission_port(const review_submission_port&)                    = delete;
		auto operator=(const review_submission_port&) -> review_submission_port& = delete;
		review_submission_port(review_submission_port&&)                         = delete;
		auto operator=(review_submission_port&&) -> review_submission_port&      = delete;

		[[nodiscard]] virtual auto submit_and_reconcile(const review_submission_request& aRequest, std::stop_token aStop = { }) -> review_submission_result = 0;
		/// \brief Reads only an already admitted attempt; never authorizes a new POST.

		[[nodiscard]] virtual auto reconcile(const review_submission_request& aRequest, std::stop_token aStop = { }) -> review_submission_result = 0;
	};
}

#endif
