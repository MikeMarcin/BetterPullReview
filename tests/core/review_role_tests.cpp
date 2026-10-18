/// \file
/// \brief [[BPR-1589]]. Proves my role and the attention list are decided from
/// accepted facts only: every role, several at once, unobserved team
/// membership, mentions as GitHub links them, and no claim of a requirement.

#include "bpr/core/review_role.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto kinds(const core::review_roles& aRoles) -> std::vector<core::review_role_kind>
	{
		auto result = std::vector<core::review_role_kind>{ };
		for (const auto& role : aRoles.Roles) {
			result.push_back(role.Kind);
		}
		return result;
	}

	void each_role_alone()
	{
		using kind      = core::review_role_kind;
		const auto none = core::decide_review_roles({ .Viewer = "mona", .Author = "octo" });
		expect(none.ViewerKnown && none.Roles.empty(), "a known viewer with nothing to do has no role");

		const auto unknown = core::decide_review_roles({ .Viewer = "", .Author = "octo", .Requests = { { .Name = "" } } });
		expect(!unknown.ViewerKnown && unknown.Roles.empty(), "an unknown viewer is told nothing, not that they have no role");

		const auto author = core::decide_review_roles({ .Viewer = "Octo", .Author = "octo" });
		expect(kinds(author) == std::vector{ kind::author }, "the author, whatever the login's case");

		const auto requested = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { { .Name = "MONA" } } });
		expect(kinds(requested) == std::vector{ kind::requested } && !requested.Roles.front().CodeOwner, "a review requested from me by name, not as a code owner");

		const auto owner = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { { .Name = "mona", .CodeOwner = true } } });
		expect(owner.Roles.front().CodeOwner, "GitHub's code-owner marking is carried as given");

		const auto team = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { { .Name = "acme/render", .Team = true, .CodeOwner = true }, { .Name = "acme/docs", .Team = true } } });
		expect(kinds(team) == std::vector{ kind::team_requested } && team.Roles.front().Teams == std::vector<std::string>{ "acme/render", "acme/docs" }
				   && team.Roles.front().Membership == core::team_membership::unknown && team.Roles.front().Reason == core::team_membership_reason::unavailable
				   && team.Roles.front().CodeOwner,
			   "teams whose membership was not asked are named, once each, with no claim either way");

		const auto reviewed = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .LatestReview = core::my_review_state::changes_requested });
		expect(kinds(reviewed) == std::vector{ kind::reviewed } && reviewed.Roles.front().Review == core::my_review_state::changes_requested, "my latest review's state");

		const auto mentioned = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Mention = core::mention_place::description });
		expect(kinds(mentioned) == std::vector{ kind::mentioned } && mentioned.Roles.front().Mention == core::mention_place::description, "a mention, and where");
	}

	void several_at_once()
	{
		using kind     = core::review_role_kind;
		const auto all = core::decide_review_roles({
			.Viewer       = "mona",
			.Author       = "mona",
			.Requests     = { { .Name = "acme/render", .Team = true }, { .Name = "mona" } },
			.LatestReview = core::my_review_state::commented,
			.Mention      = core::mention_place::comment,
		});
		expect(kinds(all) == std::vector{ kind::author, kind::requested, kind::team_requested, kind::reviewed, kind::mentioned }, "every role, strongest first: author, direct request, team request, reviewed, mentioned");

		const auto rerequested = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { { .Name = "mona" } }, .LatestReview = core::my_review_state::approved });
		expect(kinds(rerequested) == std::vector{ kind::requested, kind::reviewed }, "a re-requested reviewer is asked again and has reviewed");

		// [[BPR-1589]] v002. GitHub's answer decides: my team is my request;
		// a team I am not in is no role; a team GitHub would not tell about is
		// named, with the reason, only when no team of mine was requested.
		using member    = core::team_membership;
		using reason    = core::team_membership_reason;
		const auto team = [](const char* aName, const member aMembership, const reason aReason = reason::unavailable) {
			return core::review_role_request{ .Name = aName, .Team = true, .Membership = aMembership, .Reason = aReason };
		};
		const auto observed = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { team("acme/render", member::not_member), team("acme/docs", member::member) } });
		expect(kinds(observed) == std::vector{ kind::team_requested } && observed.Roles.front().Teams == std::vector<std::string>{ "acme/docs" } && observed.Roles.front().Membership == member::member,
			   "a team GitHub says I am in is my request; the one I am not in is not mentioned");
		const auto outside = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { team("acme/render", member::not_member) } });
		expect(outside.Roles.empty(), "a team GitHub says I am not in is no role");
		const auto scoped = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { team("acme/render", member::unknown, reason::needs_read_org), team("acme/docs", member::not_member) } });
		expect(kinds(scoped) == std::vector{ kind::team_requested } && scoped.Roles.front().Membership == member::unknown && scoped.Roles.front().Reason == reason::needs_read_org
				   && scoped.Roles.front().Teams == std::vector<std::string>{ "acme/render" },
			   "a team GitHub would not tell about is named with the reason");
		const auto both = core::decide_review_roles({ .Viewer = "mona", .Author = "octo", .Requests = { team("acme/render", member::unknown, reason::not_visible), team("acme/docs", member::member) } });
		expect(both.Roles.size() == 1U && both.Roles.front().Membership == member::member && both.Roles.front().Teams == std::vector<std::string>{ "acme/docs" },
			   "my team leads; an unknown one is not added beside it");
	}

	void mentions()
	{
		expect(core::mentions_login("Thanks @mona!", "mona"), "a mention followed by punctuation");
		expect(core::mentions_login("@Mona can you look", "mona"), "a mention at the start, in another case");
		expect(core::mentions_login("cc:\n(@mona)", "mona"), "a mention on a later line, in parentheses");
		expect(!core::mentions_login("mail mona@example.com", "mona"), "an address is not a mention");
		expect(!core::mentions_login("@monalisa", "mona"), "a longer login is someone else");
		expect(!core::mentions_login("@mona-bot", "mona"), "a hyphenated login is someone else");
		expect(!core::mentions_login("@mona/render", "mona"), "a team in an organization named like me is not me");
		expect(!core::mentions_login("run `@mona` here", "mona"), "inline code is not searched");
		expect(core::mentions_login("run `x` for @mona", "mona"), "text after a closed code span is searched");
		expect(!core::mentions_login("```\n@mona\n```", "mona"), "a fenced block is not searched");
		expect(core::mentions_login("~~~\ncode\n~~~\n@mona", "mona"), "text after a fence closes is searched");
		expect(!core::mentions_login("@mona", ""), "no login, no mention");
		expect(!core::mentions_login("", "mona"), "empty text");
	}

	void attention()
	{
		using enum core::status_attention_kind;
		const auto quiet = core::decide_status_attention({ .Viewer = "mona" });
		expect(quiet.empty(), "nothing applies, nothing is listed");

		const auto all = core::decide_status_attention({
			.Viewer       = "mona",
			.Conflicts    = true,
			.FailedChecks = { "build-linux", "lint" },
			.WaitingOn    = { "acme/render", "MONA", "octo" },
			.RepliesToMe  = 2U,
		});
		expect(all.size() == 4U && all[0].Kind == conflicts && all[1].Kind == failed_checks && all[1].Count == 2U
				   && all[1].Names == std::vector<std::string>{ "build-linux", "lint" } && all[2].Kind == waiting_on
				   && all[2].Names == std::vector<std::string>{ "acme/render", "octo" } && all[3].Kind == replies_to_me && all[3].Count == 2U,
			   "conflicts, failed checks named once, who is waited on without me, replies to me");

		const auto onlyMe = core::decide_status_attention({ .Viewer = "mona", .WaitingOn = { "mona" } });
		expect(onlyMe.empty(), "a request of mine is my role, not something I wait on");
	}
} // namespace

auto main() -> int
{
	each_role_alone();
	several_at_once();
	mentions();
	attention();
	return EXIT_SUCCESS;
}
