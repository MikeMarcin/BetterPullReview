#include "bpr/application/feedback_sources.hpp"

#include "bpr/core/feedback_conversation.hpp"
#include "bpr/core/review_tour_markdown.hpp"

#include <set>
#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto prefixed(std::string_view aPrefix, std::string_view aValue) -> std::string
		{
			auto text = std::string{ aPrefix };
			text.append(aValue);
			return text;
		}

		[[nodiscard]] auto author_of(const std::optional<github_actor_record>& aAuthor) -> std::optional<std::string>
		{
			if (!aAuthor || aAuthor->Actor.value().empty()) {
				return std::nullopt;
			}
			return std::string{ aAuthor->Actor.value() };
		}

		[[nodiscard]] auto link_first(std::string_view aBody) noexcept -> bool
		{
			return aBody.starts_with("https://") || aBody.starts_with("http://");
		}

		[[nodiscard]] auto where_of(std::string_view aPath, std::optional<std::uint32_t> aLine) -> std::string
		{
			if (aPath.empty()) {
				return "Pull request";
			}
			return aLine ? std::string{ aPath } + ":" + std::to_string(*aLine) : std::string{ aPath };
		}

		[[nodiscard]] auto trimmed(std::string_view aText) noexcept -> std::string_view
		{
			while (!aText.empty() && (aText.back() == '\n' || aText.back() == '\r' || aText.back() == ' ' || aText.back() == '\t')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		/// [[BPR-1087]]. A body that is exactly the tour index the product
		/// generates: rendering its one link list again gives back the whole
		/// body. A person's words around the list make it an ordinary comment.
		[[nodiscard]] auto generated_tour_index(std::string_view aBody) -> bool
		{
			const auto map = core::map_tour_markdown(aBody);
			if (!map.Bounded || map.Lists.size() != 1U || map.Lists.front().MappingNeeded || map.Lists.front().Entries.empty()) {
				return false;
			}
			const auto rendered = core::render_tour_index(map.Lists.front().Entries, aBody.find("\r\n") == std::string_view::npos ? "\n" : "\r\n");
			return rendered && trimmed(*rendered) == trimmed(aBody);
		}

		/// The posts and conversations of \p aSnapshot. \p aTourIndex says
		/// whether to recognize generated tour indexes, which the post list
		/// does not depend on.
		[[nodiscard]] auto collect_feedback_sources(const github_pull_request_snapshot& aSnapshot, const bool aTourIndex) -> feedback_sources
		{
			auto       sources = feedback_sources{ };
			const auto add     = [&](std::string aConversation, std::string aId, std::optional<std::string> aReplyTo, std::string_view aBody, bool aDeleted) {
				sources.Posts.push_back({ .Conversation = std::move(aConversation), .Id = std::move(aId), .ReplyTo = std::move(aReplyTo), .Body = aBody, .Segment = core::published_feedback_segment(aBody), .Deleted = aDeleted });
			};
			const auto threads  = aSnapshot.ReviewThreadsAvailability == github_collection_availability::available;
			const auto comments = aSnapshot.ReviewCommentsAvailability == github_collection_availability::available;
			// [[BPR-1605]]. A thread whose root comment is in my pending GitHub
			// review is not published yet, as that review's body is not: the
			// preflight reads it from the review's own comments, as not sent.
			auto pendingReviews = std::set<std::string_view>{ };
			for (const auto& review : aSnapshot.Reviews) {
				if (review.State == github_review_state::pending) {
					pendingReviews.insert(review.Review.value());
				}
			}
			if (threads) {
				for (const auto& thread : aSnapshot.ReviewThreads) {
					auto                                conversation = prefixed("review-thread:", thread.Thread.value());
					const auto                          file         = thread.Subject == github_review_thread_subject::file;
					const github_review_comment_record* root         = nullptr;
					auto                                roots        = std::size_t{ 0U };
					for (const auto& comment : thread.Comments) {
						add(conversation, prefixed("review-comment:", comment.Comment.value()),
							comment.ReplyTo ? std::optional{ prefixed("review-comment:", comment.ReplyTo->value()) } : std::nullopt, comment.Body, comment.Deleted);
						if (file && link_first(comment.Body)) {
							// A file comment that begins with a link may be a permalink
							// envelope (RFC-BPR-0022) whose prose this layer cannot map:
							// its label is unclassified, not guessed. One that does not
							// cannot be one, and is ordinary text.
							sources.Posts.back().Segment = { .Managed = true };
						}
						if (!comment.ReplyTo) {
							root = &comment;
							++roots;
						}
					}
					sources.Conversations.push_back(
						{ .Facts = { .Id         = conversation,
									 .Origin     = roots == 1U && pendingReviews.contains(root->Observation.ReviewNode) ? core::feedback_unit_origin::pending_publication : core::feedback_unit_origin::published,
									 .Resolution = thread.Resolved ? core::feedback_resolution::resolved : core::feedback_resolution::unresolved,
									 .Anchor     = thread.Path.empty() ? core::feedback_anchor{ } : core::feedback_anchor{ .Status = core::feedback_anchor_status::file, .Path = thread.Path, .Outdated = thread.Outdated } },
						  // No root, or more than one, names no author.
						  .RootAuthor = roots == 1U ? author_of(root->Author) : std::nullopt,
						  .Where      = where_of(thread.Path, thread.Line ? thread.Line : thread.OriginalLine) });
				}
			} else if (comments) {
				for (const auto& group : aSnapshot.ReviewCommentGroups) {
					auto conversation = prefixed("public-preview-comment:", group.Root.Comment.value());
					add(conversation, conversation, std::nullopt, group.Root.Body, group.Root.Deleted);
					for (const auto& reply : group.Replies) {
						add(conversation, prefixed("public-preview-comment:", reply.Comment.value()),
							prefixed("public-preview-comment:", reply.ReplyTo ? reply.ReplyTo->value() : group.Root.Comment.value()), reply.Body, reply.Deleted);
					}
					sources.Conversations.push_back(
						{ .Facts      = { .Id         = conversation,
										  .Resolution = core::feedback_resolution::unknown,
										  .Anchor     = group.Root.Path.empty() ? core::feedback_anchor{ } : core::feedback_anchor{ .Status = core::feedback_anchor_status::file, .Path = group.Root.Path, .Outdated = group.Root.Outdated } },
						  .RootAuthor = author_of(group.Root.Author),
						  .Where      = where_of(group.Root.Path, group.Root.Line ? group.Root.Line : group.Root.OriginalLine) });
				}
			}
			for (const auto& comment : aSnapshot.IssueComments) {
				auto conversation = prefixed("issue-comment:", comment.Comment.value());
				add(conversation, conversation, std::nullopt, comment.Body, comment.Deleted);
				sources.Conversations.push_back({ .Facts      = { .Id = conversation, .Deleted = comment.Deleted, .Resolution = core::feedback_resolution::not_applicable },
												  .RootAuthor = author_of(comment.Author),
												  .Where      = where_of({ }, std::nullopt),
												  .TourIndex  = aTourIndex && !comment.Deleted && generated_tour_index(comment.Body) });
			}
			for (const auto& review : aSnapshot.Reviews) {
				if (review.Body.empty()) {
					continue;
				}
				auto conversation = prefixed("review:", review.Review.value());
				add(conversation, conversation, std::nullopt, review.Body, false);
				// My own pending GitHub review is not published yet.
				sources.Conversations.push_back({ .Facts      = { .Id         = conversation,
																  .Origin     = review.State == github_review_state::pending ? core::feedback_unit_origin::pending_publication : core::feedback_unit_origin::published,
																  .Resolution = core::feedback_resolution::not_applicable },
												  .RootAuthor = author_of(review.Author),
												  .Where      = where_of({ }, std::nullopt) });
			}
			sources.Collection = threads || comments ? core::feedback_collection::complete : core::feedback_collection::partial;
			return sources;
		}
	} // namespace

	auto observe_feedback_sources(const github_pull_request_snapshot& aSnapshot) -> feedback_sources
	{
		auto sources = collect_feedback_sources(aSnapshot, true);
		// [[BPR-1447]]. A digest belongs to the post and length it was taken
		// from; any other post's body is hashed by the core.
		if (const auto& digests = aSnapshot.FeedbackBodyDigests.Entries; digests && digests->size() == sources.Posts.size()) {
			for (std::size_t index = 0U; index < sources.Posts.size(); ++index) {
				auto&       post  = sources.Posts[index];
				const auto& entry = (*digests)[index];
				if (entry.Post == post.Id && entry.Bytes == post.Body.size()) {
					post.BodyDigest = entry.Digest;
				}
			}
		}
		return sources;
	}

	auto digest_feedback_bodies(const github_pull_request_snapshot& aSnapshot, const body_digest_port& aDigests) -> github_feedback_body_digests
	{
		const auto sources = collect_feedback_sources(aSnapshot, false);
		auto       bodies  = std::vector<std::string_view>{ };
		bodies.reserve(sources.Posts.size());
		for (const auto& post : sources.Posts) {
			bodies.push_back(post.Body);
		}
		auto digests = std::vector<body_digest>(bodies.size());
		aDigests.digest(bodies, digests);
		auto entries = std::vector<github_feedback_body_digest>{ };
		entries.reserve(bodies.size());
		for (std::size_t index = 0U; index < sources.Posts.size(); ++index) {
			entries.push_back({ .Post = sources.Posts[index].Id, .Bytes = bodies[index].size(), .Digest = digests[index] });
		}
		return { .Entries = std::make_shared<const std::vector<github_feedback_body_digest>>(std::move(entries)) };
	}

	auto feedback_post_observations(const feedback_sources& aSources) -> std::vector<core::feedback_post_observation>
	{
		auto observed = std::vector<core::feedback_post_observation>{ };
		observed.reserve(aSources.Posts.size());
		for (const auto& post : aSources.Posts) {
			observed.push_back({ .Conversation = post.Conversation,
								 .Id           = post.Id,
								 .ReplyTo      = post.ReplyTo ? std::optional<std::string_view>{ *post.ReplyTo } : std::nullopt,
								 .Body         = post.Body,
								 .Segment      = post.Segment,
								 .Deleted      = post.Deleted,
								 .BodyDigest   = post.BodyDigest });
		}
		return observed;
	}
}
