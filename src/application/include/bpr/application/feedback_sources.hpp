#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_FEEDBACK_SOURCES_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_FEEDBACK_SOURCES_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/feedback_partition.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1087]]. The one adapter from the accepted read model to the
/// core's feedback inputs (RFC-BPR-0049): every published post and every
/// conversation's facts, read from `github_pull_request_snapshot`, never from
/// the presentation projection, whose plain `Resolved` cannot say "unknown".
namespace bpr::application {
	/// \brief One post as the core's annotation cache reads it. `Body` views the
	/// snapshot, which must outlive this value.
	struct feedback_source_post
	{
		std::string                 Conversation;
		std::string                 Id;
		std::optional<std::string>  ReplyTo;
		std::string_view            Body;
		core::feedback_body_segment Segment;
		bool                        Deleted = false;
	};

	/// \brief One conversation's facts and what a reader needs to name it.
	struct feedback_source_conversation
	{
		core::feedback_conversation_facts Facts;
		/// The root's author; none when the thread has no root or several.
		std::optional<std::string> RootAuthor;
		/// Where the conversation is: `Pull request`, `path` or `path:line`.
		std::string Where;
		/// A top-level comment that is exactly a generated tour index: a list
		/// of links to the tour's stops, which are counted as themselves. It is
		/// a reference, not a conversation (RFC-BPR-0049, "Tour index links are
		/// references, not duplicate conversation units").
		bool TourIndex = false;
	};

	struct feedback_sources
	{
		std::vector<feedback_source_post>         Posts;
		std::vector<feedback_source_conversation> Conversations;
		/// Complete when the inline review conversation was read (as native
		/// threads or as REST groups), partial when it was not.
		core::feedback_collection Collection = core::feedback_collection::partial;
	};

	/// \brief Adapts one accepted snapshot.
	/// \details Native threads carry their resolution; REST groups, which have
	/// none, are `unknown`; top-level comments and review summaries are
	/// `not_applicable`; my own pending review is `pending_publication`. A
	/// thread's file comes only from its exact path: an empty path is
	/// unanchored, never a guessed file. Native threads and REST groups are one
	/// observation each, never both, so no conversation appears twice.
	[[nodiscard]] auto observe_feedback_sources(const github_pull_request_snapshot& aSnapshot) -> feedback_sources;

	/// \brief The posts as the core's annotation refresh takes them; they view
	/// \p aSources, which must outlive the result.
	[[nodiscard]] auto feedback_post_observations(const feedback_sources& aSources) -> std::vector<core::feedback_post_observation>;
	/// [[BPR-1361]]. Refused: the posts would view a source that dies at the
	/// end of the call's full expression.
	auto feedback_post_observations(const feedback_sources&& aSources) -> std::vector<core::feedback_post_observation> = delete;
}

#endif
