#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PRIVATE_THREAD_LINEAGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PRIVATE_THREAD_LINEAGE_HPP

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/model.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

/// \file
/// \brief The provider-independent slice of private review threads: versioned
/// headers, immutable authored revisions, reply and edit lineage, completion
/// and authorship provenance.
/// \details Pull-request private threads (RFC-BPR-0023) and local Review
/// Cycles feedback (RFC-BPR-0055) hold their subjects and scopes separately
/// and share these identities, bounds and rules, so a thread's history means
/// the same thing whichever review it belongs to. Nothing here names an
/// account, repository, pull request, provider or agent run.
namespace bpr::core {
	using private_review_thread_id        = strong_id<struct private_review_thread_id_tag>;
	using private_review_turn_id          = strong_id<struct private_review_turn_id_tag>;
	using private_review_turn_revision_id = strong_id<struct private_review_turn_revision_id_tag>;
	using private_vertex_order            = strong_id<struct private_vertex_order_tag>;
	using private_body_digest             = fixed_identity_256<struct private_body_digest_tag>;

	inline constexpr std::uint64_t maximum_private_identity             = std::numeric_limits<std::int64_t>::max();
	inline constexpr std::size_t   maximum_private_reviewer_body_bytes  = 16U * 1024U;
	inline constexpr std::size_t   maximum_private_assistant_body_bytes = 1024U * 1024U;

	/// \brief Checks the positive, non-wrapping durable ledger ordinal domain.
	template <typename TTag>
	[[nodiscard]] constexpr auto valid_private_id(strong_id<TTag> aId) noexcept -> bool
	{
		return aId.value() > 0 && aId.value() <= maximum_private_identity;
	}

	enum class private_thread_lifecycle : std::uint8_t { open,
														 resolved_locally,
														 archived };
	enum class private_revision_completion : std::uint8_t { complete,
															incomplete,
															failed,
															cancelled };
	enum class private_derivation_kind : std::uint8_t {
		replies_to,
		edited_from,
		copied_from,
		refined_from,
		composed_from,
		promoted_from_finding,
		adopted_from_incomplete_run
	};
	enum class private_review_refusal : std::uint8_t {
		invalid_identity,
		invalid_binding,
		invalid_subject,
		invalid_actor,
		invalid_completion,
		bounds,
		conflict,
		archived,
		invalid_transition,
		missing_source,
		wrong_scope,
		digest_mismatch,
		incomplete_source,
		duplicate_source,
		cycle,
		ambiguous_target,
		invalid_target,
		source_changed,
		invalid_provenance,
		invalid_mode
	};

	/// \brief Who a revision's text came from, as far as it can be known.
	/// \details The four classes never merge. An agent answer pasted by hand
	/// is self-reported, not adapter-observed, however it describes itself; a
	/// record that never said who wrote it stays unknown.
	enum class authored_provenance : std::uint8_t {
		/// Typed or edited by the reviewer in this profile.
		human,
		/// Produced through an adapter this build ran and observed.
		adapter_observed,
		/// Supplied by hand or by a consumer that described its own origin;
		/// the description is a claim, not an observation.
		self_reported,
		/// No record of who wrote it.
		unknown,
	};

	[[nodiscard]] auto stable_name(private_thread_lifecycle aValue) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(private_revision_completion aValue) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(private_review_refusal aValue) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(authored_provenance aValue) noexcept -> std::string_view;
	/// \brief SHA-256 over exact bytes, including whitespace and embedded NUL.
	[[nodiscard]] auto private_digest(std::string_view aBody) noexcept -> private_body_digest;

	/// \brief The largest body a revision of this provenance may carry: the
	/// reviewer's own notes are short; observed or pasted answers may be long.
	[[nodiscard]] auto private_body_limit(authored_provenance aProvenance) noexcept -> std::size_t;
	/// \brief Only text an adapter is still producing, or stopped producing,
	/// may be less than complete.
	[[nodiscard]] auto valid_private_completion(authored_provenance aProvenance, private_revision_completion aCompletion) noexcept -> bool;

	/// \brief The mutable part of a thread header that every thread kind shares.
	struct private_thread_lineage
	{
		std::uint64_t            Version = 0;
		private_vertex_order     LastOrder;
		private_thread_lifecycle Lifecycle                                        = private_thread_lifecycle::open;
		auto                     operator<=>(const private_thread_lineage&) const = default;
	};
	/// \brief True for a header with a positive version, order and a known lifecycle.
	[[nodiscard]] auto valid_private_lineage(const private_thread_lineage& aLineage) noexcept -> bool;
	/// \brief Why a content write against this header must be refused, if it must:
	/// a stale expected version, an exhausted version, or an archived thread.
	[[nodiscard]] auto private_write_refusal(const private_thread_lineage& aLineage, std::uint64_t aExpectedVersion) noexcept
		-> std::optional<private_review_refusal>;
	/// \brief Advances the header for a new content vertex; its order must be
	/// later than every earlier vertex of the thread.
	[[nodiscard]] auto advance_private_lineage(private_thread_lineage& aLineage, private_vertex_order aOrder) noexcept
		-> std::optional<private_review_refusal>;
	/// \brief Changes local visibility or resolution only. The same lifecycle
	/// returns the header unchanged; an archive is left only explicitly.
	[[nodiscard]] auto change_private_lifecycle(private_thread_lineage& aLineage, std::uint64_t aExpectedVersion, private_thread_lifecycle aLifecycle) noexcept
		-> std::optional<private_review_refusal>;

	/// \brief One revision's place in its thread's history.
	struct private_lineage_position
	{
		private_review_turn_id          Turn;
		private_review_turn_revision_id Revision;
		private_vertex_order            Order;
		auto                            operator<=>(const private_lineage_position&) const = default;
	};
	/// \brief True when `aPrior` can precede `aNew`: an earlier vertex and a
	/// different revision.
	[[nodiscard]] auto private_prior_precedes(const private_lineage_position& aNew, const private_lineage_position& aPrior) noexcept -> bool;
	/// \brief The derivation an edit records: an ordinary edit of complete
	/// text, or the explicit adoption of text a run left incomplete.
	[[nodiscard]] auto private_edit_derivation(private_revision_completion aPrior) noexcept -> private_derivation_kind;
}

#endif
