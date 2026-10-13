#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_SUBMISSION_SESSION_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_SUBMISSION_SESSION_HPP
#include "bpr/application/review_submission_port.hpp"
#include "bpr/application/review_submission_recovery_port.hpp"
#include "github_mutation_contract.hpp"
namespace bpr::adapters::github::detail {
	/// \brief Executes the production protocol with immutable authority and durable one-POST admission.
	class review_submission_session final
	{
	public:
		review_submission_session(github_mutation_wire_transport& aTransport, live_review_mutation_configuration aConfiguration, application::review_mutation_transport aLease, std::unique_ptr<application::review_submission_recovery_port> aRecovery);
		[[nodiscard]] auto pending(std::stop_token aStop = { }) -> std::expected<std::vector<application::review_submission_request>, application::review_mutation_failure>;
		[[nodiscard]] auto submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop = { }) -> application::review_submission_result;
		[[nodiscard]] auto reconcile(const application::review_submission_request& aRequest, std::stop_token aStop = { }) -> application::review_submission_result;
		/// \brief [[BPR-1345]]. The request admitted under `aKey` in this
		/// scope, or none. Reads the journal only.
		[[nodiscard]] auto admitted(const std::string& aKey, std::stop_token aStop = { }) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure>;

		[[nodiscard]] auto pending_tours(std::stop_token aStop = { }) -> std::expected<std::vector<application::staged_review_plan>, application::review_mutation_failure>;
		[[nodiscard]] auto publish_tour(const application::staged_review_plan& aPlan, std::stop_token aStop = { }, application::staged_child_authorizer aAuthorize = { }) -> application::staged_publication_result;
		[[nodiscard]] auto reconcile_tour(const application::staged_review_plan& aPlan, std::stop_token aStop = { }) -> application::staged_publication_result;

	private:
		[[nodiscard]] auto execute(const application::review_submission_request& aRequest, std::stop_token aStop, bool aAllowNew) -> application::review_submission_result;
		[[nodiscard]] auto perform(review_mutation_wire_request aWire, std::stop_token aStop, github_mutation_transmission_witness aWitness = { }) -> github_mutation_wire_completion;
		[[nodiscard]] auto observe_exact(std::stop_token aStop, bool aHistorical = false) -> std::expected<void, application::review_mutation_failure>;
		[[nodiscard]] auto observe_reviews(const application::review_submission_request& aRequest, std::stop_token aStop) -> review_mutation_parse_result<review_submission_reconciliation>;
		[[nodiscard]] auto finish_known(const application::review_submission_recovery_record& aRecord, std::stop_token aStop, bool aDuplicate) -> application::review_submission_result;
		[[nodiscard]] auto resolve_observed(const application::review_submission_recovery_record& aRecord, const review_submission_reconciliation& aObserved, bool aDuplicate, std::stop_token aStop) -> application::review_submission_result;
		/// \brief [[BPR-1345]]. Whether the review GitHub lists as new holds
		/// exactly the line comments the request carried.
		[[nodiscard]] auto carries_exactly(const application::review_submission_request& aRequest, application::review_database_id aReview, std::stop_token aStop) -> bool;
		/// \brief [[BPR-1367]]. Every page of the pull request's review comment
		/// collection, at most `maximum_review_comment_collection_pages`.
		[[nodiscard]] auto observe_comment_collection(std::stop_token aStop) -> review_mutation_parse_result<std::vector<review_comment_collection_entry>>;
		[[nodiscard]] auto observe_staged(application::staged_review_request aRequest, std::stop_token aStop) -> review_mutation_parse_result<application::staged_review_observation>;
		/// \brief [[BPR-1367]]. A tour review's comments as GitHub lists them,
		/// and whether that list reports their lines.
		struct staged_comments
		{
			std::vector<application::staged_review_comment> Comments;
			/// A submitted review's comments come from the pull request's
			/// collection, with lines and sides. GitHub lists a pending
			/// review's comments only in the review's own list, which has none:
			/// those have no source.
			bool Lines = false;
		};
		/// \brief [[BPR-1367]]. The comments of `aReview`, chosen by its state.
		/// A submitted review's are read from `aCollection` when given, which
		/// is read once and kept there.
		[[nodiscard]] auto observe_staged_comments(const application::staged_publication_record& aRecord, const application::staged_review_entity& aReview, std::stop_token aStop, std::optional<std::vector<review_comment_collection_entry>>* aCollection = nullptr) -> review_mutation_parse_result<staged_comments>;
		[[nodiscard]] auto reconcile_staged(application::staged_publication_record& aRecord, std::stop_token aStop) -> application::staged_publication_state;
		[[nodiscard]] auto verify_tour_update(const application::staged_publication_record&, std::stop_token) -> bool;
		[[nodiscard]] auto verify_staged(const application::staged_publication_record& aRecord, std::stop_token aStop) -> bool;
		[[nodiscard]] auto execute_tour(const application::staged_review_plan& aPlan, std::stop_token aStop, bool aWrite, application::staged_child_authorizer aAuthorize = { }) -> application::staged_publication_result;

		github_mutation_wire_transport*                               Transport;
		const live_review_mutation_configuration                      Configuration;
		application::review_mutation_transport                        Lease;
		std::unique_ptr<application::review_submission_recovery_port> Recovery;
		std::mutex                                                    Mutex;
	};
}
#endif
