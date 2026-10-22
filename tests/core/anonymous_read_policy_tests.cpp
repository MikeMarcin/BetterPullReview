#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_transition.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	using intent = core::anonymous_read_intent;
	using mode   = core::github_access_mode;
	using reason = core::anonymous_refusal_reason;

	constexpr std::array<intent, 9> legal_intents{
		intent::read_repository,
		intent::read_pull_request,
		intent::read_changed_files,
		intent::read_commits,
		intent::read_reviews,
		intent::read_review_comments,
		intent::read_issue_comments,
		intent::read_checks,
		intent::search_pull_requests,
	};

	struct refusal_case
	{
		intent Intent = intent::read_review_threads;
		reason Reason = reason::graphql_refused;
	};

	constexpr std::array<refusal_case, 10> refused_intents{
		refusal_case{ intent::read_review_threads, reason::graphql_refused },
		refusal_case{
			intent::read_viewer_identity,
			reason::viewer_identity_refused },
		refusal_case{ intent::resolve_review_thread, reason::mutation_refused },
		refusal_case{
			intent::unresolve_review_thread,
			reason::mutation_refused },
		refusal_case{ intent::set_viewed_file_state, reason::mutation_refused },
		refusal_case{
			intent::publish_review_comment,
			reason::mutation_refused },
		refusal_case{ intent::submit_review, reason::mutation_refused },
		refusal_case{
			intent::read_private_repository,
			reason::private_or_missing },
		refusal_case{ intent::select_enterprise_host, reason::ghes_unavailable },
		// [[BPR-1741]].
		refusal_case{ intent::react_to_comment, reason::mutation_refused },
	};

	[[nodiscard]] auto sample_scope() -> core::github_identity_scope
	{
		return core::github_identity_scope{
			.Profile     = core::host_profile_id{ 11U },
			.Host        = core::git_host_id{ 13U },
			.Account     = core::github_account_id{ 17U },
			.Repository  = core::repository_id{ 19U },
			.PullRequest = core::pull_request_id{ 23U },
			.Snapshot    = core::provider_snapshot_id{ 29U },
		};
	}

	/// Public preview carries no account. The zero id below is not what says
	/// so — the mode is — and no test here reads it.
	[[nodiscard]] auto sample_context() -> core::pull_request_request_context
	{
		return core::pull_request_request_context{
			.Request             = core::request_id{ 5U },
			.ApplicationSession  = core::application_session_id{ 7U },
			.Profile             = core::host_profile_id{ 11U },
			.Host                = core::git_host_id{ 13U },
			.Account             = core::github_account_id{ },
			.Repository          = core::repository_locator{ .Owner = "owner",
															 .Name  = "repo" },
			.Number              = core::pull_request_number{ 42U },
			.ExpectedRepository  = std::nullopt,
			.ExpectedPullRequest = std::nullopt,
			.Capabilities        = core::capability_revision{ 3U },
			.IdentityGeneration  = core::github_identity_generation{ 2U },
			.Generation          = core::request_generation{ 1U },
		};
	}

	[[nodiscard]] auto fresh_policy() -> core::anonymous_read_policy_state
	{
		auto state               = core::anonymous_read_policy_state{ };
		state.Mode               = mode::public_preview;
		state.ApplicationSession = core::application_session_id{ 7U };
		state.IdentityGeneration = core::github_identity_generation{ 2U };
		state.Scope              = sample_scope();
		return state;
	}

	[[nodiscard]] auto refusal_of(const core::anonymous_intent_legality& aValue)
		-> std::optional<core::anonymous_refusal>
	{
		if (const auto* refused
			= std::get_if<core::anonymous_intent_refused>(&aValue)) {
			return refused->Refusal;
		}
		return std::nullopt;
	}

	void test_every_legal_intent_is_accepted()
	{
		for (const auto legalIntent : legal_intents) {
			expect(
				core::legal(legalIntent)
					&& std::holds_alternative<core::anonymous_intent_legal>(
						core::legality(legalIntent)),
				"every credential-free read intent is legal in public preview");

			auto       state   = fresh_policy();
			const auto outcome = core::open_read(
				state,
				legalIntent,
				sample_context());
			expect(
				std::holds_alternative<core::anonymous_request_identity>(
					outcome)
					&& state.Pending.has_value(),
				"a legal intent opens a request identity in public preview");
			expect(
				std::get<core::anonymous_request_identity>(outcome).Mode
					== mode::public_preview,
				"an opened identity carries the mode it was opened in");
		}
	}

	void test_every_illegal_intent_is_refused_before_construction()
	{
		for (const auto refusedCase : refused_intents) {
			const auto refusal = refusal_of(core::legality(refusedCase.Intent));
			expect(
				refusal.has_value() && refusal->Reason == refusedCase.Reason,
				"every refused intent carries its own typed reason");
			expect(
				!core::legal(refusedCase.Intent),
				"a refused intent is never legal");

			auto       state   = fresh_policy();
			const auto outcome = core::open_read(
				state,
				refusedCase.Intent,
				sample_context());
			expect(
				std::holds_alternative<core::anonymous_refusal>(outcome)
					&& std::get<core::anonymous_refusal>(outcome).Reason
						== refusedCase.Reason,
				"a refused intent yields the refusal rather than an identity");
			expect(
				!state.Pending.has_value(),
				"a refused intent leaves nothing a request could be built from");
		}
	}

	void test_the_mode_and_not_the_account_decides()
	{
		auto context    = sample_context();
		context.Account = core::github_account_id{ 101U };
		for (const auto refusedCase : refused_intents) {
			auto       state   = fresh_policy();
			const auto outcome = core::open_read(
				state,
				refusedCase.Intent,
				context);
			expect(
				std::holds_alternative<core::anonymous_refusal>(outcome),
				"a populated account never makes a public-preview intent legal");
		}
		for (const auto legalIntent : legal_intents) {
			expect(
				std::holds_alternative<core::anonymous_intent_legal>(
					core::legality(mode::authenticated, legalIntent)),
				"the authenticated mode refuses nothing in this policy");
		}
		for (const auto refusedCase : refused_intents) {
			expect(
				std::holds_alternative<core::anonymous_intent_legal>(
					core::legality(mode::authenticated, refusedCase.Intent)),
				"the refusals belong to public preview, not to every mode");
		}
	}

	void test_the_intent_sweep_covers_every_enumerator()
	{
		static_assert(
			core::anonymous_read_intent_catalog.size()
				== core::anonymous_read_intent_count,
			"the catalog holds every intent exactly once");
		expect(
			legal_intents.size() + refused_intents.size()
				== core::anonymous_read_intent_count,
			"the legal and refused tables together cover the catalog");

		std::uint32_t seen = 0U;
		for (std::size_t index = 0U;
			 index < core::anonymous_read_intent_catalog.size();
			 ++index) {
			const auto swept = core::anonymous_read_intent_catalog.at(index);
			expect(
				core::catalog_index(swept) == index,
				"each catalog entry reports its own position");
			const auto bit = static_cast<std::uint32_t>(1U) << index;
			expect((seen & bit) == 0U, "no intent appears in the catalog twice");
			seen |= bit;

			const auto decision = core::legality(swept);
			const auto refusal  = refusal_of(decision);
			auto       counted  = std::size_t{ 0U };
			for (const auto legalIntent : legal_intents) {
				if (legalIntent == swept) {
					++counted;
				}
			}
			for (const auto refusedCase : refused_intents) {
				if (refusedCase.Intent == swept) {
					++counted;
				}
			}
			expect(
				counted == 1U,
				"every enumerated intent is named by exactly one table");
			expect(
				refusal.has_value() != core::legal(swept),
				"every enumerated intent is either legal or refused");
		}

		for (std::size_t index = 0U;
			 index < core::anonymous_read_intent_count;
			 ++index) {
			const auto value
				= static_cast<intent>(static_cast<std::uint8_t>(index));
			expect(
				core::catalog_index(value) < core::anonymous_read_intent_count,
				"every enumerator value is placed in the catalog");
		}
	}

	void test_every_refusal_offers_the_sign_in_handoff()
	{
		static_assert(
			core::anonymous_refusal::SignInOffered,
			"the sign-in handoff is a property of the refusal type");

		for (const auto refusedCase : refused_intents) {
			const auto refusal = refusal_of(core::legality(refusedCase.Intent));
			expect(
				refusal.has_value() && core::anonymous_refusal::SignInOffered
					&& core::well_formed(*refusal),
				"a structural refusal offers the sign-in handoff");
		}

		auto       state = fresh_policy();
		const auto rate  = core::exhaust(
			state,
			core::anonymous_rate_limit_fact{ .Limit = 60U, .Reset = 1729U });
		expect(
			core::anonymous_refusal::SignInOffered && core::well_formed(rate),
			"the budget refusal offers the sign-in handoff");
	}

	void test_each_refusal_reason_is_reachable_and_distinct()
	{
		std::uint32_t seen = 0U;
		for (const auto refusedCase : refused_intents) {
			const auto refusal = refusal_of(core::legality(refusedCase.Intent));
			expect(refusal.has_value(), "a refused intent yields a refusal");
			seen |= static_cast<std::uint32_t>(1U)
				<< static_cast<std::uint32_t>(refusal->Reason);
			expect(
				!refusal->Budget.has_value(),
				"only the budget refusal carries a budget fact");
		}

		auto       state = fresh_policy();
		const auto rate  = core::exhaust(
			state,
			core::anonymous_rate_limit_fact{ .Limit = 60U, .Reset = 1729U });
		seen |= static_cast<std::uint32_t>(1U)
			<< static_cast<std::uint32_t>(rate.Reason);

		expect(
			seen
				== ((static_cast<std::uint32_t>(1U)
					 << static_cast<std::uint32_t>(reason::rate_limited))
					| (static_cast<std::uint32_t>(1U)
					   << static_cast<std::uint32_t>(reason::private_or_missing))
					| (static_cast<std::uint32_t>(1U)
					   << static_cast<std::uint32_t>(reason::ghes_unavailable))
					| (static_cast<std::uint32_t>(1U)
					   << static_cast<std::uint32_t>(reason::graphql_refused))
					| (static_cast<std::uint32_t>(1U)
					   << static_cast<std::uint32_t>(
							  reason::viewer_identity_refused))
					| (static_cast<std::uint32_t>(1U)
					   << static_cast<std::uint32_t>(
							  reason::mutation_refused))),
			"every refusal reason is reachable from this policy");
	}

	void test_the_private_refusal_is_existence_neutral()
	{
		const auto missing = refusal_of(
			core::legality(intent::read_private_repository));
		expect(
			missing.has_value()
				&& missing->Reason == reason::private_or_missing,
			"one reason answers both the private and the absent repository");
		expect(
			*missing == core::refuse(reason::private_or_missing),
			"the refusal carries nothing that could assert existence");
	}

	void test_the_budget_refusal_is_terminal_and_names_its_limit()
	{
		auto       state = fresh_policy();
		const auto fact
			= core::anonymous_rate_limit_fact{ .Limit = 60U, .Reset = 1729U };
		const auto refusal = core::exhaust(state, fact);
		expect(
			refusal.Reason == reason::rate_limited
				&& refusal.Budget.has_value() && *refusal.Budget == fact,
			"the budget refusal names the limit and the reset it was handed");
		expect(
			core::terminal(refusal) && core::well_formed(refusal),
			"the budget refusal is terminal");
		expect(
			!state.Pending.has_value(),
			"exhaustion abandons the request in flight");

		for (const auto legalIntent : legal_intents) {
			const auto outcome = core::open_read(
				state,
				legalIntent,
				sample_context());
			expect(
				std::holds_alternative<core::anonymous_refusal>(outcome)
					&& std::get<core::anonymous_refusal>(outcome) == refusal
					&& !state.Pending.has_value(),
				"nothing retries past the budget refusal");
		}

		for (const auto structuralCase : refused_intents) {
			const auto structural = refusal_of(
				core::legality(structuralCase.Intent));
			expect(
				structural.has_value() && !core::terminal(*structural),
				"only the budget refusal ends the request");
		}
	}

	void test_stale_and_duplicate_completions_are_rejected()
	{
		auto       state   = fresh_policy();
		const auto outcome = core::open_read(
			state,
			intent::read_pull_request,
			sample_context());
		const auto identity
			= std::get<core::anonymous_request_identity>(outcome);

		auto stale            = identity;
		stale.Context.Request = core::request_id{ 6U };
		expect(
			core::admit(state, stale)
				== core::anonymous_completion_admission::stale_completion,
			"a completion for another request is stale");

		auto crossed = identity;
		crossed.Mode = mode::authenticated;
		expect(
			core::admit(state, crossed)
				== core::anonymous_completion_admission::stale_completion,
			"a completion issued in the other mode is stale");

		auto regenerated = identity;
		regenerated.Context.IdentityGeneration
			= core::github_identity_generation{ 9U };
		expect(
			core::admit(state, regenerated)
				== core::anonymous_completion_admission::stale_completion,
			"a completion from an abandoned generation is stale");

		expect(
			core::admit(state, identity)
				== core::anonymous_completion_admission::accepted,
			"the pending completion is admitted exactly once");
		expect(
			core::admit(state, identity)
				== core::anonymous_completion_admission::stale_completion,
			"a repeat of an admitted completion is stale");

		auto empty = fresh_policy();
		expect(
			core::admit(empty, identity)
				== core::anonymous_completion_admission::stale_completion,
			"a completion without a pending request is stale");
	}

	[[nodiscard]] auto account_switch_invalidation()
		-> core::invalidate_github_identity_request
	{
		auto state               = core::github_state{ };
		state.NextRequest        = core::request_id{ 37U };
		state.ApplicationSession = core::application_session_id{ 7U };
		state.IdentityGeneration = core::github_identity_generation{ 2U };
		const auto scope         = sample_scope();
		state.ActiveProfile      = scope.Profile;
		state.Host               = scope.Host;
		state.Account            = scope.Account;
		state.Repository         = scope.Repository;
		state.PullRequest        = scope.PullRequest;
		state.AcceptedSnapshot   = scope.Snapshot;

		const auto output = core::transition(
			state,
			core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 31U } });
		expect(
			!output.Requests.empty(),
			"an account switch emits the invalidation the host applies");
		return std::get<core::invalidate_github_identity_request>(
			output.Requests.front());
	}

	void test_a_mode_switch_invalidates_as_an_account_switch_does()
	{
		const auto expected = account_switch_invalidation();

		auto       into    = fresh_policy();
		const auto leaving = core::change_mode(into, mode::authenticated, expected.Request);
		expect(
			leaving.has_value() && *leaving == expected,
			"leaving public preview yields the account-switch invalidation");
		expect(
			into.Mode == mode::authenticated
				&& into.IdentityGeneration == expected.NewGeneration
				&& into.Scope == core::github_identity_scope{ },
			"leaving public preview advances the generation and clears scope");

		auto back           = fresh_policy();
		back.Mode           = mode::authenticated;
		const auto entering = core::change_mode(back, mode::public_preview, expected.Request);
		expect(
			entering.has_value() && *entering == expected,
			"entering public preview yields the same invalidation");

		auto       unchanged = fresh_policy();
		const auto before    = unchanged;
		expect(
			!core::change_mode(unchanged, mode::public_preview, core::request_id{ 23U }).has_value()
				&& unchanged == before,
			"a change that stays in one mode invalidates nothing");

		auto       switching = fresh_policy();
		const auto outcome   = core::open_read(
			switching,
			intent::read_pull_request,
			sample_context());
		const auto identity
			= std::get<core::anonymous_request_identity>(outcome);
		static_cast<void>(core::change_mode(switching, mode::authenticated, expected.Request));
		expect(
			core::admit(switching, identity)
				== core::anonymous_completion_admission::stale_completion,
			"a completion begun before the switch is stale after it");

		auto exhausted = fresh_policy();
		static_cast<void>(core::exhaust(
			exhausted,
			core::anonymous_rate_limit_fact{ .Limit = 60U, .Reset = 1729U }));
		static_cast<void>(core::change_mode(exhausted, mode::authenticated, expected.Request));
		expect(
			!exhausted.Terminal.has_value(),
			"signing in clears the budget refusal the signed-out mode carried");
	}

	void test_the_public_preview_capability_shape_is_all_false()
	{
		constexpr std::array<core::github_capability, 4> capabilities{
			core::github_capability::review_threads,
			core::github_capability::viewed_file_mutations,
			core::github_capability::resolve_thread,
			core::github_capability::unresolve_thread,
		};

		const auto shape = core::anonymous_capabilities();
		for (const auto capability : capabilities) {
			expect(
				core::capability_state(shape, capability)
					== core::github_capability_state::unsupported,
				"every public-preview capability is unsupported by policy");
			expect(
				!core::capability_supported(shape, capability),
				"no public-preview capability reads as supported");
			expect(
				!core::legal(core::gated_intent(capability)),
				"no capability-gated action is legal in public preview");
		}
	}

	void test_unavailable_observations_are_states_and_not_defaults()
	{
		const auto observed = core::anonymous_observations();
		expect(
			observed.Viewer == core::anonymous_viewer_identity::unavailable
				&& observed.ThreadResolution
					== core::anonymous_thread_resolution::unavailable
				&& observed.ViewedFile
					== core::anonymous_viewed_file_state::unavailable
				&& observed.Actionability
					== core::anonymous_actionability::unavailable,
			"public preview observes no viewer, resolution, viewed state, or "
			"actionability");
		expect(
			observed == core::anonymous_observation_state{ },
			"the unavailable state is what a value-initialized observation is");

		static_assert(
			static_cast<std::uint8_t>(
				core::anonymous_viewer_identity::unavailable)
				== 0U,
			"no synthetic viewer can be the default");
		static_assert(
			static_cast<std::uint8_t>(
				core::anonymous_thread_resolution::unavailable)
				== 0U,
			"no resolution can be the default");
		static_assert(
			static_cast<std::uint8_t>(
				core::anonymous_viewed_file_state::unavailable)
				== 0U,
			"no viewed state can be the default");
		static_assert(
			static_cast<std::uint8_t>(core::anonymous_actionability::unavailable)
				== 0U,
			"no actionability can be the default");

		expect(
			core::observes_viewer_state(mode::authenticated)
				&& !core::observes_viewer_state(mode::public_preview),
			"only the authenticated mode observes per-viewer state");
	}

	void test_inline_review_comments_are_unavailable_and_not_empty()
	{
		const auto unread = core::unobservable_inline_review_comments();
		const auto none   = core::observed_inline_review_comments(0U);
		const auto some   = core::observed_inline_review_comments(3U);

		expect(
			unread != none,
			"an unobserved conversation is not an observed empty one");
		expect(
			!core::observable(unread) && !core::observed_none(unread),
			"an unobserved conversation reports neither content nor emptiness");
		expect(
			core::observable(none) && core::observed_none(none),
			"an observed empty conversation reports that it is empty");
		expect(
			core::observable(some) && !core::observed_none(some)
				&& some.Count == 3U,
			"an observed conversation reports what it holds");
		expect(
			core::well_formed(unread) && core::well_formed(none)
				&& core::well_formed(some),
			"each built observation is consistent");
		expect(
			!core::well_formed(core::inline_review_comment_observation{
				.Availability
				= core::anonymous_observation_availability::unavailable,
				.Count = 4U }),
			"an unobserved conversation cannot carry a count");
		expect(
			core::anonymous_observations().InlineReviewComments == unread,
			"public preview never observed the inline conversation");
	}

	/// The REST inline conversation and the GraphQL thread read are different
	/// intents with different answers, and collapsing them would make a
	/// refusal look like an answer. Reading the comments is legal signed out;
	/// reading the threads is refused, and so is everything keyed to the
	/// reader.
	void test_the_rest_conversation_is_legal_while_threads_are_refused()
	{
		expect(
			core::legal(intent::read_review_comments),
			"the REST inline review conversation is reachable signed out");
		const auto threads = refusal_of(
			core::legality(intent::read_review_threads));
		expect(
			threads.has_value()
				&& threads->Reason == reason::graphql_refused
				&& !core::legal(intent::read_review_threads),
			"the GraphQL thread read stays refused for its own reason");
		expect(
			!core::legal(intent::read_viewer_identity)
				&& !core::legal(intent::resolve_review_thread)
				&& !core::legal(intent::unresolve_review_thread)
				&& !core::legal(intent::set_viewed_file_state),
			"reading the conversation grants no resolution, no viewed state, "
			"and no viewer");
		expect(
			core::catalog_index(intent::read_review_comments)
				!= core::catalog_index(intent::read_review_threads),
			"the two intents occupy distinct catalog positions");
		expect(
			legal_intents.size() == 9U,
			"nine intents mirror the nine credential-free read operations");
	}
}

auto main() -> int
{
	test_every_legal_intent_is_accepted();
	test_every_illegal_intent_is_refused_before_construction();
	test_the_mode_and_not_the_account_decides();
	test_the_intent_sweep_covers_every_enumerator();
	test_every_refusal_offers_the_sign_in_handoff();
	test_each_refusal_reason_is_reachable_and_distinct();
	test_the_private_refusal_is_existence_neutral();
	test_the_budget_refusal_is_terminal_and_names_its_limit();
	test_stale_and_duplicate_completions_are_rejected();
	test_a_mode_switch_invalidates_as_an_account_switch_does();
	test_the_public_preview_capability_shape_is_all_false();
	test_unavailable_observations_are_states_and_not_defaults();
	test_inline_review_comments_are_unavailable_and_not_empty();
	test_the_rest_conversation_is_legal_while_threads_are_refused();
	std::cout << "anonymous public read core tests passed\n";
	return EXIT_SUCCESS;
}
