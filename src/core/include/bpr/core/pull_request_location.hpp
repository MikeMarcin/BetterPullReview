#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_LOCATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_LOCATION_HPP

#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-0987]]. What a pull request link names beyond the pull request.
	///
	/// \details The application host reads a pasted link into one of these; the
	/// core alone decides what, if anything, it names in the receiving window.
	/// `pull_request` covers the canonical page, `/files` and `/commits`: views
	/// of the whole pull request, not places in it. `unrecognized` is a
	/// fragment of a pull request link this product does not read; the pull
	/// request is still named, the location is not.
	enum class pull_request_location_kind : std::uint8_t {
		pull_request,
		commit,
		file,
		review,
		review_comment,
		issue_comment,
		unrecognized,
	};

	/// \brief A bounded, typed location inside one pull request.
	///
	/// \details `Commit` is a lowercase hexadecimal object name for `commit`.
	/// `FileDigest` is the 64 lowercase hexadecimal characters of the SHA-256
	/// of the file's path, which is how GitHub names a file on the pull
	/// request's files page (`#diff-<digest>`); `Side` and `Line` are the
	/// optional `L<n>`/`R<n>` suffix, `Line` one-based as GitHub writes it.
	/// `Identity` is the provider's numeric identity for a review or comment.
	struct pull_request_location
	{
		pull_request_location_kind Kind = pull_request_location_kind::pull_request;
		std::string                Commit;
		std::string                FileDigest;
		std::optional<diff_side>   Side;
		std::uint32_t              Line     = 0U;
		std::uint64_t              Identity = 0U;

		auto operator<=>(const pull_request_location&) const = default;
	};

	/// \brief Checks that the location carries exactly the fields its kind names.
	[[nodiscard]] auto valid(const pull_request_location& aLocation) noexcept -> bool;

	/// \brief One review or comment the accepted read identified, and the
	/// conversation the receiving window reveals for it.
	struct pull_request_location_conversation
	{
		std::uint64_t Identity = 0U;
		std::string   Conversation;

		auto operator<=>(const pull_request_location_conversation&) const = default;
	};

	/// \brief What the receiving window knows about the pull request it accepted.
	///
	/// \details Every field is a fact of the snapshot and comparison that one
	/// window accepted, supplied by its host. `ChangedPaths` are the pull
	/// request's current file paths. `Reviews` and `ReviewComments` carry only
	/// the identities the read actually returned: an authenticated read names
	/// review comments by node identity alone, so a numeric comment link has
	/// nothing to match there and is reported rather than guessed.
	/// `RightIsHead` says the comparison on screen ends at the pull request's
	/// head, which is the revision a right-side line of a link names.
	struct pull_request_location_facts
	{
		std::span<const std::string>                        ChangedPaths;
		std::span<const pull_request_location_conversation> Reviews;
		std::span<const pull_request_location_conversation> ReviewComments;
		bool                                                RightIsHead = false;
	};

	/// \brief What the receiving window does with a resolved location.
	enum class pull_request_location_action : std::uint8_t {
		/// Nothing beyond the pull request; its retained view stays.
		none,
		select_file,
		reveal_conversation,
		/// The pull request opens and the location is explained, not shown.
		unavailable,
	};

	/// \brief Why a location, or the line of a file location, is not shown.
	enum class pull_request_location_refusal : std::uint8_t {
		none,
		commit_page,
		file_not_in_pull_request,
		base_side_line,
		line_needs_head,
		line_not_in_file,
		review_not_read,
		comment_not_identified,
		issue_comment,
		unrecognized,
	};

	/// \brief The core's decision for one location against one window's facts.
	///
	/// \details For `select_file`, `Path` is the changed path the digest named;
	/// `RightLine` is present only when the requested line maps exactly, as
	/// the zero-based right-side line of the comparison on screen, and
	/// `Refusal` otherwise says why a requested line was not kept. For
	/// `reveal_conversation`, `Conversation` is the identity to reveal.
	struct pull_request_location_target
	{
		pull_request_location_action  Action = pull_request_location_action::none;
		std::string                   Path;
		std::optional<std::uint32_t>  RightLine;
		std::string                   Conversation;
		pull_request_location_refusal Refusal = pull_request_location_refusal::none;

		auto operator<=>(const pull_request_location_target&) const = default;
	};

	/// \brief Decides exactly what a location names in the receiving window.
	///
	/// \details Pure. A file digest names a changed path only when exactly one
	/// changed path hashes to it. A right-side line is kept only when the
	/// comparison on screen ends at the pull request's head; a base-side line
	/// never is, because GitHub numbers it against the merge base, which this
	/// product does not compare against. A commit page is explained: the epoch
	/// line compares uploads, not single commits. Nothing nearby is ever
	/// substituted.
	[[nodiscard]] auto resolve_pull_request_location(
		const pull_request_location&       aLocation,
		const pull_request_location_facts& aFacts) -> pull_request_location_target;

	/// \brief The sentence the window says about a refusal, without the request.
	[[nodiscard]] auto explanation(pull_request_location_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The GitHub file digest of a path: 64 lowercase hexadecimal characters.
	[[nodiscard]] auto pull_request_file_digest(std::string_view aPath) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_LOCATION_HPP
