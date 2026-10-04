#include "bpr/core/review_cycle_association.hpp"
#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1177]]. A cycle's association with its pull request: the
/// address a reviewer names, admission of an externally published pull
/// request only from a read of exactly it, the association a linked native
/// publication carries, the exact encoding, and the description in the form
/// a Review Tour maps.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle association failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto commit(const std::uint8_t aSeed) -> git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[19]  = 0xC1;
		return git_commit_oid{ git_hash_algorithm::sha1, bytes };
	}

	auto asked() -> cycle_pull_address
	{
		return { .Host = "github.com", .Owner = "octo-org", .Repository = "review-repository", .Number = 17 };
	}

	auto observed() -> cycle_pull_observation
	{
		return {
			.Address  = asked(),
			.NodeId   = "PR_kwDOexternal",
			.Url      = "https://github.com/octo-org/review-repository/pull/17",
			.State    = "open",
			.HeadRef  = "harden-parser",
			.Head     = commit(1),
			.BaseRef  = "main",
			.BaseHead = commit(2),
			.Account  = github_account_id{ 1001 },
			.Login    = "octo-reviewer",
		};
	}

	void addresses()
	{
		const auto parsed = parse_pull_address("  https://GitHub.com/octo-org/review-repository/pull/17/files?w=1#diff  ");
		expect(parsed && parsed->Host == "github.com" && parsed->Owner == "octo-org" && parsed->Repository == "review-repository" && parsed->Number == 17U,
			   "an address with a trailing path, query and fragment, and a host in capitals");
		expect(pull_address_url(*parsed) == "https://github.com/octo-org/review-repository/pull/17", "the canonical web form");
		expect(parse_pull_address("https://ghe.example.com/team/tool/pull/3").has_value(), "an Enterprise host");
		for (const auto* const text : { "http://github.com/o/r/pull/1", "github.com/o/r/pull/1", "https://github.com/o/r/issues/1", "https://github.com/o/r/pull/0",
										"https://github.com/o/r/pull/01", "https://github.com/o/r/pull/x", "https://github.com/o/r/pull/", "https://github.com/o/r", "https://bad_host/o/r/pull/1",
										"https://github.com/-bad/r/pull/1", "https://github.com/o/../pull/1", "" }) {
			expect(!parse_pull_address(text).has_value(), text);
		}
		expect(same_pull_request(asked(), { .Host = "GITHUB.COM", .Owner = "Octo-Org", .Repository = "Review-Repository", .Number = 17 }), "owner and repository compare without case");
		expect(!same_pull_request(asked(), { .Host = "github.com", .Owner = "octo-org", .Repository = "review-repository", .Number = 18 }), "another number is another pull request");
	}

	void external_link()
	{
		const auto linked = decide_external_link(asked(), observed(), { }, cycle_source_revision_id{ 3 });
		expect(linked.has_value(), "the read of exactly that pull request is admitted");
		expect(linked->Origin == cycle_pull_origin::external && !linked->Publication, "it is externally published, never a native publication");
		expect(linked->NodeId == "PR_kwDOexternal" && linked->Head == commit(1) && linked->BaseRef == "main" && linked->HeadRef == "harden-parser" && linked->Login == "octo-reviewer",
			   "every field comes from the observation");
		expect(linked->Revision == cycle_source_revision_id{ 3 }, "the cycle's latest capture when it was linked");

		auto other    = observed();
		other.Address = { .Host = "github.com", .Owner = "octo-org", .Repository = "review-repository", .Number = 18 };
		expect(decide_external_link(asked(), other, { }, std::nullopt).error() == cycle_link_refusal::mismatch, "a read that returned another pull request");
		auto url = observed();
		url.Url  = "https://github.com/octo-org/elsewhere/pull/17";
		expect(decide_external_link(asked(), url, { }, std::nullopt).error() == cycle_link_refusal::mismatch, "an observation whose own address names another repository");
		auto node   = observed();
		node.NodeId = "";
		expect(decide_external_link(asked(), node, { }, std::nullopt).error() == cycle_link_refusal::mismatch, "no node identity is filled in");
		auto head = observed();
		head.Head = { };
		expect(decide_external_link(asked(), head, { }, std::nullopt).error() == cycle_link_refusal::mismatch, "no head commit is filled in");
		auto account    = observed();
		account.Account = github_account_id{ 0 };
		expect(decide_external_link(asked(), account, { }, std::nullopt).error() == cycle_link_refusal::mismatch, "a read under no account");
		expect(decide_external_link({ }, observed(), { }, std::nullopt).error() == cycle_link_refusal::address, "an empty address");

		const auto existing = std::vector{ *linked };
		expect(decide_external_link(asked(), observed(), existing, std::nullopt).error() == cycle_link_refusal::already_linked, "the same pull request is not linked twice");
		auto many = std::vector<cycle_pull_association>(maximum_cycle_associations, *linked);
		for (std::size_t index = 0; index < many.size(); ++index) {
			many[index].Address.Number = 1000U + index;
		}
		expect(decide_external_link(asked(), observed(), many, std::nullopt).error() == cycle_link_refusal::bounds, "bounded");
	}

	auto plan() -> publication_plan
	{
		auto value        = publication_plan{ };
		value.Start       = cycle_source_revision_id{ 1 };
		value.Final       = cycle_source_revision_id{ 3 };
		value.Format      = git_hash_algorithm::sha1;
		value.Parent      = commit(2);
		value.Commit      = commit(5);
		value.TargetHead  = commit(2);
		value.Destination = { .Host = "github.com", .Account = github_account_id{ 1001 }, .Login = "octo-reviewer", .Owner = "octo-org", .Repository = "review-repository", .BaseBranch = "main", .NewBranch = "bpr/cycle-1-r3" };
		value.PullTitle   = "Harden the parser";
		value.Draft       = true;
		return value;
	}

	void native_link()
	{
		auto journal = publication_journal{ .Phase = publication_phase::pr_creation_pending };
		expect(!published_association(cycle_publication_id{ 4 }, plan(), journal), "nothing before the pull request is linked");
		journal.Phase          = publication_phase::linked;
		journal.Link           = publication_link{ .Number = 17, .NodeId = "PR_kwDOnative", .Url = "https://github.com/octo-org/review-repository/pull/17", .Head = commit(5), .BaseBranch = "main" };
		const auto association = published_association(cycle_publication_id{ 4 }, plan(), journal);
		expect(association && association->Origin == cycle_pull_origin::published && association->Publication == cycle_publication_id{ 4 }, "a linked publication is a published association");
		expect(association->Address == asked() && association->Head == commit(5) && association->HeadRef == "bpr/cycle-1-r3" && association->BaseRef == "main" && association->Draft
				   && association->Revision == cycle_source_revision_id{ 3 } && !association->Drifted,
			   "it carries the journal's link and the plan's destination");
		journal.Link->Drifted = true;
		expect(published_association(cycle_publication_id{ 4 }, plan(), journal)->Drifted, "drift recorded on the link stays visible");
	}

	void encoding()
	{
		const auto linked  = *decide_external_link(asked(), observed(), { }, cycle_source_revision_id{ 3 });
		const auto bytes   = encode_association(linked);
		const auto decoded = decode_association(bytes);
		expect(decoded && *decoded == linked, "an external association round-trips exactly");
		auto journal      = publication_journal{ .Phase = publication_phase::linked };
		journal.Link      = publication_link{ .Number = 17, .NodeId = "PR_kwDOnative", .Url = "https://github.com/octo-org/review-repository/pull/17", .Head = commit(5), .BaseBranch = "main" };
		const auto native = *published_association(cycle_publication_id{ 4 }, plan(), journal);
		expect(decode_association(encode_association(native)) == native, "a published association round-trips exactly");
		auto truncated = bytes;
		truncated.pop_back();
		expect(!decode_association(truncated), "truncated bytes are refused");
		auto extra = bytes;
		extra.push_back(0);
		expect(!decode_association(extra), "trailing bytes are refused");
		auto forged        = linked;
		forged.Publication = cycle_publication_id{ 9 };
		expect(!decode_association(encode_association(forged)), "an external association naming a publication is refused");
	}

	void description()
	{
		const auto body = publication_body("Parsing accepted a truncated header.", "Validate the length before reading.");
		expect(body && *body == "## Problem\nParsing accepted a truncated header.\n\n## Solution\nValidate the length before reading.\n\n", "the Problem and Solution brief");
		const auto mapped = map_tour_markdown(*body);
		expect(std::ranges::count_if(mapped.Sections, [](const tour_markdown_section& aSection) { return aSection.Name == "Problem" || aSection.Name == "Solution"; }) == 2,
			   "a Review Tour maps both fields from the description");
		expect(!publication_body(std::string(maximum_publication_body_bytes, 'x'), "").has_value(), "a description over its bound is refused, never cut");
		expect(suggested_publication_branch(review_cycle_id{ 2 }, cycle_source_revision_id{ 5 }) == "bpr/cycle-2-r5", "the suggested branch names the cycle and revision");
		expect(valid_publication_branch(suggested_publication_branch(review_cycle_id{ 2 }, cycle_source_revision_id{ 5 })), "and is a valid new branch");
	}
}

auto main() -> int
{
	addresses();
	external_link();
	native_link();
	encoding();
	description();
	std::cout << "review cycle association: pass\n";
	return EXIT_SUCCESS;
}
