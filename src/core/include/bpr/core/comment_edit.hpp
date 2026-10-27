#ifndef BPR_CORE_COMMENT_EDIT_HPP
#define BPR_CORE_COMMENT_EDIT_HPP

#include "bpr/core/description_edit.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief [[BPR-1775]]. Whether one of my published comments may be edited,
/// and whether a save may be written.
///
/// \details The owner, 2026-10-26: "we seem to be missing controls for
/// editing comments we have left". Edit is offered on a post the viewer wrote
/// (the read's own "Mine"), of three kinds: a review-thread comment, a pull
/// request comment, and a review's overall body. As for the description
/// ([[BPR-1774]]), Save is decided twice: once on the gesture, and once on
/// what GitHub says of the comment just before the write, so a comment
/// changed on GitHub since editing began is never overwritten silently.
namespace bpr::core {
	/// \brief GitHub's limit on a comment's body, in characters.
	inline constexpr std::size_t maximum_comment_characters = 65536U;

	/// \brief What a published comment is on GitHub; each kind has its own
	/// update.
	enum class comment_edit_kind : std::uint8_t {
		/// A comment in a review thread, on a file
		/// (`updatePullRequestReviewComment`).
		review_comment,
		/// A comment on the pull request itself (`updateIssueComment`).
		pull_request_comment,
		/// A review's overall body (`updatePullRequestReview`).
		review_body,
	};

	[[nodiscard]] auto name(comment_edit_kind aKind) noexcept -> std::string_view;

	/// \brief A post as the conversation names it: its kind and its GitHub
	/// node.
	struct comment_edit_subject
	{
		comment_edit_kind Kind = comment_edit_kind::review_comment;
		std::string_view  Node;

		auto operator==(const comment_edit_subject&) const -> bool = default;
	};

	/// \brief The kind and node of a post named "review-comment:<node>",
	/// "issue-comment:<node>" or "review:<node>"; nothing for any other name
	/// (a draft, a thread, an empty node).
	[[nodiscard]] auto comment_edit_subject_of(std::string_view aPost) noexcept -> std::optional<comment_edit_subject>;

	/// \brief Why a comment edit or save is refused.
	enum class comment_edit_refusal : std::uint8_t {
		none,
		/// The viewer did not write the comment, or reads anonymously.
		not_mine,
		/// The text is the body editing began with; there is nothing to send.
		unchanged,
		/// The text has nothing but spaces and line breaks. GitHub keeps no
		/// empty comment, and emptying one is not how it is deleted.
		empty,
		/// The text is longer than GitHub keeps.
		too_long,
	};

	[[nodiscard]] auto name(comment_edit_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Whether Edit is offered: the viewer wrote the post, in a
	/// signed-in read, and the post is one of the three kinds.
	[[nodiscard]] auto comment_edit_offered(std::string_view aPost, bool aMine, bool aAnonymous) noexcept -> bool;

	/// \brief What decides a Save gesture, before anything is asked of GitHub.
	struct comment_edit_input
	{
		/// The read says the viewer wrote it.
		bool Mine = false;
		/// The session reads anonymously.
		bool Anonymous = false;
		/// The body editing began with, byte for byte.
		std::string_view Original;
		/// The text to save.
		std::string_view Edited;
	};

	/// \brief Decides a Save gesture from its inputs alone, in order: not
	/// mine, unchanged, empty, too long.
	[[nodiscard]] auto decide_comment_edit(const comment_edit_input& aInput) noexcept -> comment_edit_refusal;

	/// \brief Decides a save from GitHub's answer, exactly as a description's
	/// is: already saved, not permitted, conflict, write.
	[[nodiscard]] auto decide_comment_save(const description_save_observation& aObservation) noexcept -> description_save_outcome;
} // namespace bpr::core

#endif
