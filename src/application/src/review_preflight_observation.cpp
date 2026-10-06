#include "bpr/application/review_preflight_observation.hpp"

#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr std::size_t maximum_subject_bytes = 200U;

		struct posted
		{
			std::string                 Conversation;
			std::string                 Id;
			std::optional<std::string>  ReplyTo;
			std::string_view            Body;
			core::feedback_body_segment Segment;
			bool                        Deleted = false;
		};

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

		/// An ordinary body is the user's from its first byte; a tour stop's
		/// only inside its mapped narrative, and not at all when the generated
		/// heading is there but the rest of the envelope does not map.
		[[nodiscard]] auto segment_of(std::string_view aBody) -> core::feedback_body_segment
		{
			const auto stop = core::map_tour_stop(aBody);
			if (stop.Narrative) {
				return { .Managed = true, .UserBody = core::feedback_span{ stop.Narrative->Begin, stop.Narrative->End } };
			}
			if (stop.Heading) {
				return { .Managed = true };
			}
			return { };
		}

		/// The subject as written, else the first line, cut at a character.
		[[nodiscard]] auto subject_of(std::string_view aBody, const core::feedback_annotation* aAnnotation) -> std::string
		{
			auto text = aAnnotation != nullptr && aAnnotation->Subject ? core::feedback_text(aBody, *aAnnotation->Subject) : aBody;
			if (aAnnotation != nullptr && aAnnotation->UserBody && !aAnnotation->Subject) {
				text = core::feedback_text(aBody, *aAnnotation->UserBody);
			}
			text = text.substr(0U, text.find('\n'));
			while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) {
				text.remove_suffix(1U);
			}
			if (text.size() > maximum_subject_bytes) {
				auto end = maximum_subject_bytes;
				while (end > 0U && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
					--end;
				}
				text = text.substr(0U, end);
			}
			return std::string{ text };
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
	} // namespace

	auto observe_review_preflight(const core::cached_review_key& aScope, const github_pull_request_snapshot& aSnapshot)
		-> review_preflight_observation
	{
		auto       observation = review_preflight_observation{ .Snapshot  = aSnapshot.Snapshot,
															   .Account   = aSnapshot.Account,
															   .Head      = std::string{ aSnapshot.PullRequest.HeadOid.value() },
															   .Submitter = std::string{ aSnapshot.Viewer.Actor.value() } };
		auto       posts       = std::vector<posted>{ };
		auto       facts       = std::vector<core::feedback_conversation_facts>{ };
		auto       where       = std::map<std::string, std::string>{ };
		const auto add         = [&](std::string aConversation, std::string aId, std::optional<std::string> aReplyTo, std::string_view aBody, bool aDeleted) {
			posts.push_back({ .Conversation = std::move(aConversation), .Id = std::move(aId), .ReplyTo = std::move(aReplyTo), .Body = aBody, .Segment = segment_of(aBody), .Deleted = aDeleted });
		};
		const auto threads = aSnapshot.ReviewThreadsAvailability == github_collection_availability::available;
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
						posts.back().Segment = { .Managed = true };
					}
					if (!comment.ReplyTo) {
						root = &comment;
						++roots;
					}
				}
				// No root, or more than one, names no author.
				observation.Authors.push_back({ .Conversation = conversation, .RootAuthor = roots == 1U ? author_of(root->Author) : std::nullopt });
				facts.push_back({ .Id         = conversation,
								  .Resolution = thread.Resolved ? core::feedback_resolution::resolved : core::feedback_resolution::unresolved,
								  .Anchor     = thread.Path.empty() ? core::feedback_anchor{ } : core::feedback_anchor{ .Status = core::feedback_anchor_status::file, .Path = thread.Path, .Outdated = thread.Outdated } });
				where.emplace(conversation, where_of(thread.Path, thread.Line ? thread.Line : thread.OriginalLine));
			}
		} else if (aSnapshot.ReviewCommentsAvailability == github_collection_availability::available) {
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				auto conversation = prefixed("public-preview-comment:", group.Root.Comment.value());
				add(conversation, conversation, std::nullopt, group.Root.Body, group.Root.Deleted);
				for (const auto& reply : group.Replies) {
					add(conversation, prefixed("public-preview-comment:", reply.Comment.value()),
						prefixed("public-preview-comment:", reply.ReplyTo ? reply.ReplyTo->value() : group.Root.Comment.value()), reply.Body, reply.Deleted);
				}
				observation.Authors.push_back({ .Conversation = conversation, .RootAuthor = author_of(group.Root.Author) });
				facts.push_back({ .Id         = conversation,
								  .Resolution = core::feedback_resolution::unknown,
								  .Anchor     = group.Root.Path.empty() ? core::feedback_anchor{ } : core::feedback_anchor{ .Status = core::feedback_anchor_status::file, .Path = group.Root.Path, .Outdated = group.Root.Outdated } });
				where.emplace(conversation, where_of(group.Root.Path, group.Root.Line ? group.Root.Line : group.Root.OriginalLine));
			}
		}
		for (const auto& comment : aSnapshot.IssueComments) {
			auto conversation = prefixed("issue-comment:", comment.Comment.value());
			add(conversation, conversation, std::nullopt, comment.Body, comment.Deleted);
			observation.Authors.push_back({ .Conversation = conversation, .RootAuthor = author_of(comment.Author) });
			facts.push_back({ .Id = conversation, .Deleted = comment.Deleted, .Resolution = core::feedback_resolution::not_applicable });
			where.emplace(conversation, where_of({ }, std::nullopt));
		}
		for (const auto& review : aSnapshot.Reviews) {
			if (review.Body.empty()) {
				continue;
			}
			auto conversation = prefixed("review:", review.Review.value());
			add(conversation, conversation, std::nullopt, review.Body, false);
			observation.Authors.push_back({ .Conversation = conversation, .RootAuthor = author_of(review.Author) });
			// My own pending GitHub review is not published yet.
			facts.push_back({ .Id         = conversation,
							  .Origin     = review.State == github_review_state::pending ? core::feedback_unit_origin::pending_publication : core::feedback_unit_origin::published,
							  .Resolution = core::feedback_resolution::not_applicable });
			where.emplace(conversation, where_of({ }, std::nullopt));
		}

		auto observed = std::vector<core::feedback_post_observation>{ };
		observed.reserve(posts.size());
		for (const auto& post : posts) {
			observed.push_back({ .Conversation = post.Conversation,
								 .Id           = post.Id,
								 .ReplyTo      = post.ReplyTo ? std::optional<std::string_view>{ *post.ReplyTo } : std::nullopt,
								 .Body         = post.Body,
								 .Segment      = post.Segment,
								 .Deleted      = post.Deleted });
		}
		auto refresh = core::refresh_feedback_annotations({ }, aScope, observed);
		if (refresh.Refusal != core::feedback_refresh_refusal::none) {
			return observation;
		}
		auto adopted = core::adopt_feedback_refresh({ }, std::move(refresh));
		if (!adopted) {
			return observation;
		}
		const auto collection = threads || aSnapshot.ReviewCommentsAvailability == github_collection_availability::available
			? core::feedback_collection::complete
			: core::feedback_collection::partial;
		auto       population = core::build_feedback_population(*adopted, facts, collection);
		if (population.Refusal != core::feedback_population_refusal::none) {
			return observation;
		}
		observation.Annotations = std::move(*adopted);
		observation.Population  = std::move(population.Population);
		observation.Admitted    = true;

		auto bodies = std::unordered_map<std::string_view, std::string_view>{ };
		for (const auto& post : posts) {
			bodies.emplace(post.Id, post.Body);
		}
		auto annotations = std::unordered_map<std::string_view, const core::feedback_annotation*>{ };
		for (const auto& post : observation.Annotations.Posts) {
			if (post.Annotation) {
				annotations.emplace(post.Id, &*post.Annotation);
			}
		}
		for (const auto& conversation : observation.Annotations.Conversations) {
			auto reference = review_preflight_reference{ .Where = where[conversation.Id] };
			if (conversation.Root) {
				const auto body       = bodies.find(*conversation.Root);
				const auto annotation = annotations.find(*conversation.Root);
				if (body != bodies.end()) {
					reference.Subject = subject_of(body->second, annotation == annotations.end() ? nullptr : annotation->second);
				}
			}
			observation.References.emplace(conversation.Id, std::move(reference));
		}
		return observation;
	}

	auto project_review_preflight(const review_preflight_observation&                 aObservation,
								  const std::span<const core::review_preflight_draft> aDrafts) -> std::optional<core::review_preflight>
	{
		if (!aObservation.Admitted) {
			return std::nullopt;
		}
		auto preflight = core::project_review_preflight(aObservation.Submitter, aObservation.Annotations, aObservation.Population, aObservation.Authors, aDrafts);
		if (preflight.Refusal != core::review_preflight_refusal::none) {
			return std::nullopt;
		}
		return preflight;
	}
}
