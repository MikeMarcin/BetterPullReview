#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROUND_TRIP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROUND_TRIP_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	using review_draft_id = strong_id<struct review_draft_id_tag>;

	/// \brief Names the disposition a published review carries.
	///
	/// \details These are GitHub's three ordinary dispositions and nothing
	/// else. BetterPullReview adds no fourth: a review this tool publishes must
	/// be indistinguishable from one written in the web UI.
	enum class review_disposition : std::uint8_t {
		comment,
		approve,
		request_changes,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_disposition aDisposition) noexcept
		-> std::string_view;

	/// \brief Names where one draft sits between composition and publication.
	///
	/// \details `publishing` exists because publication is not instantaneous
	/// and the interval is observable. Without it a draft would be either
	/// editable or published, and a second publish issued while the first was
	/// in flight would look legal.
	enum class review_draft_state : std::uint8_t {
		drafting,
		publishing,
		published,
		discarded,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_draft_state aState) noexcept
		-> std::string_view;

	inline constexpr std::size_t maximum_pending_mark_path_bytes       = 4096U;
	inline constexpr std::size_t maximum_pending_mark_submission_bytes = 256U;

	/// \brief [[BPR-1345]]. A draft kept pending in a review the reviewer
	/// finishes later, GitHub's "Start a review".
	///
	/// \details The mark is kept with the draft on this computer and names
	/// where the comment goes when the review is submitted: the file's path at
	/// the head the draft was written on. The draft itself still names its
	/// line or range and side, and its revision says which text is meant.
	/// `Submission` is empty while the review is being written. When the
	/// reviewer submits, every carried draft is moved to `publishing` with the
	/// submission's idempotency key here, in one durable write before anything
	/// is sent, so a draft a submission holds can be neither edited nor
	/// published on its own until that submission's answer is known.
	struct review_pending_mark
	{
		std::string    Path;
		git_commit_oid Head;
		std::string    Submission;

		auto operator<=>(const review_pending_mark&) const = default;
	};

	/// \brief Checks a bounded path, a valid head and a bounded printable key.
	[[nodiscard]] auto valid(const review_pending_mark& aMark) noexcept -> bool;

	/// \brief Holds one comment a reviewer is composing.
	///
	/// \details The anchor is RFC-BPR-0004's semantic source-line identity, not
	/// a visual row. A draft that remembered where a line appeared on screen
	/// would attach itself to the wrong text as soon as anything above it
	/// changed.
	///
	/// A draft is private until it is published. There is no state in which it
	/// is partly shared: `Shared` is a consequence of reaching `published`, and
	/// is not settable on its own.
	struct review_comment_draft
	{
		review_draft_id    Id;
		source_line_ref    Anchor;
		std::string        Body;
		review_draft_state State = review_draft_state::drafting;
		/// [[BPR-0657]]. The thread this draft answers, by the provider's
		/// opaque thread identity, or none for a new root comment at the line.
		std::optional<std::string> ReplyToThread;
		/// [[BPR-1212]]. The zero-based first line of a comment on several
		/// lines, or none for one line. `Anchor` is then the range's last
		/// line, which is what GitHub calls `line`; this is its `start_line`,
		/// on the anchor's own side and comparison. A range is a new comment
		/// on the right side and always starts above its anchor
		/// (`valid_comment_range`); a reply is anchored by its thread and has
		/// none.
		std::optional<std::uint32_t> StartLine = { };
		/// [[BPR-1345]]. Pending in the reviewer's review, or none. Only a
		/// new comment on a line or range of either side can be; a reply
		/// cannot.
		std::optional<review_pending_mark> InReview = { };

		[[nodiscard]] auto shared() const noexcept -> bool
		{
			return State == review_draft_state::published;
		}

		auto operator<=>(const review_comment_draft&) const = default;
	};

	/// rief [[BPR-1345]]. A draft still being written that is pending in
	/// the review: counted in it and sent with it.
	[[nodiscard]] constexpr auto pending_in_review(const review_comment_draft& aDraft) noexcept -> bool
	{
		return aDraft.InReview.has_value() && aDraft.State == review_draft_state::drafting;
	}

	/// rief [[BPR-1345]]. A draft a submitted review holds until its answer
	/// is known. Only that review's answer may complete or release it.
	[[nodiscard]] constexpr auto held_by_review(const review_comment_draft& aDraft) noexcept -> bool
	{
		return aDraft.InReview.has_value() && !aDraft.InReview->Submission.empty() && aDraft.State == review_draft_state::publishing;
	}

	/// \brief Admits new inline comments and replies on either side.
	/// \details This is target shape only; current-head, authority and endpoint checks
	/// remain with their existing core and host admission paths. [[BPR-1212]]: a new
	/// comment on the left (base) side is GitHub's `side: LEFT`, whose line numbers
	/// count the merge base's version of the file. The host admits one only with its
	/// evidence that the comparison's base side is numbered as that version, and
	/// `decide_comment_range` asks for the same evidence.
	[[nodiscard]] constexpr auto supported_comment_target(const diff_side aSide) noexcept -> bool
	{
		return aSide == diff_side::left || aSide == diff_side::right;
	}

	/// \brief [[BPR-1212]]. Whether a draft's line range is one GitHub can take.
	/// \details One line always is. A range is a new comment, on one side, whose
	/// first line is above its last: GitHub refuses a reply with a `start_line`,
	/// a range across sides and a start that does not precede the end. Both ends
	/// carry the anchor's side. Whether both ends sit in one hunk is decided
	/// against the diff by `decide_comment_range`.
	[[nodiscard]] constexpr auto valid_comment_range(const review_comment_draft& aDraft) noexcept -> bool
	{
		return !aDraft.StartLine.has_value()
			|| (!aDraft.ReplyToThread.has_value() && supported_comment_target(aDraft.Anchor.Side) && *aDraft.StartLine < aDraft.Anchor.Line);
	}

	/// \brief Names one command a reviewer can issue against a draft.
	enum class review_draft_command_kind : std::uint8_t {
		edit,
		discard,
		begin_publish,
		complete_publish,
		fail_publish,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_draft_command_kind aKind) noexcept
		-> std::string_view;

	/// \brief Carries one draft command.
	struct review_draft_command
	{
		review_draft_id           Id;
		review_draft_command_kind Kind = review_draft_command_kind::edit;
		std::string               Body;

		auto operator<=>(const review_draft_command&) const = default;
	};

	/// \brief Names why a draft command was refused.
	///
	/// \details Each value is a different mistake with a different repair. A
	/// single "illegal" would tell a caller nothing it could act on.
	enum class review_draft_rejection : std::uint8_t {
		none,
		unknown_draft,
		empty_body,
		already_publishing,
		already_published,
		discarded_draft,
		not_publishing,
		/// [[BPR-1212]]. A line range GitHub would refuse (`valid_comment_range`).
		invalid_range,
		/// [[BPR-1345]]. A pending mark this draft cannot take or lose here:
		/// a reply, an invalid mark, or a submission key changed other than by
		/// the review that holds the draft.
		invalid_review_mark,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_draft_rejection aRejection) noexcept
		-> std::string_view;

	/// \brief Reports what one draft command did.
	struct review_draft_outcome
	{
		review_comment_draft   Draft;
		review_draft_rejection Rejection = review_draft_rejection::none;
		bool                   Changed   = false;

		auto operator<=>(const review_draft_outcome&) const = default;
	};

	/// \brief Applies one command to one draft.
	///
	/// \details This is the whole of the draft policy. It performs no request,
	/// reaches no provider, and knows nothing about HTTP. Publication is
	/// proposed here and executed elsewhere.
	[[nodiscard]] auto apply(
		const review_comment_draft& aDraft,
		const review_draft_command& aCommand) -> review_draft_outcome;

	/// \brief Names the exact pull request and revision a mutation targets.
	///
	/// \details All four fields together are the target. Three of them say
	/// *where* the comment goes and the fourth says *what it is about*, and the
	/// two kinds of mismatch are not equally recoverable — which is why they
	/// are decided together rather than checked in separate places that could
	/// disagree.
	struct review_mutation_target
	{
		git_host_id     Host;
		repository_id   Repository;
		pull_request_id PullRequest;
		git_commit_oid  ExpectedHead;

		auto operator<=>(const review_mutation_target&) const = default;
	};

	/// \brief Names the outcome of comparing a prepared target to the observed one.
	enum class target_revalidation : std::uint8_t {
		accepted,
		head_moved,
		target_changed,
		head_unknown,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(target_revalidation aOutcome) noexcept
		-> std::string_view;

	/// \brief Compares the target a publication was prepared against to reality.
	///
	/// \details The order of the checks is the policy. A changed host,
	/// repository, or pull request is reported before a moved head, because a
	/// comment aimed at the wrong pull request is a different and worse mistake
	/// than one aimed at the right pull request's newer revision, and reporting
	/// the head first would hide it.
	[[nodiscard]] auto revalidate(
		const review_mutation_target& aPrepared,
		const review_mutation_target& aObserved) noexcept -> target_revalidation;

	/// \brief Decides whether a publication may proceed after revalidation.
	///
	/// \details A moved head may be accepted by an explicit acknowledgement:
	/// the reviewer has been shown the new revision and said to go ahead.
	///
	/// A changed target may **not**, whatever the reviewer acknowledged. There
	/// is no interface through which someone can agree to post a comment to a
	/// repository they were not looking at, because the acknowledgement they
	/// gave was about a different pull request. Nor may an unknown head: a
	/// revision nobody can name is not one a reviewer can have agreed to.
	[[nodiscard]] auto publication_permitted(
		target_revalidation aOutcome,
		bool                aAcknowledged) noexcept -> bool;

	/// \brief Local-only durable identity included in idempotency, never public prose.
	struct durable_review_publication_identity
	{
		std::array<std::uint8_t, 32> Incarnation{ };
		std::uint64_t                DraftRevision                                                 = 0;
		auto                         operator<=>(const durable_review_publication_identity&) const = default;
	};
	/// \brief Describes the semantic proposal a publish produces.
	///
	/// \details The core emits this rather than a request. It names what is to
	/// be said and where, and carries no host, no endpoint, and no credential —
	/// those are the host's, and keeping them out of the core is what stops a
	/// policy decision from quietly becoming a network call.
	struct review_publication_proposal
	{
		review_draft_id    Draft;
		source_line_ref    Anchor;
		std::string        Body;
		review_disposition Disposition = review_disposition::comment;
		/// [[BPR-0657]]. Carried from the draft: the thread the comment
		/// answers, or none.
		std::optional<std::string> ReplyToThread;

		std::optional<durable_review_publication_identity> Durable = { };
		/// [[BPR-1212]]. Carried from the draft: the range's first line, or
		/// none for one line. `Anchor` is the range's last line.
		std::optional<std::uint32_t> StartLine = { };

		auto operator<=>(const review_publication_proposal&) const = default;
	};

	/// \brief Why a thread resolution is refused before any provider is asked ([[BPR-0657]]).
	enum class thread_resolution_refusal : std::uint8_t {
		none,
		/// The session reads anonymously; a mutation has no account to act as.
		anonymous_read,
		/// The server never said it resolves threads, or said it does not.
		capability_absent,
		/// The thread is already in the state asked for.
		already_in_state,
	};

	/// \brief What decides whether a thread may be resolved or unresolved.
	struct thread_resolution_input
	{
		github_capability_state Capability    = github_capability_state::unknown;
		bool                    AnonymousRead = false;
		bool                    Resolved      = false;
		bool                    Resolve       = true;

		auto operator<=>(const thread_resolution_input&) const = default;
	};

	/// \brief Decides a thread resolution from its inputs alone.
	[[nodiscard]] constexpr auto decide_thread_resolution(
		const thread_resolution_input& aInput) noexcept -> thread_resolution_refusal
	{
		if (aInput.AnonymousRead) {
			return thread_resolution_refusal::anonymous_read;
		}
		if (aInput.Capability != github_capability_state::supported) {
			return thread_resolution_refusal::capability_absent;
		}
		if (aInput.Resolved == aInput.Resolve) {
			return thread_resolution_refusal::already_in_state;
		}
		return thread_resolution_refusal::none;
	}

	/// \brief Builds the proposal for a draft that has begun publishing.
	///
	/// \details Returns a proposal only in the `publishing` state. A draft
	/// still being composed has not been submitted, and one already published
	/// must not be proposed twice.
	[[nodiscard]] auto propose_publication(
		const review_comment_draft& aDraft,
		review_disposition          aDisposition)
		-> std::optional<review_publication_proposal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROUND_TRIP_HPP
