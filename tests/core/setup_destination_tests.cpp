#include "bpr/core/setup_destination.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
int main()
{
	using namespace bpr::core;
	const auto destination = std::optional{ setup_destination{ .Hostname = "ghe.example", .Repository = { "owner", "repo" }, .Number = pull_request_number{ 42U }, .Location = "/files#diff-deadbeef" } };

	const auto owner    = setup_owner_id{ 7U };
	const auto request  = setup_request_id{ 11U };
	auto       pending  = pending_setup_destination{ .Request = request, .Owner = owner, .Destination = *destination, .Profile = std::nullopt };
	const auto profiles = std::array{ setup_profile_observation{ host_profile_id{ 2U }, "github.com", true }, setup_profile_observation{ host_profile_id{ 3U }, "ghe.example", false } };
	auto       resolved = resolve_setup_destination(pending, owner, profiles);
	expect(resolved && resolved->Profile == host_profile_id{ 3U } && resolved->Stage == setup_destination_stage::sign_in, "never route to active other host");
	expect(!accept_setup_destination(resolved, request, owner, host_profile_id{ 3U }, "ghe.example"), "sign in requires explicit continuation");
	resolved->Stage = setup_destination_stage::authenticating;
	expect(accept_setup_destination(resolved, request, owner, host_profile_id{ 3U }, "ghe.example") == destination, "exact accepted destination");
	expect(!accept_setup_destination(resolved, setup_request_id{ 12U }, owner, host_profile_id{ 3U }, "ghe.example"), "stale request refused");
	expect(!accept_setup_destination(resolved, request, setup_owner_id{ 8U }, host_profile_id{ 3U }, "ghe.example"), "replaced owner refused");
	expect(!accept_setup_destination(resolved, request, owner, host_profile_id{ 2U }, "ghe.example"), "wrong profile refused");
	expect(!accept_setup_destination(resolved, request, owner, host_profile_id{ 3U }, "github.com"), "wrong hostname refused");
	expect(!accept_setup_destination(std::nullopt, request, owner, host_profile_id{ 3U }, "ghe.example"), "cancelled request refused");
	pending.Destination.Hostname = "unknown.example";
	resolved                     = resolve_setup_destination(pending, owner, profiles);
	expect(resolved && !resolved->Profile && resolved->Stage == setup_destination_stage::add_server, "unknown authority requires explicit configuration");
	// [[BPR-0732]]. A known public host with no account opens signed out in
	// one gesture; a host that refuses preview keeps the sign-in stop; an
	// account still wins over preview.
	const auto anonymous         = std::array{ setup_profile_observation{ host_profile_id{ 2U }, "github.com", false, true }, setup_profile_observation{ host_profile_id{ 3U }, "ghe.example", false, false } };
	pending.Destination.Hostname = "github.com";
	resolved                     = resolve_setup_destination(pending, owner, anonymous);
	expect(resolved && resolved->Profile == host_profile_id{ 2U } && resolved->Stage == setup_destination_stage::public_preview, "public host with no account opens signed out");
	expect(accept_setup_destination(resolved, request, owner, host_profile_id{ 2U }, "github.com") == pending.Destination, "the signed-out stage is accepted without a continuation press");
	pending.Destination.Hostname = "ghe.example";
	resolved                     = resolve_setup_destination(pending, owner, anonymous);
	expect(resolved && resolved->Profile == host_profile_id{ 3U } && resolved->Stage == setup_destination_stage::sign_in, "a host that refuses preview keeps sign in");
	const auto both              = std::array{ setup_profile_observation{ host_profile_id{ 2U }, "github.com", true, true } };
	pending.Destination.Hostname = "github.com";
	resolved                     = resolve_setup_destination(pending, owner, both);
	expect(resolved && resolved->Stage == setup_destination_stage::ready, "an account wins over preview");
	const auto nativeAndCli = std::array{
		setup_profile_observation{ host_profile_id{ 3U }, "ghe.example", false, false },
		setup_profile_observation{ host_profile_id{ 4U }, "ghe.example", true, false },
		setup_profile_observation{ host_profile_id{ 5U }, "other.example", false, false },
	};
	pending.Destination.Hostname = "ghe.example";
	resolved                     = resolve_setup_destination(pending, owner, nativeAndCli);
	expect(resolved && resolved->Profile == host_profile_id{ 4U } && resolved->Stage == setup_destination_stage::ready, "accepted CLI profile wins over earlier inactive native profile");
	expect(accept_setup_destination(resolved, request, owner, host_profile_id{ 4U }, "ghe.example") == pending.Destination, "CLI destination is accepted without native enrollment");
	pending.Destination.Hostname = "other.example";
	resolved                     = resolve_setup_destination(pending, owner, nativeAndCli);
	expect(resolved && resolved->Profile == host_profile_id{ 5U } && resolved->Stage == setup_destination_stage::sign_in, "another host cannot borrow CLI authentication");
	pending.Destination.Hostname  = "ghe.example";
	auto disconnected             = nativeAndCli;
	disconnected[1].Authenticated = false;
	resolved                      = resolve_setup_destination(pending, owner, disconnected);
	expect(resolved && resolved->Profile == host_profile_id{ 3U } && resolved->Stage == setup_destination_stage::sign_in, "disconnected CLI profile does not grant authentication");
	disconnected[1].RevalidationAllowed = true;
	resolved                            = resolve_setup_destination(pending, owner, disconnected);
	expect(resolved && resolved->Profile == host_profile_id{ 4U } && resolved->Stage == setup_destination_stage::authenticating, "remembered choice validates exact account ahead of inactive native profile");
	pending.Destination.Hostname = "other.example";
	resolved                     = resolve_setup_destination(pending, owner, disconnected);
	expect(resolved && resolved->Profile == host_profile_id{ 5U } && resolved->Stage == setup_destination_stage::sign_in, "remembered consent cannot validate another server");

	pending.Destination.Hostname = "github.com";
	pending.Destination.Kind     = setup_destination_kind::repository;
	resolved                     = resolve_setup_destination(pending, owner, anonymous);
	expect(resolved && resolved->Stage == setup_destination_stage::sign_in,
		   "repository authority never uses anonymous public preview");
	pending.Destination.Kind = setup_destination_kind::pull_request;

	// [[BPR-0987]]. The core decides what a link's location names in the
	// receiving window, from that window's accepted facts alone.
	expect(pull_request_file_digest("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "the file digest is SHA-256 in lowercase hexadecimal");
	const auto paths          = std::array{ std::string{ "src/review.cpp" }, std::string{ "docs/readme.md" } };
	const auto reviews        = std::array{ pull_request_location_conversation{ .Identity = 4001U, .Conversation = "review:PRR_1" } };
	const auto reviewComments = std::array{ pull_request_location_conversation{ .Identity = 71001U, .Conversation = "public-preview-comment:71001" } };
	const auto facts          = [&](const bool aRightIsHead) {
		return pull_request_location_facts{ .ChangedPaths = paths, .Reviews = reviews, .ReviewComments = reviewComments, .RightIsHead = aRightIsHead };
	};
	const auto file = [](const std::string_view aPath, std::optional<diff_side> aSide = std::nullopt, const std::uint32_t aLine = 0U) {
		return pull_request_location{ .Kind = pull_request_location_kind::file, .FileDigest = pull_request_file_digest(aPath), .Side = aSide, .Line = aLine };
	};
	expect(resolve_pull_request_location({ }, facts(true)).Action == pull_request_location_action::none, "the pull request itself names no location");
	auto target = resolve_pull_request_location(file("src/review.cpp"), facts(false));
	expect(target.Action == pull_request_location_action::select_file && target.Path == "src/review.cpp" && !target.RightLine && target.Refusal == pull_request_location_refusal::none,
		   "a file digest names exactly its changed path");
	target = resolve_pull_request_location(file("src/other.cpp"), facts(true));
	expect(target.Action == pull_request_location_action::unavailable && target.Refusal == pull_request_location_refusal::file_not_in_pull_request && target.Path.empty(),
		   "a digest naming no changed file substitutes nothing");
	target = resolve_pull_request_location(file("src/review.cpp", diff_side::right, 42U), facts(true));
	expect(target.Action == pull_request_location_action::select_file && target.RightLine == std::optional<std::uint32_t>{ 41U }, "a right-side line maps zero-based under a comparison ending at the head");
	target = resolve_pull_request_location(file("src/review.cpp", diff_side::right, 42U), facts(false));
	expect(target.Action == pull_request_location_action::select_file && !target.RightLine && target.Refusal == pull_request_location_refusal::line_needs_head,
		   "a right-side line is refused under any other comparison, the file is still named");
	target = resolve_pull_request_location(file("src/review.cpp", diff_side::left, 3U), facts(true));
	expect(target.Action == pull_request_location_action::select_file && !target.RightLine && target.Refusal == pull_request_location_refusal::base_side_line,
		   "a base-side line is never mapped against another base");
	const auto duplicated = std::array{ std::string{ "a.cpp" }, std::string{ "a.cpp" } };
	target                = resolve_pull_request_location(file("a.cpp"), pull_request_location_facts{ .ChangedPaths = duplicated });
	expect(target.Action == pull_request_location_action::unavailable, "a digest two paths share is not exact");
	target = resolve_pull_request_location({ .Kind = pull_request_location_kind::review, .Identity = 4001U }, facts(true));
	expect(target.Action == pull_request_location_action::reveal_conversation && target.Conversation == "review:PRR_1", "a review the read identified is revealed");
	target = resolve_pull_request_location({ .Kind = pull_request_location_kind::review, .Identity = 4002U }, facts(true));
	expect(target.Action == pull_request_location_action::unavailable && target.Refusal == pull_request_location_refusal::review_not_read, "an unread review is explained");
	target = resolve_pull_request_location({ .Kind = pull_request_location_kind::review_comment, .Identity = 71001U }, facts(true));
	expect(target.Action == pull_request_location_action::reveal_conversation && target.Conversation == "public-preview-comment:71001", "a comment the read identified is revealed");
	target = resolve_pull_request_location({ .Kind = pull_request_location_kind::review_comment, .Identity = 71002U }, facts(true));
	expect(target.Refusal == pull_request_location_refusal::comment_not_identified, "a comment the read did not identify is explained");
	expect(resolve_pull_request_location({ .Kind = pull_request_location_kind::issue_comment, .Identity = 9U }, facts(true)).Refusal == pull_request_location_refusal::issue_comment,
		   "a discussion comment link is explained");
	expect(resolve_pull_request_location({ .Kind = pull_request_location_kind::commit, .Commit = "abcdef0" }, facts(true)).Refusal == pull_request_location_refusal::commit_page,
		   "a commit page is explained, never mapped to another revision");
	expect(resolve_pull_request_location({ .Kind = pull_request_location_kind::unrecognized }, facts(true)).Refusal == pull_request_location_refusal::unrecognized,
		   "an unread fragment is explained");
	expect(resolve_pull_request_location({ .Kind = pull_request_location_kind::file, .FileDigest = "ABC" }, facts(true)).Action == pull_request_location_action::unavailable
			   && !valid(pull_request_location{ .Kind = pull_request_location_kind::review }) && !valid(pull_request_location{ .Commit = "abc" }),
		   "an invalid typed location decides nothing");
	for (const auto refusal : { pull_request_location_refusal::commit_page, pull_request_location_refusal::file_not_in_pull_request, pull_request_location_refusal::base_side_line,
								pull_request_location_refusal::line_needs_head, pull_request_location_refusal::line_not_in_file, pull_request_location_refusal::review_not_read,
								pull_request_location_refusal::comment_not_identified, pull_request_location_refusal::issue_comment, pull_request_location_refusal::unrecognized }) {
		expect(!explanation(refusal).empty(), "every refusal is explained");
	}
	return EXIT_SUCCESS;
}
