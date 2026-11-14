#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_RETURNING_REVIEW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_RETURNING_REVIEW_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). A reviewer returning to a
/// pull request with new commits asks what changed since their review, and
/// whether a review thread was addressed. Pure decisions: when each question
/// is offered, which two revisions it names, the change between them as the
/// agent reads it up front, and the lead word of an answer about a thread.
namespace bpr::core {
	/// \brief The moment the earlier revision stands for.
	enum class returning_basis : std::uint8_t {
		/// The head the reviewer's last submitted review is on.
		review,
		/// The head the reviewer said Seen on ([[BPR-2049]]).
		seen,
	};
	[[nodiscard]] auto stable_name(returning_basis aBasis) noexcept -> std::string_view;

	/// \brief Whether the earlier revision's material can be read here.
	enum class returning_material : std::uint8_t {
		/// Not asked yet, or asked and not answered.
		reading,
		/// Every changed file was read at both revisions.
		readable,
		/// The earlier revision, or a file at it, cannot be read (a rewritten
		/// branch whose earlier commits are gone, objects never retained).
		unreadable,
	};

	/// \brief What the pull request says about the reviewer's return.
	///
	/// \details Commits are full lowercase hexadecimal names; empty means
	/// none. `Commits` is the pull request's commits, oldest first, as the
	/// read lists them: it orders a review against a Seen.
	struct latest_upload_facts
	{
		std::string              ReviewedHead;
		std::string              SeenHead;
		std::string              CurrentHead;
		std::vector<std::string> Commits;
		returning_material       Material = returning_material::reading;
		/// The reviewer authored the pull request: the commits are their own.
		bool Authored = false;

		auto operator<=>(const latest_upload_facts&) const = default;
	};

	/// \brief The earlier revision of a return: the later of the reviewer's
	/// last review and their Seen.
	struct returning_point
	{
		std::string     Head;
		returning_basis Basis = returning_basis::review;

		auto operator<=>(const returning_point&) const = default;
	};

	/// \brief The head the reviewer last looked at, or none.
	///
	/// \details Of a review and a Seen, the one whose commit is later in
	/// `Commits`; a Seen when neither is listed (a Seen is said only after a
	/// review). None on a pull request the reviewer authored, or with neither.
	[[nodiscard]] auto returning_point_of(const latest_upload_facts& aFacts) -> std::optional<returning_point>;

	enum class latest_upload_state : std::uint8_t {
		/// Not offered: no review or Seen, or nothing newer than it.
		hidden,
		/// Newer commits; the earlier material is still being read.
		reading,
		/// Newer commits and both revisions read.
		offered,
		/// Newer commits, but the earlier revision cannot be read: the control
		/// says so and sends nothing.
		unavailable,
	};
	[[nodiscard]] auto stable_name(latest_upload_state aState) noexcept -> std::string_view;

	struct latest_upload_offer
	{
		latest_upload_state            State = latest_upload_state::hidden;
		std::optional<returning_point> Earlier;

		auto operator<=>(const latest_upload_offer&) const = default;
	};

	/// \brief Whether What changed since my review is offered, and on what.
	[[nodiscard]] auto decide_latest_upload(const latest_upload_facts& aFacts) -> latest_upload_offer;

	/// \brief What a review thread says about the reviewer's part in it.
	struct proposed_fix_facts
	{
		/// A private note: never asked "was this addressed".
		bool Note = false;
		/// The reviewer wrote a comment in the thread.
		bool ReviewerCommented = false;
		/// The commit the reviewer's last comment in it was written on; empty
		/// when the read does not say.
		std::string LastCommentCommit;
		std::string CurrentHead;

		auto operator<=>(const proposed_fix_facts&) const = default;
	};

