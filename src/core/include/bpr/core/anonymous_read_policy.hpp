#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_ANONYMOUS_READ_POLICY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_ANONYMOUS_READ_POLICY_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

/// The core owns the signed-out public-preview mode as a mode of its own. It
/// decides which read intents a signed-out reader may express, which are
/// refused before a request could be constructed and for which typed reason,
/// which observations have no signed-out meaning at all, and what switching
/// between the two modes invalidates. Every nondeterministic fact a decision
/// rests on is handed in: no type here can represent a route, a request
/// header, a credential, a vault, a provider, or a clock. A budget refusal
/// carries the limit and an opaque reset instant supplied by the caller, and
/// core neither reads nor interprets a clock to produce one.
namespace bpr::core {
	/// The access mode a reader is in. Public preview is signed out and is a
	/// mode of its own rather than the authenticated mode with an absent
	/// account; nothing may infer it from a zero account id.
	enum class github_access_mode : std::uint8_t {
		authenticated,
		public_preview,
	};

	/// The intents a reader can express against a pull request. The first
	/// nine mirror, one to one, the read operations that need no credential,
	/// so the adapter maps its catalog onto this policy without keeping a
	/// second table. The remaining eight are the operations and conditions
	/// public preview refuses.
	///
	/// `read_review_comments` is the inline review conversation read over
	/// REST. It is legal and distinct from `read_review_threads`, which is
	/// the GraphQL thread read: the conversation is reachable signed out, the
	/// threads and their resolution are not, and collapsing the two would
	/// make a refusal look like an answer.
	enum class anonymous_read_intent : std::uint8_t {
		read_repository,
		read_pull_request,
		read_changed_files,
		read_commits,
		read_reviews,
		read_review_comments,
		read_issue_comments,
		read_checks,
		search_pull_requests,
		read_review_threads,
		read_viewer_identity,
		resolve_review_thread,
		unresolve_review_thread,
		set_viewed_file_state,
		publish_review_comment,
		submit_review,
		read_private_repository,
		select_enterprise_host,
		/// [[BPR-PENDING-AR1]]. A reaction added to or removed from a comment.
		react_to_comment,
	};

	inline constexpr std::size_t anonymous_read_intent_count = 19U;

	/// Every intent, in declaration order. `catalog_index` decides an intent's
	/// position with a switch that names each enumerator, so an intent added
	/// to the enumeration cannot reach a legality answer without being placed
	/// here as well.
	inline constexpr std::array<anonymous_read_intent, anonymous_read_intent_count>
		anonymous_read_intent_catalog{
			anonymous_read_intent::read_repository,
			anonymous_read_intent::read_pull_request,
			anonymous_read_intent::read_changed_files,
			anonymous_read_intent::read_commits,
			anonymous_read_intent::read_reviews,
			anonymous_read_intent::read_review_comments,
			anonymous_read_intent::read_issue_comments,
			anonymous_read_intent::read_checks,
			anonymous_read_intent::search_pull_requests,
			anonymous_read_intent::read_review_threads,
			anonymous_read_intent::read_viewer_identity,
			anonymous_read_intent::resolve_review_thread,
			anonymous_read_intent::unresolve_review_thread,
			anonymous_read_intent::set_viewed_file_state,
			anonymous_read_intent::publish_review_comment,
			anonymous_read_intent::submit_review,
			anonymous_read_intent::read_private_repository,
			anonymous_read_intent::select_enterprise_host,
			anonymous_read_intent::react_to_comment,
		};

	/// \brief Computes the catalog position of the supplied intent.
	[[nodiscard]] auto catalog_index(anonymous_read_intent aIntent) noexcept
		-> std::size_t;

	enum class anonymous_refusal_reason : std::uint8_t {
		rate_limited,
		private_or_missing,
		ghes_unavailable,
		graphql_refused,
		viewer_identity_refused,
		mutation_refused,
	};

	/// \brief Describes the budget fact a rate refusal reports.
	///
	/// Both fields are opaque to core. `Reset` is whatever instant value the
	/// caller measured; core copies it and never orders, ages, or interprets
	/// it.
	struct anonymous_rate_limit_fact
	{
		std::uint32_t Limit = 0;
		std::uint64_t Reset = 0;

		auto operator<=>(const anonymous_rate_limit_fact&) const = default;
	};

	/// \brief Describes a public-preview refusal.
	///
	/// The sign-in handoff is offered from every refusal, so it is a property
	/// of this type rather than a flag a caller sets per refusal: no
	/// constructor can produce a refusal that does not offer it.
	struct anonymous_refusal
	{
		static constexpr bool SignInOffered = true;

		anonymous_refusal_reason Reason
			= anonymous_refusal_reason::mutation_refused;
		std::optional<anonymous_rate_limit_fact> Budget;

