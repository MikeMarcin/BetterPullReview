#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_CONVERSATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_CONVERSATION_HPP

#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/feedback_convention.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Per-post Conventional Comments annotations and the root-post label of
/// each conversation (RFC-BPR-0047 R07, R08, R41).
/// \details Every post keeps its own annotation, so a reply shows its own
/// label. A conversation's counting label is its root post's and nothing
/// else's. The root is found from reply linkage alone: the one post that
/// replies to nothing, reached by every other post. Input order, page
/// position, recency and identifier order never choose it, and a surviving
/// reply is never promoted when the root is missing, deleted or ambiguous.
///
/// The annotations are a disposable derived cache keyed by scope, post
/// identity, body digest, user-body segment and parser revision. Nothing here
/// writes to a provider or stores classification as an editable authority.
namespace bpr::core {
	/// \brief One accepted provider observation of one post.
	/// \details `Conversation` is the provider's grouping (a review thread,
	/// a reconstructed REST comment group, or the post itself for a general
	/// comment). Post identities are unique across the whole scope.
	struct feedback_post_observation
	{
		std::string_view                Conversation;
		std::string_view                Id;
		std::optional<std::string_view> ReplyTo = std::nullopt;
		std::string_view                Body;
		feedback_body_segment           Segment;
		/// A deleted post has no current text, whatever its last body was.
		bool Deleted = false;
	};

	/// \brief One post and its own annotation.
	struct feedback_post
	{
		std::string                Conversation;
		std::string                Id;
		std::optional<std::string> ReplyTo;
		bool                       Deleted = false;
		feedback_body_segment      Segment;
		/// Absent for a deleted post.
		std::optional<feedback_annotation> Annotation;
	};

	/// \brief What a conversation's root says about its counting label.
	enum class feedback_root_status : std::uint8_t {
		/// The root carries a catalog label.
		labelled,
		/// The root carries a well-formed custom label: catch-all, known.
		unknown_label,
		/// The root has no header: catch-all, known.
		plain,
		/// The root exists but is malformed, over budget or inside an envelope
		/// whose user text is unknown: partially classified.
		unclassified,
		/// No observed post replies to nothing.
		root_missing,
		/// The root was observed deleted.
		root_deleted,
		/// More than one candidate root, a duplicate identity, or a reply
		/// whose chain does not reach the one root.
		root_ambiguous,
	};
	[[nodiscard]] auto stable_name(feedback_root_status aStatus) noexcept -> std::string_view;
	/// \brief Whether the conversation's label bucket is known: labelled or
	/// a known catch-all.
	[[nodiscard]] auto classification_known(feedback_root_status aStatus) noexcept -> bool;

	/// \brief The counting classification of one conversation.
	struct feedback_conversation
	{
		std::string          Id;
		feedback_root_status Status = feedback_root_status::root_missing;
		/// The root post, when exactly one was found (also when deleted).
		std::optional<std::string> Root;
		/// Set only when `labelled`.
		std::optional<feedback_label> Label;
		auto                          operator==(const feedback_conversation&) const -> bool = default;
	};

	/// \brief Classify one conversation from its posts, in any order.
	[[nodiscard]] auto classify_feedback_conversation(std::string_view aConversation, std::span<const feedback_post> aPosts) -> feedback_conversation;

	/// \brief The derived annotations of one scope (account, host, repository
	/// and pull request). `Posts` is ordered by conversation then post,
	/// `Conversations` by identity. `Epoch` counts adopted refreshes.
	struct feedback_annotation_set
	{
		cached_review_key                  Scope;
		std::uint64_t                      Epoch = 0U;
		std::vector<feedback_post>         Posts;
		std::vector<feedback_conversation> Conversations;
	};

	enum class feedback_refresh_refusal : std::uint8_t {
		none,
		/// The scope key is not a valid exact reader and destination.
		invalid_scope,
		/// A post or conversation identity is empty.
		invalid_identity,
		/// One post identity was observed twice with different facts, such as
		/// another body or another conversation.
		duplicate_post,
	};
	[[nodiscard]] auto stable_name(feedback_refresh_refusal aRefusal) noexcept -> std::string_view;

	/// \brief A complete replacement of the derived set, computed against the
	/// exact set it replaces.
	/// \details `Recognized` names posts parsed anew because they are new or
	/// their digest, segment or parser revision changed; every other current
	/// post reused its annotation. `Removed` names posts no longer observed
	/// or now deleted. `ChangedConversations` names conversations whose
	/// root, status or label changed, appeared or vanished; a reply's own
	/// edit never appears there. After a scope replacement nothing is reused
	/// and every conversation counts as changed. `Collapsed` counts repeated
	/// observations of one post that overlapping provider pages delivered
	/// with identical facts; each is kept once.
	struct feedback_annotation_refresh
	{
		feedback_refresh_refusal Refusal = feedback_refresh_refusal::none;
		cached_review_key        BaseScope;
		std::uint64_t            BaseEpoch     = 0U;
		bool                     ScopeReplaced = false;
		feedback_annotation_set  Next;
		std::vector<std::string> Recognized;
		std::vector<std::string> Removed;
		std::vector<std::string> ChangedConversations;
		std::size_t              Reused    = 0U;
		std::size_t              Collapsed = 0U;
	};

	/// \brief Recompute the set from one complete accepted observation of a
	/// scope, reusing only annotations whose exact key still matches.
	/// \details A post repeated with identical facts counts once. A post
	/// repeated with any different fact refuses the whole refresh, because
	/// no observation says which one is current.
	[[nodiscard]] auto refresh_feedback_annotations(const feedback_annotation_set& aCurrent, const cached_review_key& aScope,
													std::span<const feedback_post_observation> aPosts) -> feedback_annotation_refresh;

	/// \brief Adopt a refresh only onto the exact set it was computed against.
	/// \details A refresh computed before another one was adopted, or before
	/// the scope was replaced by another account, window or pull request, is
	/// stale and refused; the current set stays as it is.
	[[nodiscard]] auto adopt_feedback_refresh(const feedback_annotation_set& aCurrent, feedback_annotation_refresh aRefresh) -> std::optional<feedback_annotation_set>;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_CONVERSATION_HPP
