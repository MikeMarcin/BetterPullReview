/// \file
/// \brief Proves a session stays pinned to what it was asked about.

#include "bpr/core/agent_session.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <tuple>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto binding() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 10U },
			.Repository  = core::repository_id{ 20U },
			.PullRequest = core::pull_request_id{ 30U },
			.BaseOid     = commit(1U),
			.HeadOid     = commit(2U),
			.LeftEpoch   = core::epoch_ordinal{ 1 },
			.RightEpoch  = core::epoch_ordinal{ 2 },
		};
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 7U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = binding(),
		};
	}

	/// \brief A moved head makes a session stale, and never retargets it.
	void test_head_movement_makes_a_session_stale()
	{
		const auto pinned = session();
		auto       moved  = binding();
		moved.HeadOid     = commit(9U);

		const auto status = core::observe_agent_session(pinned, moved);
		expect(
			!status.Current
				&& status.Staleness == core::agent_session_staleness::head_moved,
			"a session pinned to one head is stale when the head moves");
		expect(
			pinned.Binding == binding(),
			"and the session still carries the identity it was started with, "
			"rather than adopting the one it was compared against");
		expect(
			core::continue_against_binding(pinned).HeadOid == commit(2U),
			"continuing answers about the revision the agent was asked about");
		expect(
			core::continue_against_binding(pinned).HeadOid != moved.HeadOid,
			"which is not the revision now under review, so the two are "
			"distinguishable rather than quietly the same");
	}

	/// \brief A stale session refuses to be treated as current.
	void test_a_stale_session_refuses_to_be_current()
	{
		const auto pinned = session();
		auto       moved  = binding();
		moved.HeadOid     = commit(9U);

		expect(
			core::may_treat_as_current(pinned, binding()),
			"an unmoved revision is current, so the refusal below is about the "
			"movement and not about the function always refusing");
		expect(
			!core::may_treat_as_current(pinned, moved),
			"a stale session refuses to be treated as current");
		// Continuing is still available, and is a separate call. That is the
		// point: "use the old answer" is never what happens by default.
		expect(
			core::continue_against_binding(pinned) == pinned.Binding,
			"continuing against the old revision remains possible, as a "
			"distinct explicit choice");
	}

	/// \brief Two sessions differing only in epoch pair are different sessions.
	void test_the_epoch_pair_is_part_of_the_identity()
	{
		const auto pinned     = session();
		auto       reselected = binding();
		reselected.RightEpoch = core::epoch_ordinal{ 5 };

		expect(
			reselected.HeadOid == pinned.Binding.HeadOid
				&& reselected.BaseOid == pinned.Binding.BaseOid,
			"the two bindings agree on every revision, so the difference below "
			"is the epoch pair alone");
		expect(
			reselected != pinned.Binding,
			"two bindings differing only in epoch pair are different bindings");
		const auto status = core::observe_agent_session(pinned, reselected);
		expect(
			!status.Current
				&& status.Staleness
					== core::agent_session_staleness::epoch_pair_changed,
			"and a session pinned to one epoch pair is stale against the "
			"other, because it was asked a different question");
	}

	/// \brief A stale session offers two continuations and retargets neither.
	void test_a_stale_session_offers_exactly_two_continuations()
	{
		static_assert(
			core::agent_continuation_count == 2U,
			"there are two things to do with a session that no longer matches. "
			"A third would be retargeting it, which is what the binding exists "
			"to prevent.");

		const auto pinned     = session();
		auto       moved      = binding();
		moved.HeadOid         = commit(9U);
		auto reselected       = binding();
		reselected.RightEpoch = core::epoch_ordinal{ 5 };
		auto elsewhere        = binding();
		elsewhere.PullRequest = core::pull_request_id{ 31U };

		struct staleness_row
		{
			core::agent_revision_binding  Observed;
			core::agent_session_staleness Staleness;
			bool                          Required;
		};
		const auto rows = std::array{
			staleness_row{
				moved, core::agent_session_staleness::head_moved, true },
			staleness_row{
				reselected,
				core::agent_session_staleness::epoch_pair_changed,
				true },
			staleness_row{
				elsewhere,
				core::agent_session_staleness::different_review,
				true },
			staleness_row{
				binding(), core::agent_session_staleness::current, false },
		};

		for (const auto& row : rows) {
			const auto offer = core::offer_agent_continuations(pinned, row.Observed);
			static_assert(
				std::tuple_size_v<decltype(offer.Choices)>
					== core::agent_continuation_count,
				"the offer names exactly two continuations, and the array is "
				"what says so rather than a branch that could append a third");
			expect(
				offer.Status
					== core::observe_agent_session(pinned, row.Observed),
				"the offer carries the verdict observe_agent_session returned, "
				"so there is one staleness vocabulary and not two");
			expect(
				offer.Status.Staleness == row.Staleness
					&& offer.Required == row.Required,
				"a session that no longer matches requires a choice, and a "
				"current one does not");
			expect(
				offer.Choices[0] == core::agent_continuation::start_new_session
					&& offer.Choices[1]
						== core::agent_continuation::continue_against_old_revision
					&& offer.Choices[0] != offer.Choices[1],
				"the two choices are asking again about what you are looking at, "
				"and keeping this answer about what it was asked");
			expect(
				offer.ContinueAgainst == pinned.Binding
					&& offer.ContinueAgainst
						== core::continue_against_binding(pinned),
				"and continuing answers about the session's own binding");
			expect(
				row.Observed == pinned.Binding
					|| offer.ContinueAgainst != row.Observed,
				"which is never the revision now under review, so offering the "
				"choice does not quietly take it");
			expect(
				pinned.Binding == binding(),
				"offering a continuation retargets nothing: the session still "
				"carries the identity it was started with");
			expect(
				!core::stable_name(offer.Choices[0]).empty()
					&& !core::stable_name(offer.Choices[1]).empty(),
				"both continuations have stable names a drawer can label");
		}
	}

	/// \brief A session about another review is not stale, it is elsewhere.
	void test_another_review_is_not_staleness()
	{
		const auto pinned = session();
		auto       other  = binding();
		other.PullRequest = core::pull_request_id{ 31U };
		const auto status = core::observe_agent_session(pinned, other);
		expect(
			status.Staleness == core::agent_session_staleness::different_review,
			"a session bound to another review reports that, rather than "
			"reporting a moved head and inviting a refresh that could not "
			"mean anything");
	}
} // namespace

auto main() -> int
{
	test_head_movement_makes_a_session_stale();
	test_a_stale_session_refuses_to_be_current();
	test_the_epoch_pair_is_part_of_the_identity();
	test_a_stale_session_offers_exactly_two_continuations();
	test_another_review_is_not_staleness();
	std::cout << "agent-session-tests:pass\n";
	return EXIT_SUCCESS;
}
