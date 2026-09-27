#include "bpr/application/review_tour_import.hpp"
#include <algorithm>

namespace bpr::application {
	namespace {
		auto valid_destination(const github_comment_observation& aObservation, core::tour_remote_kind aKind, std::string_view aPrefix) -> bool
		{
			if (!aObservation.DatabaseId || aObservation.WebUrl.size() > core::maximum_tour_path_bytes) {
				return false;
			}
			std::string_view fragment = "#discussion_r";
			if (aKind == core::tour_remote_kind::review) {
				fragment = "#pullrequestreview-";
			}
			if (aKind == core::tour_remote_kind::conversation_comment) {
				fragment = "#issuecomment-";
			}
			return aObservation.WebUrl == std::string{ aPrefix } + std::string{ fragment } + std::to_string(aObservation.DatabaseId);
		}
		auto object(const github_comment_observation& aObservation, core::tour_remote_kind aKind, std::string_view aEntity,
					const std::optional<github_actor_record>& aAuthor, const github_pull_request_snapshot& aSnapshot, std::string_view aPrefix, bool aRoot) -> core::tour_remote_object
		{
			return { .Binding          = { .Kind = aKind, .DatabaseId = aObservation.DatabaseId, .Entity = std::string{ aEntity }, .Actor = aAuthor ? std::string{ aAuthor->Actor.value() } : std::string{ }, .WebUrl = aObservation.WebUrl, .Review = aObservation.ReviewNode, .Own = aAuthor && aAuthor->Actor == aSnapshot.Viewer.Actor },
					 .Raw              = aObservation.RawBody.value_or(std::string{ }),
					 .Version          = aObservation.UpdatedAt,
					 .Available        = aObservation.RawBody.has_value(),
					 .ValidDestination = valid_destination(aObservation, aKind, aPrefix),
					 .Root             = aRoot };
		}
	}
	auto observe_review_tours(const core::review_tour& aSeed, const github_pull_request_snapshot& aSnapshot, std::string_view aHostname) -> std::vector<core::tour_import_candidate>
	{
		if (!valid_github_canonical_hostname(aHostname) || aSeed.Owner.Host != aSnapshot.Host || aSeed.Owner.Account != aSnapshot.Account
			|| aSeed.Owner.Repository != aSnapshot.Repository.Repository || aSeed.Owner.PullRequest != aSnapshot.PullRequest.PullRequest) {
			return { };
		}
		const auto                            prefix = "https://" + std::string{ aHostname } + "/" + aSnapshot.Repository.Owner + "/" + aSnapshot.Repository.Name + "/pull/" + std::to_string(aSnapshot.PullRequest.Number.value());
		std::vector<core::tour_remote_object> objects;
		for (const auto& review : aSnapshot.Reviews) {
			if (review.State == github_review_state::pending) {
				continue;
			}
			objects.push_back(object(review.Observation, core::tour_remote_kind::review, review.Review.value(), review.Author, aSnapshot, prefix, true));
		}
		for (const auto& comment : aSnapshot.IssueComments) {
			if (!comment.Deleted) {
				objects.push_back(object(comment.Observation, core::tour_remote_kind::conversation_comment, comment.Comment.value(), comment.Author, aSnapshot, prefix, true));
			}
		}
		for (const auto& thread : aSnapshot.ReviewThreads) {
			for (const auto& comment : thread.Comments) {
				if (comment.ReplyTo || comment.Deleted) {
					continue;
				}
				auto value             = object(comment.Observation, thread.Subject == github_review_thread_subject::file ? core::tour_remote_kind::file_comment : core::tour_remote_kind::line_comment, comment.Comment.value(), comment.Author, aSnapshot, prefix, false);
				value.Binding.Resolved = thread.Resolved;
				value.Binding.Outdated = thread.Outdated || comment.Outdated;
				objects.push_back(std::move(value));
			}
		}
		for (const auto& group : aSnapshot.ReviewCommentGroups) {
			const auto& comment    = group.Root;
			auto        value      = object(comment.Observation, core::tour_remote_kind::line_comment, comment.Comment.value(), comment.Author, aSnapshot, prefix, false);
			value.Binding.Outdated = comment.Outdated;
			objects.push_back(std::move(value));
		}
		// Failed pagination never reaches an accepted snapshot. Missing raw facts in
		// older recordings or unavailable collections cannot imply editable completeness.
		const bool complete = (aSnapshot.ReviewThreadsAvailability == github_collection_availability::available || aSnapshot.ReviewCommentsAvailability == github_collection_availability::available) && std::ranges::all_of(objects, [](const auto& aObject) { return aObject.Available; });
		return core::discover_tours(aSeed, aSnapshot.PullRequest.Body, aSnapshot.PullRequest.Node.value(), objects, complete);
	}
}
