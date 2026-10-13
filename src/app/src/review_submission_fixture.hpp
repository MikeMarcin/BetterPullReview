#ifndef BPR_SRC_APP_SRC_REVIEW_SUBMISSION_FIXTURE_HPP
#define BPR_SRC_APP_SRC_REVIEW_SUBMISSION_FIXTURE_HPP
#if !defined(BPR_BUILD_TESTING)
	#error "Whole-review fixtures are test-build only"
#endif
#include "bpr/application/review_submission_application_host.hpp"
#include "bpr/application/review_submission_recovery_port.hpp"
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>

namespace bpr::app::detail {
	/// \brief Independent synthetic server state, reusable across reconstructed applications.
	struct review_submission_fixture_state
	{
		std::mutex Mutex;
		/// Strict synthetic operation admission; no real token or native tool is used.
		bool                                                CliStrictCourse = false;
		std::optional<application::review_mutation_failure> CliAcquisitionFailure;
		std::size_t                                         CliAcquisitions = 0U;
		std::function<void(std::size_t, std::string_view)>  Boundary;
		bool                                                DraftPullRequest = false;
		std::map<std::uint64_t, std::uint64_t>              ReplyParents;
		std::condition_variable_any                         StagedWait;
		std::optional<application::staged_review_entity>    StagedReview;
		std::vector<application::staged_review_entity>      PreviousReviews;
		std::vector<application::staged_review_comment>     PreviousComments;
		std::vector<std::string>                            StagedWritePaths;
		std::vector<application::staged_review_comment>     StagedComments, CompatibilityComments;
		std::optional<std::string>                          StagedDescription;
		/// Synthetic read-only check collections for composed Overview courses.
		std::optional<std::string>                          CheckRuns, CommitStatuses;
		std::optional<std::string>                          MergeAssessment, ReviewRequests, BranchRules, BranchProtection, ReviewHistory;
		std::size_t                                         StagedWrites = 0, StagedCreates = 0, StagedSubmits = 0;
		bool                                                StagedQualified = true, StagedConflict = false, StagedRejectDescription = false, StagedBlockNext = false, StagedBlocked = false;
		std::vector<application::review_submission_request> Reviews;
		bool                                                LoseNextResponse = false;
		bool                                                RefuseBeforeSend = false;
		bool                                                StaleNextResult  = false;
		std::size_t                                         Submissions      = 0U;
		std::size_t                                         Reconciliations  = 0U;
		/// [[BPR-1345]]. A review that carries line comments goes through the
		/// production session to this server: each POST's exact body, a 422
		/// for the next one, or the next one created with its answer lost.
		std::vector<std::string> ReviewPosts;
		bool                     RefuseNextReview = false;
		bool                     LoseNextReview   = false;
		/// The review list cannot be read: set when the next review's answer
		/// is lost, so its outcome is unknown until the reads come back.
		bool HideReviewReads = false;
	};
	[[nodiscard]] auto make_staged_review_fixture_provider(std::shared_ptr<review_submission_fixture_state>, std::unique_ptr<application::review_submission_recovery_port>, application::review_submission_live_binding, std::string) -> std::unique_ptr<application::review_submission_port>;
	[[nodiscard]] auto make_review_submission_fixture_provider(
		std::shared_ptr<review_submission_fixture_state>              aState,
		std::unique_ptr<application::review_submission_recovery_port> aStore,
		application::review_submission_live_binding                   aBinding) -> std::unique_ptr<application::review_submission_port>;
}
#endif
