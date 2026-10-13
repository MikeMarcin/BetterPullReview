#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PUBLICATION_POLICY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PUBLICATION_POLICY_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	/// \brief Publication knowledge for the exact draft, distinct from a retry's refusal.
	enum class review_publication_outcome : std::uint8_t { not_published,
														   indeterminate,
														   published };
	/// \brief A refusal to check an earlier attempt cannot establish non-publication.
	/// \details The host supplies exact prior draft knowledge and current outcome facts.
	/// Only a confirmed receipt resolves an indeterminate publication. This grants no write.
	[[nodiscard]] auto retain_review_publication_outcome(review_publication_outcome aPrior, review_publication_outcome aCurrent) noexcept -> review_publication_outcome;

	/// \brief Provider evidence about the selected anchor, never painted diff geometry.
	enum class review_placement : std::uint8_t { unknown,
												 legacy_line,
												 off_hunk };
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
		rejected,
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
	};
	enum class review_publication_diagnostic : std::uint8_t {
		none,
		ineligible_anchor,
		invalid_observation,
		incompatible_body,
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
		definitively_rejected,
	};
	/// \brief Checks a proposed durable transition; rejection leaves the old state intact.
	/// \details Exact non-creation evidence may resolve a witness. Timeout, cancellation,
	/// malformed success and GET absence have no event and cannot authorize fallback.
	[[nodiscard]] auto advance_review_publication(const review_publication_observation& aObservation, review_publication_event aEvent) noexcept -> std::optional<review_publication_state>;

	/// \brief Conservatively recognizes fenced suggestion bodies that require a line subject.
	[[nodiscard]] auto classify_review_body(std::string_view aBody) noexcept -> review_body_compatibility;
}
#endif