	/// \brief Was this addressed? is offered on a review thread the reviewer
	/// commented in, once the pull request has commits newer than their last
	/// comment there. Not when the read does not name that comment's commit.
	///
	/// \details RFC-BPR-0128 (owner amendment to RFC-BPR-0116, 2026-11-13):
	/// offered on any review thread, whoever started it. The window passes
	/// the thread's first comment as `LastCommentCommit` (Q6: the baseline is
	/// the commit the thread started on) and `ReviewerCommented` as whether
	/// the thread has one.
	[[nodiscard]] auto proposed_fix_offered(const proposed_fix_facts& aFacts) noexcept -> bool;

	/// \brief The four words an answer about a thread leads with. The answer
	/// is the agent's claim: nothing is resolved, verified or posted by it.
	enum class addressed_claim : std::uint8_t {
		addressed,
		partly,
		not_addressed,
		cannot_tell,
	};
	inline constexpr std::size_t addressed_claim_count = 4U;
	[[nodiscard]] auto           stable_name(addressed_claim aClaim) noexcept -> std::string_view;
	/// \brief The claim as the answer's header shows it: "Addressed",
	/// "Partly", "Not addressed", "Cannot tell".
	[[nodiscard]] auto claim_words(addressed_claim aClaim) noexcept -> std::string_view;

	/// \brief The claim an answer leads with, or none when it leads with none.
	///
	/// \details Reads the first words after leading space and Markdown
	/// emphasis, heading or quote marks, case-insensitively: "not addressed",
	/// "cannot tell" (or "can't tell"), "partly", "addressed", each ending at
	/// a character that is not a letter.
	[[nodiscard]] auto read_addressed_claim(std::string_view aAnswer) noexcept -> std::optional<addressed_claim>;

	/// \brief The fence an answer about several threads lists its claims in,
	/// one line a thread: `IDENTITY: claim words`.
	inline constexpr std::string_view addressed_claims_fence = "```bpr-claims";

	/// \brief RFC-BPR-0128 as amended (owner, look ask-v005, 2026-11-14: "Why
	/// would we do it this one instead of having one large prompt with all 6
	/// threads? We could still have it build structured output if we want to
	/// display each thread result separately."). Each thread's claim, in the
	/// order the answer's `bpr-claims` block lists them; a line naming no
	/// thread or no claim is left out. None without a block.
	[[nodiscard]] auto read_addressed_claims(std::string_view aAnswer) -> std::vector<std::pair<std::string, addressed_claim>>;

	/// \brief The most changed files the change since a review shows.
	inline constexpr std::size_t maximum_returning_files = 64U;
	/// \brief The change text's bound: the up-front diff's (RFC-BPR-0101 AC13).
	inline constexpr std::size_t maximum_returning_change_bytes = 96U * 1024U;
	/// \brief Unchanged lines shown around a change.
	inline constexpr std::uint32_t returning_context_lines = 3U;

	/// \brief The material key of one file compared between two revisions.
	[[nodiscard]] auto returning_comparison(std::string_view aPath, std::optional<std::span<const std::uint8_t>> aEarlier, std::optional<std::span<const std::uint8_t>> aCurrent,
											const diff_options& aOptions) -> std::optional<material_comparison_key>;

	/// \brief One file's change between the two revisions as unified diff text
	/// (`--- a/`, `+++ b/`, `@@` hunks with `returning_context_lines`), or a
	/// line that says it is binary. A side that is absent is `/dev/null`.
	[[nodiscard]] auto render_returning_change(std::string_view aPath, const computed_material_diff& aDiff) -> std::string;

	struct returning_change_file
	{
		std::string Path;
		/// The rendered change; empty when the file did not change.
		std::string Text;

		auto operator<=>(const returning_change_file&) const = default;
	};

	/// \brief The change the agent is given, within its bound.
	struct returning_changes
	{
		std::string Text;
		std::size_t FilesShown   = 0U;
		std::size_t FilesOmitted = 0U;

		auto operator<=>(const returning_changes&) const = default;
	};

	/// \brief Joins the files' changes in path order while the whole fits
	/// `maximum_returning_change_bytes` and `maximum_returning_files`, and
	/// counts the files left out. A file that did not change is not counted.
	[[nodiscard]] auto bound_returning_changes(std::vector<returning_change_file> aFiles) -> returning_changes;