		auto operator<=>(const anonymous_refusal&) const = default;
	};

	/// \brief Builds the refusal for a reason that carries no budget fact.
	[[nodiscard]] auto refuse(anonymous_refusal_reason aReason) noexcept
		-> anonymous_refusal;

	/// \brief Builds the rate refusal carrying the supplied budget fact.
	[[nodiscard]] auto refuse_rate_limited(
		const anonymous_rate_limit_fact& aFact) noexcept -> anonymous_refusal;

	/// \brief Reports whether the refusal carries exactly its reason's facts.
	[[nodiscard]] auto well_formed(const anonymous_refusal& aRefusal) noexcept
		-> bool;

	/// \brief Reports whether the refusal ends the request rather than
	/// permitting another attempt.
	[[nodiscard]] auto terminal(const anonymous_refusal& aRefusal) noexcept
		-> bool;

	/// \brief Describes a legal intent.
	struct anonymous_intent_legal
	{
		auto operator<=>(const anonymous_intent_legal&) const = default;
	};

	/// \brief Describes an intent refused before a request is constructed.
	struct anonymous_intent_refused
	{
		anonymous_refusal Refusal;

		auto operator<=>(const anonymous_intent_refused&) const = default;
	};

	using anonymous_intent_legality
		= std::variant<anonymous_intent_legal, anonymous_intent_refused>;

	/// \brief Decides the intent's legality in the supplied mode.
	///
	/// This policy governs public preview alone: in the authenticated mode
	/// every intent is legal here and the capability and permission rules the
	/// authenticated course already owns decide the rest.
	[[nodiscard]] auto legality(
		github_access_mode    aMode,
		anonymous_read_intent aIntent) noexcept -> anonymous_intent_legality;

	/// \brief Decides the intent's legality in public preview.
	[[nodiscard]] auto legality(anonymous_read_intent aIntent) noexcept
		-> anonymous_intent_legality;

	/// \brief Reports whether the intent is legal in public preview.
	[[nodiscard]] auto legal(anonymous_read_intent aIntent) noexcept -> bool;

	enum class anonymous_viewer_identity : std::uint8_t {
		unavailable,
		identified,
	};

	enum class anonymous_thread_resolution : std::uint8_t {
		unavailable,
		unresolved,
		resolved,
	};

	enum class anonymous_viewed_file_state : std::uint8_t {
		unavailable,
		dismissed,
		unviewed,
		viewed,
	};

	enum class anonymous_actionability : std::uint8_t {
		unavailable,
		blocked,
		actionable,
	};

	enum class anonymous_observation_availability : std::uint8_t {
		unavailable,
		observed,
	};

	/// \brief Describes what was observed of the inline review conversation.
	///
	/// An unobservable conversation and an observed empty one are different
	/// answers and this type keeps them apart: `unavailable` says the
	/// conversation was never read, `observed` with a zero count says it was
	/// read and holds nothing.
	struct inline_review_comment_observation
	{
		anonymous_observation_availability Availability
			= anonymous_observation_availability::unavailable;
		std::uint32_t Count = 0;

		auto operator<=>(const inline_review_comment_observation&) const
			= default;
	};

	/// \brief Builds the observation for a conversation that was never read.
	[[nodiscard]] auto unobservable_inline_review_comments() noexcept
		-> inline_review_comment_observation;

	/// \brief Builds the observation for a conversation that was read.
	[[nodiscard]] auto observed_inline_review_comments(
		std::uint32_t aCount) noexcept -> inline_review_comment_observation;

	/// \brief Reports whether the conversation was read at all.
	[[nodiscard]] auto observable(
		const inline_review_comment_observation& aObservation) noexcept -> bool;

	/// \brief Reports whether the conversation was read and holds nothing.
	[[nodiscard]] auto observed_none(
		const inline_review_comment_observation& aObservation) noexcept -> bool;

	/// \brief Reports whether the observation carries a consistent count.
	[[nodiscard]] auto well_formed(
		const inline_review_comment_observation& aObservation) noexcept -> bool;

	/// \brief Describes the observations a mode can carry.
	struct anonymous_observation_state
	{
		anonymous_viewer_identity Viewer
			= anonymous_viewer_identity::unavailable;
		anonymous_thread_resolution ThreadResolution
			= anonymous_thread_resolution::unavailable;
		anonymous_viewed_file_state ViewedFile
			= anonymous_viewed_file_state::unavailable;
		anonymous_actionability Actionability
			= anonymous_actionability::unavailable;
		inline_review_comment_observation InlineReviewComments;

		auto operator<=>(const anonymous_observation_state&) const = default;
	};

