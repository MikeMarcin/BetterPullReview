#include "bpr/core/conversation_mutation_lifecycle.hpp"
#include "bpr/core/conversation_reactions.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using core::reaction_content;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto group(const reaction_content aContent, const std::uint32_t aCount, const bool aMine, std::vector<std::string> aWho = { }) -> core::reaction_group
	{
		return core::reaction_group{ .Content = aContent, .Count = aCount, .ViewerReacted = aMine, .Reactors = std::move(aWho) };
	}

	void test_names_round_trip_in_github_order()
	{
		auto previous = std::string_view{ };
		for (std::size_t index = 0; index < core::reaction_contents.size(); ++index) {
			const auto content = core::reaction_contents[index];
			expect(static_cast<std::size_t>(content) == index, "the contents are listed in GitHub's order");
			expect(core::reaction_from_graphql_name(core::reaction_graphql_name(content)) == content, "every GraphQL name reads back as its content");
			expect(!core::reaction_emoji(content).empty() && !core::reaction_label(content).empty(), "every content has an emoji and a label");
			expect(core::reaction_graphql_name(content) != previous, "names are distinct");
			previous = core::reaction_graphql_name(content);
		}
		expect(core::reaction_graphql_name(reaction_content::thumbs_up) == "THUMBS_UP" && core::reaction_graphql_name(reaction_content::eyes) == "EYES", "GitHub's spelling");
		expect(!core::reaction_from_graphql_name("thumbs_up").has_value() && !core::reaction_from_graphql_name("PLUS_ONE").has_value()
				   && !core::reaction_from_graphql_name("").has_value(),
			   "any other text is no content");
		expect(core::reaction_emoji(reaction_content::thumbs_up) == "\xF0\x9F\x91\x8D", "thumbs up is U+1F44D");
	}

	void test_a_summary_is_canonical_or_refused()
	{
		const auto canonical = core::canonical_reaction_summary(core::reaction_summary{
			.ViewerCanReact = true,
			.Groups         = { group(reaction_content::eyes, 1U, false, { "a" }), group(reaction_content::laugh, 0U, false), group(reaction_content::thumbs_up, 3U, true, { "me", "b" }) },
		});
		expect(canonical.has_value() && canonical->Groups.size() == 2U && canonical->Groups[0].Content == reaction_content::thumbs_up
				   && canonical->Groups[1].Content == reaction_content::eyes,
			   "zero groups are dropped and the rest are in GitHub's order");
		expect(!core::canonical_reaction_summary({ .Groups = { group(reaction_content::heart, 1U, false), group(reaction_content::heart, 2U, false) } }).has_value(),
			   "a content twice is refused");
		expect(!core::canonical_reaction_summary({ .Groups = { group(reaction_content::heart, 0U, true) } }).has_value(), "a viewer reaction in an empty group is refused");
		expect(!core::canonical_reaction_summary({ .Groups = { group(reaction_content::heart, 1U, false, { "a", "b" }) } }).has_value(), "more names than reactions is refused");
		expect(!core::canonical_reaction_summary({ .Groups = { group(reaction_content::heart, 9U, false, { "a", "b", "c", "d", "e", "f" }) } }).has_value(),
			   "more than five names is refused");
		expect(!core::canonical_reaction_summary({ .Groups = { group(static_cast<reaction_content>(8U), 1U, false) } }).has_value(), "an unknown content is refused");
	}

	void test_a_reaction_is_refused_for_exactly_four_reasons()
	{
		using core::decide_reaction;
		using core::reaction_refusal;
		const auto allowed = core::reaction_input{ .Observed = true, .ViewerCanReact = true };
		expect(decide_reaction(allowed) == reaction_refusal::none, "an observed, permitted reaction is allowed");
		expect(decide_reaction({ .AnonymousRead = true }) == reaction_refusal::anonymous_read, "an anonymous read is refused first");
		expect(decide_reaction({ .ViewerCanReact = true }) == reaction_refusal::unobserved, "unread reactions are refused");
		expect(decide_reaction({ .Observed = true }) == reaction_refusal::not_permitted, "GitHub's viewerCanReact false is refused");
		expect(decide_reaction({ .Observed = true, .ViewerCanReact = true, .InFlight = true }) == reaction_refusal::in_flight, "an unanswered change refuses another");
		for (const auto refusal : { reaction_refusal::none, reaction_refusal::anonymous_read, reaction_refusal::unobserved, reaction_refusal::not_permitted, reaction_refusal::in_flight }) {
			expect(!core::name(refusal).empty(), "every refusal names itself");
		}
	}

	void test_a_change_shows_at_once_and_settles_on_github_s_answer()
	{
		const auto observed = core::reaction_summary{ .ViewerCanReact = true, .Groups = { group(reaction_content::thumbs_up, 2U, false, { "a", "b" }) } };
		auto       overlay  = core::conversation_overlay{ };
		expect(core::reaction_toggle(observed, reaction_content::thumbs_up), "a click on a reaction I have not made adds it");
		expect(core::begin_reaction_change(overlay, "review-comment:C1", reaction_content::thumbs_up, true), "the change begins");
		expect(!core::begin_reaction_change(overlay, "review-comment:C1", reaction_content::thumbs_up, false), "a second change to the same reaction waits");
		expect(core::reaction_in_flight(overlay, "review-comment:C1", reaction_content::thumbs_up) && !core::reaction_in_flight(overlay, "review-comment:C1", reaction_content::heart),
			   "only that reaction is in flight");

		auto shown = core::project_reaction_summary(&observed, overlay, "review-comment:C1", "me");
		expect(shown.has_value() && shown->Groups.size() == 1U && shown->Groups[0].Count == 3U && shown->Groups[0].ViewerReacted && shown->Groups[0].Pending
				   && shown->Groups[0].Reactors.front() == "me",
			   "the count, my tint and my name change at once, marked pending");
		expect(!core::reaction_toggle(shown, reaction_content::thumbs_up), "the next click would remove it");

		// GitHub's answer says someone else reacted meanwhile.
		core::confirm_reaction_change(overlay, "review-comment:C1", reaction_content::thumbs_up,
									  core::reaction_summary{ .ViewerCanReact = true, .Groups = { group(reaction_content::thumbs_up, 4U, true, { "me", "a", "b", "c" }) } });
		shown = core::project_reaction_summary(&observed, overlay, "review-comment:C1", "me");
		expect(shown.has_value() && shown->Groups[0].Count == 4U && shown->Groups[0].ViewerReacted && !shown->Groups[0].Pending,
			   "GitHub's answer replaces the read, and is no longer pending");

		// A read that began before the write arrives after it, without my reaction.
		core::observe_conversation(overlay, { .ReactionsObserved = true, .Reactions = { { "review-comment:C1", observed } } });
		shown = core::project_reaction_summary(&observed, overlay, "review-comment:C1", "me");
		expect(shown.has_value() && shown->Groups[0].Count == 3U && shown->Groups[0].ViewerReacted, "a stale read does not take my reaction away");
		// A read that agrees ends the change.
		const auto agreeing = core::reaction_summary{ .ViewerCanReact = true, .Groups = { group(reaction_content::thumbs_up, 5U, true, { "me" }) } };
		core::observe_conversation(overlay, { .ReactionsObserved = true, .Reactions = { { "review-comment:C1", agreeing } } });
		expect(overlay.Reactions.empty() && overlay.Answered.empty(), "an agreeing read settles the change");
		shown = core::project_reaction_summary(&agreeing, overlay, "review-comment:C1", "me");
		expect(shown == agreeing, "the read is shown as read");
	}

	void test_a_read_that_disagrees_twice_is_believed()
	{
		const auto observed = core::reaction_summary{ .ViewerCanReact = true, .Groups = { group(reaction_content::heart, 1U, true, { "me" }) } };
		auto       overlay  = core::conversation_overlay{ };
		expect(core::begin_reaction_change(overlay, "issue-comment:I1", reaction_content::heart, false), "removing my heart begins");
		auto shown = core::project_reaction_summary(&observed, overlay, "issue-comment:I1", "me");
		expect(shown.has_value() && shown->Groups.empty(), "the only heart was mine, so the group goes at once");
		core::confirm_reaction_change(overlay, "issue-comment:I1", reaction_content::heart, std::nullopt);
		core::observe_conversation(overlay, { .ReactionsObserved = true, .Reactions = { { "issue-comment:I1", observed } } });
		expect(overlay.Reactions.size() == 1U, "one disagreeing read is not believed");
		core::observe_conversation(overlay, { .ReactionsObserved = true, .Reactions = { { "issue-comment:I1", observed } } });
		expect(overlay.Reactions.empty(), "the second is");
		shown = core::project_reaction_summary(&observed, overlay, "issue-comment:I1", "me");
		expect(shown == observed, "and the comment shows what GitHub says");
	}

	void test_a_failed_change_restores_the_read()
	{
		const auto observed = core::reaction_summary{ .ViewerCanReact = true };
		auto       overlay  = core::conversation_overlay{ };
		expect(!core::project_reaction_summary(nullptr, overlay, "review:R1", "me").has_value(), "nothing read, nothing shown");
		expect(core::begin_reaction_change(overlay, "review:R1", reaction_content::rocket, true), "a rocket on a review begins");
		auto shown = core::project_reaction_summary(&observed, overlay, "review:R1", "me");
		expect(shown.has_value() && shown->Groups.size() == 1U && shown->Groups[0].Content == reaction_content::rocket && shown->Groups[0].Count == 1U,
			   "a new group appears at once");
		core::abandon_reaction_change(overlay, "review:R1", reaction_content::rocket);
		shown = core::project_reaction_summary(&observed, overlay, "review:R1", "me");
		expect(shown == observed && overlay.Reactions.empty(), "a change that failed leaves the read as it was");
		expect(!core::begin_reaction_change(overlay, "", reaction_content::rocket, true), "a nameless subject is no change");
		// A read without reactions keeps changes and answers alike.
		expect(core::begin_reaction_change(overlay, "review:R1", reaction_content::rocket, true), "begins again");
		core::confirm_reaction_change(overlay, "review:R1", reaction_content::rocket, core::reaction_summary{ .ViewerCanReact = true, .Groups = { group(reaction_content::rocket, 1U, true) } });
		core::observe_conversation(overlay, { .ReactionsObserved = false });
		expect(overlay.Reactions.size() == 1U && overlay.Answered.size() == 1U, "a read that did not see reactions changes nothing about them");
	}

	void test_a_resolution_shows_at_once_and_reconciles()
	{
		auto overlay = core::conversation_overlay{ };
		expect(core::project_thread_resolution(false, overlay, "review-thread:T1") == core::projected_resolution{ .Resolved = false }, "no change, the read");
		expect(core::begin_resolution_change(overlay, "review-thread:T1", true), "resolving begins");
		expect(!core::begin_resolution_change(overlay, "review-thread:T1", false), "and holds the thread until answered");
		expect(core::project_thread_resolution(false, overlay, "review-thread:T1") == core::projected_resolution{ .Resolved = true, .Pending = true }, "resolved at once, pending");
		core::confirm_resolution_change(overlay, "review-thread:T1");
		expect(core::project_thread_resolution(false, overlay, "review-thread:T1") == core::projected_resolution{ .Resolved = true }, "GitHub answered");
		core::observe_conversation(overlay, { .Resolved = { { "review-thread:T1", true } } });
		expect(overlay.Resolutions.empty(), "an agreeing read settles it");
		expect(core::begin_resolution_change(overlay, "review-thread:T1", false), "reopening begins");
		core::abandon_resolution_change(overlay, "review-thread:T1");
		expect(core::project_thread_resolution(true, overlay, "review-thread:T1") == core::projected_resolution{ .Resolved = true }, "a failed reopen shows the read");
	}

	void test_exact_conversation_terminals_survive_only_the_same_context()
	{
		const auto scope = core::conversation_mutation_scope{
			.Session     = core::application_session_id{ 1U },
			.Account     = core::github_account_id{ 2U },
			.PullRequest = core::pull_request_id{ 3U },
			.Head        = *core::parse_git_commit_oid("1111111111111111111111111111111111111111"),
		};
		const auto request = core::conversation_mutation_request{ core::request_id{ 1U }, scope.Session, core::request_generation{ 4U } };
		const auto change  = core::pending_conversation_mutation{ .Subject = "review-thread:T1", .Add = true };
		auto       state   = core::conversation_mutation_state{ };
		auto       overlay = core::conversation_overlay{ };
		expect(core::adopt_conversation_mutations(state, overlay, scope, false) == core::conversation_mutation_retirement::cancel, "a first context has no previous authority to drain");
		expect(core::begin_resolution_change(overlay, change.Subject, change.Add) && core::retain_conversation_mutation(state, request, change), "an admitted resolution owns its exact request");
		expect(core::adopt_conversation_mutations(state, overlay, scope, true) == core::conversation_mutation_retirement::drain && core::resolution_in_flight(overlay, change.Subject), "an accepted same-context read cannot retire an admitted request");
		for (const auto wrong : { core::conversation_mutation_request{ core::request_id{ 2U }, request.Session, request.Generation },
								  core::conversation_mutation_request{ request.Request, core::application_session_id{ 2U }, request.Generation },
								  core::conversation_mutation_request{ request.Request, request.Session, core::request_generation{ 5U } } }) {
			expect(!core::settle_conversation_mutation(state, overlay, wrong, change, true), "a different request, session or host generation cannot settle this change");
		}
		auto wrongChange = change;
		wrongChange.Add  = false;
		expect(!core::settle_conversation_mutation(state, overlay, request, wrongChange, true), "a terminal must answer the exact gesture too");
		expect(core::settle_conversation_mutation(state, overlay, request, change, true) && !core::resolution_in_flight(overlay, change.Subject), "the original successful terminal settles after the refresh");
		expect(!core::settle_conversation_mutation(state, overlay, request, change, false), "a duplicate terminal cannot undo success");

		// A replacement host may reuse Request=1. Its generation remains part
		// of ownership, and the old terminal can never answer its new gesture.
		const auto next = core::conversation_mutation_request{ request.Request, request.Session, core::request_generation{ 5U } };
		expect(core::begin_resolution_change(overlay, change.Subject, false) && core::retain_conversation_mutation(state, next, wrongChange), "the next host may admit the opposite change");
		expect(!core::settle_conversation_mutation(state, overlay, request, change, true) && core::resolution_in_flight(overlay, change.Subject), "an old host's duplicate cannot answer a reused numeric request");
		expect(core::settle_conversation_mutation(state, overlay, next, wrongChange, false) && !core::resolution_in_flight(overlay, change.Subject), "failure, refusal, cancellation and an unconfirmed outcome release ownership without retry");
		const auto otherThread = core::pending_conversation_mutation{ .Subject = "review-thread:T2", .Add = true };
		expect(core::begin_resolution_change(overlay, change.Subject, true) && core::retain_conversation_mutation(state, request, change)
				   && core::begin_resolution_change(overlay, otherThread.Subject, true) && core::retain_conversation_mutation(state, next, otherThread),
			   "overlapping old and replacement hosts can both own numeric request 1");
		expect(core::settle_conversation_mutation(state, overlay, request, change, true) && core::resolution_in_flight(overlay, otherThread.Subject), "the old answer settles only its own thread");
		expect(!core::settle_conversation_mutation(state, overlay, request, change, false) && core::settle_conversation_mutation(state, overlay, next, otherThread, true), "the replacement host keeps its own terminal despite an old duplicate");

		auto       alternatives = std::vector<core::conversation_mutation_scope>{ };
		const auto alternative  = [&](auto aChange) { auto other = scope; aChange(other); alternatives.push_back(other); };
		alternative([](auto& aScope) { aScope.Session = core::application_session_id{ 2U }; });
		alternative([](auto& aScope) { aScope.Profile = core::host_profile_id{ 2U }; });
		alternative([](auto& aScope) { aScope.ProfileRevision = core::github_profile_revision{ 2U }; });
		alternative([](auto& aScope) { aScope.Host = core::git_host_id{ 2U }; });
		alternative([](auto& aScope) { auto bytes = std::array<std::uint8_t, 32U>{}; bytes[0] = 1U; aScope.HostAuthority = core::host_authority_fingerprint{ bytes }; });
		alternative([](auto& aScope) { aScope.Account = core::github_account_id{ 3U }; });
		alternative([](auto& aScope) { aScope.Viewer = core::github_viewer_actor_id{ 2U }; });
		alternative([](auto& aScope) { aScope.IdentityGeneration = core::github_identity_generation{ 2U }; });
		alternative([](auto& aScope) { aScope.Repository = core::repository_id{ 2U }; });
		alternative([](auto& aScope) { aScope.PullRequest = core::pull_request_id{ 4U }; });
		alternative([](auto& aScope) { aScope.Number = core::pull_request_number{ 2U }; });
		alternative([](auto& aScope) { aScope.Base = aScope.Head; });
		alternative([](auto& aScope) { aScope.Head = { }; });
		alternative([](auto& aScope) { aScope.Mode = static_cast<core::github_access_mode>(1); });
		for (const auto& other : alternatives) {
			static_cast<void>(core::adopt_conversation_mutations(state, overlay, scope, false));
			expect(core::begin_resolution_change(overlay, change.Subject, true) && core::retain_conversation_mutation(state, request, change), "prepare another pending request");
			expect(core::adopt_conversation_mutations(state, overlay, other, true) == core::conversation_mutation_retirement::cancel && state.Pending.empty() && overlay.Resolutions.empty(), "every changed authority or source field retires old ownership");
			expect(!core::settle_conversation_mutation(state, overlay, request, change, true), "the retired terminal cannot affect the replacement context");
		}
		static_cast<void>(core::adopt_conversation_mutations(state, overlay, scope, false));
		expect(core::begin_resolution_change(overlay, change.Subject, true) && core::retain_conversation_mutation(state, request, change), "prepare a provider binding change");
		expect(core::adopt_conversation_mutations(state, overlay, scope, false) == core::conversation_mutation_retirement::cancel && state.Pending.empty(), "changed exact provider target/actor metadata also retires ownership");
		core::retire_conversation_mutations(state, overlay);
		expect(!state.Scope && state.Pending.empty() && overlay.Resolutions.empty(), "real retirement drops authority and overlay together");
		static_cast<void>(core::adopt_conversation_mutations(state, overlay, scope, false));
		const auto reaction = core::pending_conversation_mutation{ core::conversation_mutation_kind::reaction, "comment:C1", reaction_content::heart, true };
		expect(core::begin_reaction_change(overlay, reaction.Subject, reaction.Content, true) && core::retain_conversation_mutation(state, request, reaction), "a reaction uses the same exact request ownership");
		expect(core::adopt_conversation_mutations(state, overlay, scope, true) == core::conversation_mutation_retirement::drain, "a refresh also preserves an admitted reaction");
		expect(core::settle_conversation_mutation(state, overlay, request, reaction, true, core::reaction_summary{ .ViewerCanReact = true, .Groups = { group(reaction.Content, 1U, true) } })
				   && !core::reaction_in_flight(overlay, reaction.Subject, reaction.Content),
			   "the reaction's original answer settles it");
		expect(!core::settle_conversation_mutation(state, overlay, request, reaction, false), "a duplicate cannot abandon a confirmed reaction");
	}
}

auto main() -> int
{
	test_names_round_trip_in_github_order();
	test_a_summary_is_canonical_or_refused();
	test_a_reaction_is_refused_for_exactly_four_reasons();
	test_a_change_shows_at_once_and_settles_on_github_s_answer();
	test_a_read_that_disagrees_twice_is_believed();
	test_a_failed_change_restores_the_read();
	test_a_resolution_shows_at_once_and_reconciles();
	test_exact_conversation_terminals_survive_only_the_same_context();
	std::cout << "conversation-reactions-core-tests:pass\n";
	return EXIT_SUCCESS;
}
