#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PUBLICATION_POLICY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PUBLICATION_POLICY_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	/// \brief Publication knowledge for the exact draft, distinct from a retry's refusal.
	enum class review_publication_outcome : std::uint8_t {
		not_published,
		indeterminate,
		published,
		/// [[BPR-1735]]. The durable journal recorded the publication rejected:
		/// GitHub answered its last child with a fully received refusal, or a
		/// migrated record was settled. No comment of it exists.
		rejected,
	};
	/// \brief A refusal to check an earlier attempt cannot establish non-publication.
	/// \details The host supplies exact prior draft knowledge and current outcome facts.
	/// Only a confirmed receipt resolves an indeterminate publication, and only
	/// the journal's `rejected` record establishes that none exists ([[BPR-1735]]):
	/// it is retained as `not_published`. This grants no write.
	[[nodiscard]] auto retain_review_publication_outcome(review_publication_outcome aPrior, review_publication_outcome aCurrent) noexcept -> review_publication_outcome;

	/// \brief Provider evidence about the selected anchor, never painted diff geometry.
	/// \details [[BPR-1706]]. `unresolvable_line` is the one exception RFC-BPR-0089
	/// makes: the core placed a commit-filtered diff's line where GitHub resolves
	/// no line comment (`filtered_comment_placement::file_comment`, decided from
	/// the line correspondences against the hunk rule measured on BPR-Test #116
	/// and #117). It is published as the file comment with no native attempt.
	enum class review_placement : std::uint8_t { unknown,
												 legacy_line,
												 off_hunk,
												 unresolvable_line };
	/// \brief Fresh evidence for one exact repository, actor and mutation operation.
	enum class off_hunk_creation_capability : std::uint8_t { unknown,
															 supported,
															 unsupported };
	enum class review_publication_representation : std::uint8_t { native_line,
																  file_comment_with_permalink };
	enum class review_body_compatibility : std::uint8_t { portable,
														  requires_line_subject };

	/// \brief Durable facts for one logical publication, with at most two child attempts.
	/// \details Only the host's committed recovery observation supplies this state.
	/// A cache entry is never a durable fallback authorization.
	enum class review_publication_state : std::uint8_t {
		initial,
		native_prepared,
		native_possibly_transmitted,
		native_definitively_unsupported,
		unsupported_evidence_admitted,
		fallback_prepared,
		fallback_possibly_transmitted,
		succeeded_native,
		succeeded_fallback,
		/// [[BPR-1735]]. GitHub answered the last child with a fully received
		/// refusal (a 4xx), so no comment of this publication exists. Planned
		/// as `initial`: publishing again starts a new attempt, and a line the
		/// core now places outside GitHub's diff is admitted to its file comment.
		rejected,
		/// [[BPR-1706]]. First-attempt admission of an unresolvable line: the
		/// parent's own durable authorization for its file comment, with no
		/// native child. Last, so every earlier state keeps its stored value.
		unresolvable_line_admitted,
	};

	/// \brief Bounded semantic inputs whose authority is validated by the host.
	struct review_publication_observation
	{
		review_placement             Placement  = review_placement::unknown;
		off_hunk_creation_capability Capability = off_hunk_creation_capability::unknown;
		review_body_compatibility    Body       = review_body_compatibility::portable;
		review_publication_state     State      = review_publication_state::initial;
		bool                         Eligible   = false;
		/// Exact scope and freshness were both validated; false ignores the cache.
		bool CapabilityCurrent = false;
	};

	enum class review_publication_action : std::uint8_t {
		refuse,
		native_attempt,
		admit_unsupported_evidence,
		fallback_attempt,
		reconcile_native,
		reconcile_fallback,
		already_succeeded,
		/// [[BPR-1706]]. Record the unresolvable line's admission before its
		/// file comment is prepared.
		admit_unresolvable_line,
	};
	enum class review_publication_diagnostic : std::uint8_t {
		none,
		ineligible_anchor,
		invalid_observation,
		incompatible_body,
		/// No longer planned: [[BPR-1735]] plans a rejected publication as a new
		/// attempt. Kept so the values after it keep their meaning.
		previously_rejected,
	};
	struct review_publication_decision
	{
		review_publication_action                        Action     = review_publication_action::refuse;
		review_publication_diagnostic                    Diagnostic = review_publication_diagnostic::none;
		std::optional<review_publication_representation> Representation;
		auto                                             operator<=>(const review_publication_decision&) const = default;
	};

	/// \brief Plans one step without I/O; a fallback step requires committed authorization.
	[[nodiscard]] auto plan_review_publication(const review_publication_observation& aObservation) noexcept -> review_publication_decision;

	/// \brief Exact facts a host may commit; ambiguous failures supply no terminal event.
	enum class review_publication_event : std::uint8_t {
		prepare_native,
		admit_unsupported_evidence,
		native_transmission_witness,
		native_created,
		native_definitively_unsupported,
		prepare_fallback,
		fallback_transmission_witness,
		fallback_created,
		/// [[BPR-1735]]. A fully received refusal of a child: never a timeout, a
		/// lost or malformed answer, or GET absence.
		definitively_rejected,
		/// [[BPR-1706]]. Only from `initial`, only for an unresolvable line.
		admit_unresolvable_line,
	};
	/// \brief Checks a proposed durable transition; rejection leaves the old state intact.
	/// \details Exact non-creation evidence may resolve a witness. Timeout, cancellation,
	/// malformed success and GET absence have no event and cannot authorize fallback.
	[[nodiscard]] auto advance_review_publication(const review_publication_observation& aObservation, review_publication_event aEvent) noexcept -> std::optional<review_publication_state>;

	/// \brief Conservatively recognizes fenced suggestion bodies that require a line subject.
	[[nodiscard]] auto classify_review_body(std::string_view aBody) noexcept -> review_body_compatibility;

	/// \brief [[BPR-1735]]. What GitHub answered the POST of a logical
	/// publication's last child, as its record keeps it.
	enum class review_publication_answer : std::uint8_t {
		/// The child may have been sent before answers were kept: a record an
		/// earlier version left, kept at the time it was migrated.
		kept_before_answers,
		success,
		error_status,
		/// The POST may have left and nothing definite came back.
		no_answer,
	};
	/// \brief [[BPR-1735]]. `StatusClass` is the hundreds digit of an answer's
	/// status (2 to 5), 0 with none. `RecordedUtcMs` is when it was kept, in
	/// UTC milliseconds since the Unix epoch.
	struct review_publication_post
	{
		review_publication_answer Answer        = review_publication_answer::no_answer;
		std::uint8_t              StatusClass   = 0U;
		std::int64_t              RecordedUtcMs = 0;

		auto operator<=>(const review_publication_post&) const = default;
	};
	/// \brief [[BPR-1735]]. How long after a migrated record was kept GitHub's
	/// comment list is trusted to show a comment GitHub created for it: as
	/// [[BPR-1715]] decided for a review.
	inline constexpr std::int64_t review_publication_settle_delay_ms = std::int64_t{ 10 } * 60 * 1000;
	/// \brief [[BPR-1735]]. Whether the answer is in range: a known answer, a
	/// class that matches it (2 for success, 3 to 5 for an error status, 0
	/// otherwise) and a time that is not negative.
	[[nodiscard]] auto valid(const review_publication_post& aPost) noexcept -> bool;
	/// \brief [[BPR-1735]]. Whether a child that may have been sent, and that
	/// GitHub's complete comment list shows no new exact comment for, is
	/// definitely not published, so it is rejected and its draft kept.
	///
	/// \details Only two answers settle it:
	/// - a 4xx status, fully received: GitHub validates a comment before it
	///   creates one, and refused this one;
	/// - a record kept before answers were (`kept_before_answers`), once
	///   `review_publication_settle_delay_ms` have passed since it was
	///   migrated, as a review's unanswered attempt settles ([[BPR-1715]]).
	///
	/// No kept answer, a success, a redirect or server error, and a POST with
	/// no definite answer never settle: their outcome is genuinely unknown,
	/// and RFC-BPR-0022 keeps them to GET-only reconciliation. A clock earlier
	/// than the record waits.
	[[nodiscard]] auto publication_definitely_unpublished(const std::optional<review_publication_post>& aPost, std::int64_t aNowUtcMs) noexcept -> bool;
}
#endif