	/// \brief Reports whether the mode can observe per-viewer state at all.
	///
	/// Viewer identity, thread resolution, viewed-file state, and
	/// actionability are all keyed to who is reading. Public preview has no
	/// reader to key them to, so it observes none of them.
	[[nodiscard]] auto observes_viewer_state(github_access_mode aMode) noexcept
		-> bool;

	/// \brief Yields the observations a public-preview projection carries.
	///
	/// Each one is the unavailable state. There is no defaulted answer and no
	/// synthetic viewer: an unavailable observation is a state a projection
	/// must render as unavailable rather than as a value.
	[[nodiscard]] auto anonymous_observations() noexcept
		-> anonymous_observation_state;

	/// \brief Yields the capability shape a public-preview profile carries.
	///
	/// Every capability is unsupported by policy rather than unknown: nothing
	/// is probed, because the mode cannot support any of them.
	[[nodiscard]] auto anonymous_capabilities() noexcept -> github_capabilities;

	/// \brief Names the intent the supplied capability gates.
	[[nodiscard]] auto gated_intent(github_capability aCapability) noexcept
		-> anonymous_read_intent;

	/// \brief Describes a change of access mode on one host.
	struct github_access_mode_change
	{
		github_access_mode Previous = github_access_mode::authenticated;
		github_access_mode Next     = github_access_mode::public_preview;

		auto operator<=>(const github_access_mode_change&) const = default;
	};

	/// \brief Reports whether the change crosses between the two modes.
	[[nodiscard]] auto crosses_modes(
		const github_access_mode_change& aChange) noexcept -> bool;

	/// \brief Yields the invalidation a mode change requires of the host.
	///
	/// A crossing change is an identity change: it yields the same request an
	/// account switch yields, carrying the advanced generation and the scope
	/// left behind, so the host applies one invalidation rule rather than two.
	/// The caller supplies the core-issued request identity; this policy has no
	/// second allocator. A change that stays in one mode yields nothing.
	[[nodiscard]] auto mode_change_invalidation(
		const github_access_mode_change& aChange,
		application_session_id           aApplicationSession,
		github_identity_generation       aCurrentGeneration,
		const github_identity_scope&     aPrevious,
		request_id                       aRequest)
		-> std::optional<invalidate_github_identity_request>;

	/// \brief Describes the public-preview request identity.
	///
	/// The mode is part of the identity, so a completion issued in one mode
	/// can never be admitted in the other. The context is the same one the
	/// authenticated course carries; this policy invents no second identity.
	struct anonymous_request_identity
	{
		github_access_mode           Mode = github_access_mode::public_preview;
		pull_request_request_context Context;

		auto operator<=>(const anonymous_request_identity&) const = default;
	};

	using anonymous_open_outcome
		= std::variant<anonymous_request_identity, anonymous_refusal>;

	enum class anonymous_completion_admission : std::uint8_t {
		accepted,
		stale_completion,
	};

	/// \brief Describes the public-preview read policy state.
	struct anonymous_read_policy_state
	{
		github_access_mode                        Mode = github_access_mode::public_preview;
		application_session_id                    ApplicationSession;
		github_identity_generation                IdentityGeneration;
		github_identity_scope                     Scope;
		std::optional<anonymous_request_identity> Pending;
		std::optional<anonymous_refusal>          Terminal;

		auto operator<=>(const anonymous_read_policy_state&) const = default;
	};

	/// \brief Opens the read the supplied intent needs.
	///
	/// A refused intent yields the refusal and no identity at all, which is
	/// what refusing before construction means here: there is nothing from
	/// which a request could be built. A recorded terminal refusal answers
	/// every later intent, so nothing retries past it.
	[[nodiscard]] auto open_read(
		anonymous_read_policy_state&        aState,
		anonymous_read_intent               aIntent,
		const pull_request_request_context& aContext)
		-> anonymous_open_outcome;

	/// \brief Admits a completion against the pending request.
	///
	/// A completion is admitted when its identity is the pending one and that
	/// identity still belongs to this session and generation. Admission clears
	/// the pending request, so a repeat of an admitted completion is stale by
	/// the same rule that rejects a completion from an abandoned generation.
	[[nodiscard]] auto admit(
		anonymous_read_policy_state&      aState,
		const anonymous_request_identity& aObserved)
		-> anonymous_completion_admission;

	/// \brief Records the budget refusal that ends the request.
	[[nodiscard]] auto exhaust(
		anonymous_read_policy_state&     aState,
		const anonymous_rate_limit_fact& aFact) -> anonymous_refusal;

	/// \brief Changes the mode and yields the invalidation the host applies.
	[[nodiscard]] auto change_mode(
		anonymous_read_policy_state& aState,
		github_access_mode           aNext,
		request_id                   aRequest)
		-> std::optional<invalidate_github_identity_request>;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_ANONYMOUS_READ_POLICY_HPP
