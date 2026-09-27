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
		std::mutex                                          Mutex;
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
		std::size_t                                         StagedWrites = 0, StagedCreates = 0, StagedSubmits = 0;
		bool                                                StagedQualified = true, StagedConflict = false, StagedRejectDescription = false, StagedBlockNext = false, StagedBlocked = false;
		std::vector<application::review_submission_request> Reviews;
		bool                                                LoseNextResponse = false;
		bool                                                RefuseBeforeSend = false;
		bool                                                StaleNextResult  = false;
		std::size_t                                         Submissions      = 0U;
		std::size_t                                         Reconciliations  = 0U;
	};
	[[nodiscard]] auto make_staged_review_fixture_provider(std::shared_ptr<review_submission_fixture_state>, std::unique_ptr<application::review_submission_recovery_port>, application::review_submission_live_binding, std::string) -> std::unique_ptr<application::review_submission_port>;
	[[nodiscard]] auto make_review_submission_fixture_provider(
		std::shared_ptr<review_submission_fixture_state>              aState,
		std::unique_ptr<application::review_submission_recovery_port> aStore,
		application::review_submission_live_binding                   aBinding) -> std::unique_ptr<application::review_submission_port>;
}
#endif
