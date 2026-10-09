#ifndef BPR_APPLICATION_STAGED_REVIEW_CONTRACT_HPP
#define BPR_APPLICATION_STAGED_REVIEW_CONTRACT_HPP

#include "bpr/application/review_submission_contract.hpp"
#include "bpr/core/review_tour.hpp"

namespace bpr::application {
	/// Finite tour operations. No caller-supplied endpoint, method or GraphQL document.
	enum class staged_review_operation : std::uint8_t {
		list_reviews,
		read_review,
		list_comments,
		create_pending,
		update_comment,
		update_summary,
		submit_comment,
		discard_pending,
		update_description,
		list_issue_comments,
		create_issue_comment,
		update_issue_comment,
		/// [[BPR-1368]]. A stop's body edited while its review is pending.
		/// GitHub's REST `PATCH /pulls/comments/{id}` answers 404 for such a
		/// comment (MikeMarcin/BPR-Test#45), so this is the fixed GraphQL
		/// `updatePullRequestReviewComment` mutation, addressed by the
		/// comment's node ID. A submitted comment keeps `update_comment`.
		update_pending_comment
	};
	inline constexpr std::size_t   maximum_staged_review_comments = 100;
	inline constexpr std::uint32_t maximum_staged_review_pages    = 10;
	struct staged_review_line
	{
		core::tour_code_destination Source;
		std::string                 Body;
		auto                        operator==(const staged_review_line&) const -> bool = default;
	};
	/// Exact operation input; numerical REST identities never pass through floating point.
	/// Review/Comment identify already observed objects. This record grants no authority.
	struct staged_review_request
	{
		review_submission_live_binding Binding;
		staged_review_operation        Operation = staged_review_operation::list_reviews;
		review_database_id             Review;
		std::uint64_t                  Comment = 0;
		/// [[BPR-1368]]. The node ID of `Comment`, for `update_pending_comment` only.
		std::string                     CommentNode;
		std::uint32_t                   Page = 1;
		std::string                     Body;
		std::vector<staged_review_line> Lines;
		auto                            operator==(const staged_review_request&) const -> bool = default;
	};
	enum class staged_review_state : std::uint8_t { pending,
													commented,
													approved,
													changes_requested,
													dismissed };
	struct staged_review_actor
	{
		std::uint64_t DatabaseId = 0;
		std::string   NodeId;
		std::string   Login;
		auto          operator==(const staged_review_actor&) const -> bool = default;
	};
	struct staged_review_entity
	{
		review_database_id   Id;
		std::string          NodeId;
		std::string          WebUrl;
		staged_review_actor  Actor;
		core::git_commit_oid Commit;
		std::string          Body;
		staged_review_state  State                                                 = staged_review_state::pending;
		auto                 operator==(const staged_review_entity&) const -> bool = default;
	};
	struct staged_review_comment
	{
		std::uint64_t                              Id = 0;
		std::string                                NodeId;
		std::string                                WebUrl;
		staged_review_actor                        Actor;
		review_database_id                         Review;
		std::optional<core::tour_code_destination> Source;
		std::string                                Body;
		bool                                       Reply                                                  = false;
		auto                                       operator==(const staged_review_comment&) const -> bool = default;
	};
	/// One validated page; the session must exhaust HasNext before using membership.
	struct staged_review_observation
	{
		std::vector<staged_review_entity>  Reviews;
		std::vector<staged_review_comment> Comments;
		std::optional<std::string>         Description;
		bool                               HasNext                                                    = false;
		auto                               operator==(const staged_review_observation&) const -> bool = default;
	};
}
#endif