	/// \brief Which question a returning run answers.
	enum class returning_operation : std::uint8_t {
		/// `analyze_latest_upload`: what changed since my review.
		latest_upload,
		/// `evaluate_proposed_fix`: was this thread addressed.
		proposed_fix,
		/// `evaluate_proposed_fix` at a file or the pull request: were these
		/// threads addressed, in one ask (RFC-BPR-0128 as amended).
		addressed_threads,
	};
	[[nodiscard]] auto stable_name(returning_operation aOperation) noexcept -> std::string_view;

	/// \brief Lines of one file at one commit, one-based from `FirstLine`.
	struct returning_lines
	{
		std::string              Commit;
		std::string              Path;
		std::uint32_t            FirstLine = 0U;
		std::vector<std::string> Lines;

		auto operator<=>(const returning_lines&) const = default;
	};

	/// \brief Lines kept around a thread's lines, each side.
	inline constexpr std::uint32_t returning_excerpt_context       = 6U;
	inline constexpr std::size_t   maximum_returning_excerpt_lines = 64U;
	inline constexpr std::size_t   maximum_returning_line_bytes    = 512U;

	/// \brief Lines `aFirst`..`aLast` (one-based) of a text with
	/// `returning_excerpt_context` around them, within the excerpt bounds;
	/// none when the lines are not in the text.
	[[nodiscard]] auto returning_excerpt(std::string_view aCommit, std::string_view aPath, std::string_view aText, std::uint32_t aFirst, std::uint32_t aLast)
		-> std::optional<returning_lines>;

	/// \brief The most threads one Was this addressed? carries.
	inline constexpr std::size_t maximum_addressed_threads = 32U;
	/// \brief A thread's conversation's bound in a many-thread ask.
	inline constexpr std::size_t maximum_addressed_conversation_bytes = 8U * 1024U;

	/// \brief One thread of a many-thread Was this addressed?: where it is,
	/// the commit it started on, its conversation, and its lines then and now.
	struct returning_thread
	{
		std::string                    Identity;
		std::string                    Path;
		std::uint32_t                  Line = 0U;
		std::string                    Baseline;
		std::string                    Conversation;
		std::optional<returning_lines> Then;
		std::optional<returning_lines> Now;

		auto operator<=>(const returning_thread&) const = default;
	};

	/// \brief What a returning run carries up front, so its one tool-free call
	/// has its context in the request (RFC-BPR-0101 AC13).
	///
	/// \details A latest-upload run names both revisions and carries the
	/// change between them. A proposed-fix run names both too and carries the
	/// thread's lines as they were (`Then`, at the commit the reviewer's last
	/// comment was written on, absent when that commit cannot be read) and as
	/// they are (`Now`, at the current head, absent for lines the head no
	/// longer has); the thread itself rides as the conversation.
	struct supplement_returning
	{
		returning_operation Operation = returning_operation::latest_upload;
		std::string         EarlierHead;
		std::string         CurrentHead;
		returning_basis     Basis = returning_basis::review;
		std::string         Changes;
		/// RFC-BPR-0128 ([[BPR-2167]]). The commits since the review, a merge
		/// of the base branch as one line and the rest by area
		/// (`render_commit_listing`); empty when none are listed.
		std::string                    Commits;
		std::size_t                    FilesShown   = 0U;
		std::size_t                    FilesOmitted = 0U;
		std::optional<returning_lines> Then;
		std::optional<returning_lines> Now;
		/// An `addressed_threads` run's threads.
		std::vector<returning_thread> Threads;

		auto operator<=>(const supplement_returning&) const = default;
	};

	/// \brief Whether a returning supplement is within every bound: both
	/// heads named and different, changes only on a latest-upload run and
	/// within its bound, lines only on a proposed-fix run and within theirs.
	[[nodiscard]] auto valid(const supplement_returning& aReturning) -> bool;
} // namespace bpr::core

#endif
