#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	struct agent_session_id_tag;

	using agent_session_id = strong_id<agent_session_id_tag>;

	/// \brief The exact revision one agent session was started against.
	///
	/// \details Immutable by construction: every member is part of the value,
	/// and `operator<=>` compares all of them. Two bindings differing in any
	/// field — including the epoch pair — are different bindings, and therefore
	/// different sessions.
	///
	/// The epoch pair is part of the identity rather than a detail of it,
	/// because an agent asked about "what changed since the last review" and
	/// one asked about "what changed in the whole pull request" were asked
	/// different questions even where the head is the same.
	struct agent_revision_binding
	{
		git_host_id     Host;
		repository_id   Repository;
		pull_request_id PullRequest;
		git_commit_oid  BaseOid;
		git_commit_oid  HeadOid;
		epoch_ordinal   LeftEpoch;
		epoch_ordinal   RightEpoch;

		auto operator<=>(const agent_revision_binding&) const = default;
	};

	/// \brief Why a session is no longer current.
	enum class agent_session_staleness : std::uint8_t {
		/// The binding still matches what the reviewer is looking at.
		current,
		/// The pull request's head has moved since the session started.
		head_moved,
		/// The reviewer selected a different epoch pair.
		epoch_pair_changed,
		/// The session is bound to a different review entirely.
		different_review,
	};

	[[nodiscard]] auto stable_name(agent_session_staleness aStaleness) noexcept
		-> std::string_view;

	/// \brief One explicit agent invocation, pinned to what it was asked about.
	///
	/// \details The binding is `const` in effect: nothing in this header
	/// mutates it, and `observe` returns a verdict rather than retargeting.
	///
	/// **Head movement makes a session stale; it never silently retargets it.**
	/// An agent asked about one revision and answering about another would be
	/// answering a question nobody put, and the reviewer would have no way to
	/// tell which revision the answer was about.
	struct agent_session
	{
		agent_session_id       Id;
		agent_profile_id       Profile;
		agent_revision_binding Binding;

		auto operator<=>(const agent_session&) const = default;
	};

	/// \brief Checks whether the supplied session satisfies its contract.
	///
	/// \details A usable session names itself, names the profile it was
	/// started with, and is pinned to a complete revision binding. Stated once
	/// here because two callers already needed it and a second copy is where
	/// the two answers start to differ.
	///
	/// Deliberately not another `valid` overload. "Is this session usable" is
	/// asked in more than one layer, about different things — this one is
	/// about the value being complete, not about what a store will accept —
	/// and one unqualified overload set spanning both is where the two
	/// questions get answered by the wrong function.
	[[nodiscard]] auto valid_agent_session(const agent_session& aSession) noexcept
		-> bool;

	/// \brief What one session may still be used for.
	struct agent_session_status
	{
		agent_session_staleness Staleness = agent_session_staleness::current;
		/// Whether the session may be treated as answering about what the
		/// reviewer is looking at now.
		bool Current = false;

		auto operator<=>(const agent_session_status&) const = default;
	};

	/// \brief Compares a session against the revision now under review.
	///
	/// \details Returns a verdict and changes nothing. The session keeps the
	/// identity it was started with, so a caller that wants an answer about the
	/// new revision has to start a new session — which is a distinct explicit
	/// choice, and visible as one.
	[[nodiscard]] auto observe_agent_session(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept
		-> agent_session_status;

	/// \brief Whether a session may be treated as current.
	///
	/// \details A stale session refuses. Continuing against the revision it was
	/// pinned to is legitimate and is a separate decision the caller makes with
	/// `continue_against_binding`, so that "use the old answer" is never
	/// something that happens by default.
	[[nodiscard]] auto may_treat_as_current(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept -> bool;

	/// \brief The binding a stale session may still be continued against.
	///
	/// \returns The session's own binding, always — a stale session answers
	/// about what it was asked about. Named as a function so that continuing
	/// is a call a reader can find, rather than a field access that looks like
	/// reading current state.
	[[nodiscard]] auto continue_against_binding(
		const agent_session& aSession) noexcept -> agent_revision_binding;

	/// \brief What a reviewer may do with a session that no longer matches.
	///
	/// \details Two, and there is no third. Either ask again about what you
	/// are looking at now, which is a new session with a new binding, or keep
	/// talking to this one about the revision it was asked about. The third
	/// choice a product usually grows here — "keep this session, point it at
	/// the new head" — is the silent retargeting `agent_session` exists to
	/// prevent, so it is not an enumerator that could be added by accident.
	enum class agent_continuation : std::uint8_t {
		start_new_session,
		continue_against_old_revision,
	};

	/// \brief The number of declared continuations.
	inline constexpr std::size_t agent_continuation_count = 2U;

	static_assert(
		static_cast<std::size_t>(agent_continuation::continue_against_old_revision)
				+ 1U
			== agent_continuation_count,
		"agent_continuation_count must cover every declared continuation");

	[[nodiscard]] auto stable_name(agent_continuation aContinuation) noexcept
		-> std::string_view;

	/// \brief The choices one session offers against the revision now under review.
	///
	/// \details `Choices` is a fixed-size array, so "exactly two" is a
	/// property of the type rather than of a branch that could one day append
	/// to a vector. `Required` says whether the reviewer has to choose at all;
	/// the choices themselves do not change with the answer, because what can
	/// be done with a session does not depend on why it stopped matching.
	///
	/// `ContinueAgainst` is the session's own binding, unconditionally. This
	/// type has no field the observed binding could occupy, which is how
	/// "retargets neither" survives somebody deciding it would be convenient.
	struct agent_continuation_offer
	{
		/// The verdict `observe_agent_session` returned, carried whole. There
		/// is exactly one staleness vocabulary and this is it.
		agent_session_status                                     Status;
		bool                                                     Required = false;
		std::array<agent_continuation, agent_continuation_count> Choices  = {
			agent_continuation::start_new_session,
			agent_continuation::continue_against_old_revision,
		};
		agent_revision_binding ContinueAgainst;

		auto operator<=>(const agent_continuation_offer&) const = default;
	};

	/// \brief Offers the continuations one session has, and retargets nothing.
	///
	/// \details Decided from `observe_agent_session`, whose verdict it carries
	/// rather than recomputing. A current session still reports both choices
	/// and its own binding; `Required` is what says a stale one has to be
	/// answered before the reviewer keeps going.
	[[nodiscard]] auto offer_agent_continuations(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept
		-> agent_continuation_offer;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_HPP
