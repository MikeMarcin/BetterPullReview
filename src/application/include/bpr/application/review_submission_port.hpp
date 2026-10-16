#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_PORT_HPP

#include "bpr/application/review_submission_contract.hpp"
#include "bpr/application/staged_review_publication.hpp"
#include "bpr/core/review_submission_unknown.hpp"

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
		/// [[BPR-1540]]. Why an indeterminate result is unknown, as the host
		/// established it; none for any other result.
		std::optional<core::review_submission_unknown_diagnosis> Unknown;
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

		[[nodiscard]] virtual auto pending_tours(std::stop_token aStop = { }) -> std::expected<std::vector<staged_review_plan>, review_mutation_failure>
		{
			static_cast<void>(aStop);
			return std::unexpected(review_mutation_failure::unavailable);
		}
		[[nodiscard]] virtual auto publish_tour(const staged_review_plan& aPlan, std::stop_token aStop = { }, staged_child_authorizer aAuthorize = { }) -> staged_publication_result
		{
			static_cast<void>(aPlan);
			static_cast<void>(aStop);
			static_cast<void>(aAuthorize);
			return { };
		}
		[[nodiscard]] virtual auto reconcile_tour(const staged_review_plan& aPlan, std::stop_token aStop = { }) -> staged_publication_result
		{
			static_cast<void>(aPlan);
			static_cast<void>(aStop);
			return { };
		}
		/// \brief Discovers bounded original pending requests on the effect lane; includes prepared attempts.
		[[nodiscard]] virtual auto pending(std::stop_token aStop = { }) -> std::expected<std::vector<review_submission_request>, review_mutation_failure> = 0;

		[[nodiscard]] virtual auto submit_and_reconcile(const review_submission_request& aRequest, std::stop_token aStop = { }) -> review_submission_result = 0;
		/// \brief Reads only an already admitted attempt; never authorizes a new POST.
		/// \details A missing local row cannot establish definite nonpublication.

		[[nodiscard]] virtual auto reconcile(const review_submission_request& aRequest, std::stop_token aStop = { }) -> review_submission_result = 0;
		/// \brief [[BPR-1345]]. The request admitted under this key in this
		/// scope, resolved or not, or none when nothing was ever admitted
		/// under it. Read only: it never authorizes a POST.
		/// \details Admission precedes every transmission, so no admitted
		/// request is proof that nothing under this key was sent.
		/// \brief [[BPR-1541]]. Why the unresolved attempt under `aKey` was
		/// last left unknown, as its record keeps it; none when nothing is
		/// kept. Read only.
		[[nodiscard]] virtual auto last_unknown(const std::string& aKey, std::stop_token aStop = { }) -> std::optional<core::review_submission_unknown_diagnosis>
		{
			static_cast<void>(aKey);
			static_cast<void>(aStop);
			return std::nullopt;
		}
		[[nodiscard]] virtual auto admitted(const std::string& aKey, std::stop_token aStop = { })
			-> std::expected<std::optional<review_submission_request>, review_mutation_failure>
		{
			static_cast<void>(aKey);
			static_cast<void>(aStop);
			return std::unexpected(review_mutation_failure::unavailable);
		}
	};
}

#endif
