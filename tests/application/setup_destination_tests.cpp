#include "bpr/application/manual_executor.hpp"
#include "bpr/application/setup_destination.hpp"

#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	struct recording_port final : application::setup_destination_port
	{
		std::function<void()>                               OnPresent;
		core::setup_owner_id                                Owner{ 1U };
		std::vector<application::setup_profile_observation> Profiles{ { core::host_profile_id{ 1U }, "github.com", true } };
		std::vector<core::pending_setup_destination>        Configurations;
		std::vector<core::pending_setup_destination>        Authentications;
		std::vector<core::pending_setup_destination>        Previews;
		std::vector<core::setup_destination>                Opened;
		application::setup_destination_projection           Projection;
		bool                                                Navigate = false;
		auto                                                profiles() const -> std::vector<application::setup_profile_observation> override { return Profiles; }
		auto                                                owner() const -> core::setup_owner_id override { return Owner; }
		auto                                                shorthand_hostname() const -> std::string override { return "github.com"; }
		void                                                configure(const core::pending_setup_destination& aPending) override { Configurations.push_back(aPending); }
		void                                                authenticate(const core::pending_setup_destination& aPending) override { Authentications.push_back(aPending); }
		void                                                preview(const core::pending_setup_destination& aPending) override { Previews.push_back(aPending); }
		auto                                                open(core::host_profile_id, const core::setup_destination& aDestination) -> application::setup_destination_opening override
		{
			Opened.push_back(aDestination);
			return Navigate ? application::setup_destination_opening::navigated : application::setup_destination_opening::reading;
		}
		void present(const application::setup_destination_projection& aProjection) override
		{
			Projection = aProjection;
			if (OnPresent) {
				OnPresent();
			}
		}
	};
}
int main()
{
	using namespace bpr;
	for (const auto input : { "owner/repo#42", "owner/repo/pull/42", "https://GHE.example/owner/repo/pull/42", "https://ghe.example/owner/repo/pull/42/files?ignored=1#diff-deadbeef", "https://ghe.example/owner/repo/pull/42/commits/abcdef" }) {
		const auto parsed = application::parse_setup_destination(input);
		expect(parsed && parsed->Repository == core::repository_locator{ "owner", "repo" } && parsed->Number.value() == 42U, "supported exact locator");
	}
	const auto destination = application::parse_setup_destination("https://GHE.example/owner/repo/pull/42/files?redirect=elsewhere#diff-deadbeef");
	expect(destination && destination->Hostname == "ghe.example" && destination->Location == "/files#diff-deadbeef", "retain host/location and ignore query");
	for (const auto input : { "http://ghe.example/o/r/pull/1", "https://user@ghe.example/o/r/pull/1", "https://ghe.example:443/o/r/pull/1", "https://ghe.example/o/r/commit/abc", "https://ghe.example/o/r/pull/0", "https://ghe.example/o/r/pull/2147483648", "https://ghe.example/o/r/pull/1/nope", "https://ghe.example/o/r/pull/1/commits/xyz", "o/r#1#other", "https://-bad.example/o/r/pull/1", "https://ghe.example/o/r/pull/1\\evil" }) {
		expect(!application::parse_setup_destination(input), "refuse ambiguous or unsupported locator");
	}
	expect(!application::parse_setup_destination(std::string(4097U, 'x')), "bounded input");

	// [[BPR-0987]]. Each supported link shape reads into its exact typed
	// location; an unread fragment still names the pull request.
	{
		using kind          = core::pull_request_location_kind;
		const auto digest   = std::string(64U, 'a');
		const auto upper    = std::string(64U, 'A');
		const auto location = [](const std::string& aText) {
			const auto parsed = application::parse_setup_destination(aText);
			expect(parsed.has_value(), "a supported pull request link parses");
			return parsed->Target;
		};
		const auto base = std::string{ "https://github.com/o/r/pull/7" };
		expect(location(base).Kind == kind::pull_request && location(base + "/files").Kind == kind::pull_request
				   && location(base + "/commits").Kind == kind::pull_request && location(base + "/").Kind == kind::pull_request
				   && location("o/r#7").Kind == kind::pull_request,
			   "the canonical page, files and commits name no location");
		expect(location(base + "/commits/ABCDEF0123").Kind == kind::commit && location(base + "/commits/ABCDEF0123").Commit == "abcdef0123"
				   && location(base + "/commits/abcdef0123#diff-" + digest).Kind == kind::commit,
			   "a commit page names its commit whatever its fragment says");
		const auto file = location(base + "/files#diff-" + digest);
		expect(file.Kind == kind::file && file.FileDigest == digest && !file.Side && file.Line == 0U, "a file fragment names the file");
		const auto right = location(base + "/files#diff-" + upper + "R42");
		expect(right.Kind == kind::file && right.FileDigest == digest && right.Side == core::diff_side::right && right.Line == 42U, "a right-side line fragment");
		const auto left = location(base + "#diff-" + digest + "L3");
		expect(left.Kind == kind::file && left.Side == core::diff_side::left && left.Line == 3U, "a left-side line fragment on the canonical page");
		expect(location(base + "#discussion_r123456").Kind == kind::review_comment && location(base + "#discussion_r123456").Identity == 123456U,
			   "a review comment fragment");
		expect(location(base + "#pullrequestreview-4001").Kind == kind::review && location(base + "#pullrequestreview-4001").Identity == 4001U,
			   "a review fragment");
		expect(location(base + "#issuecomment-99").Kind == kind::issue_comment && location(base + "#issuecomment-99").Identity == 99U,
			   "a discussion comment fragment");
		for (const auto* fragment : { "#diff-abcd", "#diff-", "#discussion_r", "#discussion_r0", "#discussion_r12x", "#pullrequestreview-", "#issuecomment-018", "#top", "#diff-deadbeef" }) {
			expect(location(base + fragment).Kind == kind::unrecognized, "an unread fragment is unrecognized, not a refusal");
		}
		for (const auto* suffix : { "R0", "R", "X4", "R10000001", "R4x" }) {
			auto link = base;
			link += "#diff-";
			link += digest;
			link += suffix;
			expect(location(link).Kind == kind::unrecognized, "a malformed line keeps no file guess");
		}
		expect(location(base + "?utm_source=chat&token=x#diff-" + digest).Kind == kind::file, "query parameters are dropped, never routed");
		for (const auto* input : { "https://github.com/o/r/commit/abcdef0", "https://github.com/o/r/pull/7/checks", "https://github.com/o/r/pull/7/commits/xyz", "https://github.com/o/r/issues/7", "https://github.com/o/r" }) {
			expect(!application::parse_setup_destination(input), "a non-PR or unsupported page never guesses a pull request");
		}
		for (const auto& target : { location(base), location(base + "/commits/abcdef0123"), file, right, left, location(base + "#discussion_r1"), location(base + "#pullrequestreview-1"), location(base + "#issuecomment-1"), location(base + "#top") }) {
			expect(core::valid(target), "every parsed location is a valid typed location");
		}
	}
	auto port = recording_port{ };
	auto host = application::setup_destination_host{ port };
	auto lane = application::manual_executor{ };
	host.submit("https://enterprise.example/o/r/pull/42/files#diff-abcd");
	expect(host.pending() && port.Configurations.empty() && port.Authentications.empty() && port.Opened.empty(), "parsing unknown host has no effect");
	const auto first = host.pending()->Request;
	host.continue_setup(first);
	expect(port.Configurations.size() == 1U && port.Authentications.empty(), "explicit configuration only");
	port.Profiles.push_back({ core::host_profile_id{ 2U }, "enterprise.example", false });
	host.configured(first);
	expect(host.pending()->Stage == core::setup_destination_stage::sign_in, "configured server offers sign in");
	host.continue_setup(first);
	expect(port.Authentications.size() == 1U && port.Authentications.back().Profile == core::host_profile_id{ 2U }, "authenticate exact configured server");
	lane.post([&] { host.authenticated(first, core::host_profile_id{ 2U }, "enterprise.example"); });
	host.cancel();
	lane.drain();
	expect(port.Opened.empty(), "cancel ignores queued authentication completion");
	host.submit("https://enterprise.example/o/r/pull/43");
	const auto second = host.pending()->Request;
	host.continue_setup(second);
	lane.post([&] { host.authenticated(second, core::host_profile_id{ 2U }, "enterprise.example"); });
	host.submit("https://enterprise.example/o/r/pull/44");
	const auto third = host.pending()->Request;
	host.continue_setup(third);
	lane.drain();
	expect(port.Opened.empty(), "new target ignores older completion");
	host.authenticated(third, core::host_profile_id{ 1U }, "github.com");
	expect(port.Opened.empty(), "other authority never completes target");
	port.Owner = core::setup_owner_id{ 2U };
	host.authenticated(third, core::host_profile_id{ 2U }, "enterprise.example");
	expect(port.Opened.empty(), "profile/account replacement invalidates pending target");
	host.submit("https://enterprise.example/o/r/pull/45/files#diff-abcd");
	const auto fourth = host.pending()->Request;
	host.continue_setup(fourth);
	host.authenticated(fourth, core::host_profile_id{ 2U }, "enterprise.example");
	host.authenticated(fourth, core::host_profile_id{ 2U }, "enterprise.example");
	expect(port.Opened.size() == 1U && port.Opened.back().Number.value() == 45U && port.Opened.back().Hostname == "enterprise.example" && port.Opened.back().Location == "/files#diff-abcd", "exact success once");
	// [[BPR-0987]]. The host no longer judges the location: the receiving
	// window decides once it has accepted the pull request. The typed target
	// travels with the destination.
	expect(port.Opened.back().Target.Kind == core::pull_request_location_kind::unrecognized && port.Projection.Opened
			   && port.Projection.Notice.empty() && !port.Projection.Navigated,
		   "an unread fragment opens the pull request and leaves the explanation to the window");
	host.submit("o/r#46");
	expect(port.Opened.size() == 2U && port.Opened.back().Hostname == "github.com", "shorthand binds host-owned current server");
	host.submit("https://enterprise.example/o/r/pull/47");
	const auto fifth               = host.pending()->Request;
	const auto authenticationCount = port.Authentications.size();
	port.OnPresent                 = [&] {
		if (port.Projection.Pending && port.Projection.Pending->Stage == core::setup_destination_stage::authenticating) {
			host.cancel();
		}
	};
	host.continue_setup(fifth);
	expect(port.Authentications.size() == authenticationCount && !host.pending(), "reentrant cancellation prevents setup effect");

	// [[BPR-0732]]. A public host with no account: the host is asked to
	// preview on submit, with no continuation press, and the destination
	// opens once the signed-out session is reported for that exact profile.
	port.OnPresent = { };
	port.Profiles  = { { core::host_profile_id{ 1U }, "github.com", false, true }, { core::host_profile_id{ 2U }, "enterprise.example", false, false } };
	host.submit("https://github.com/o/r/pull/48");
	const auto sixth = host.pending()->Request;
	expect(host.pending()->Stage == core::setup_destination_stage::public_preview && port.Previews.size() == 1U && port.Previews.back().Request == sixth && port.Authentications.size() == authenticationCount, "a public host with no account is previewed on submit, not authenticated");
	expect(port.Projection.Notice.find("without signing in") != std::string::npos && port.Projection.Pending && port.Projection.Pending->Stage == core::setup_destination_stage::public_preview, "the notice says the open is signed out");
	host.previewed(sixth, core::host_profile_id{ 2U }, "enterprise.example");
	expect(port.Opened.size() == 2U, "another profile's session never opens the target");
	host.previewed(sixth, core::host_profile_id{ 1U }, "github.com");
	expect(port.Opened.size() == 3U && port.Opened.back().Number.value() == 48U && port.Opened.back().Hostname == "github.com" && !host.pending(), "the signed-out session opens the exact target once");
	host.submit("https://enterprise.example/o/r/pull/49");
	expect(host.pending()->Stage == core::setup_destination_stage::sign_in && port.Previews.size() == 1U, "a host that refuses preview keeps the sign-in stop");
	const auto seventh = host.pending()->Request;
	host.previewed(seventh, core::host_profile_id{ 2U }, "enterprise.example");
	expect(port.Opened.size() == 3U && host.pending(), "a preview report never completes a sign-in stage");

	// An explicit account choice binds the destination to that exact transport.
	port.Profiles.push_back({ core::host_profile_id{ 3U }, "enterprise.example", false, false });
	expect(!host.begin_authentication(sixth, core::host_profile_id{ 3U }), "stale choice cannot authenticate a replacement");
	expect(!host.begin_authentication(seventh, core::host_profile_id{ 1U }), "another host cannot consume the pending destination");
	expect(host.begin_authentication(seventh, core::host_profile_id{ 3U }), "same-host CLI choice replaces the native setup profile");
	host.authenticated(seventh, core::host_profile_id{ 2U }, "enterprise.example");
	expect(port.Opened.size() == 3U, "former native profile cannot complete chosen CLI setup");
	host.authenticated(seventh, core::host_profile_id{ 3U }, "enterprise.example");
	host.authenticated(seventh, core::host_profile_id{ 3U }, "enterprise.example");
	expect(port.Opened.size() == 4U, "chosen profile opens the destination once");
	host.submit("https://enterprise.example/o/r/pull/50");
	// Avoid recursive presentation while exercising cancellation during selection.
	port.OnPresent = [&] {
		if (host.pending()) {
			host.cancel();
		}
	};
	expect(!host.begin_authentication(host.pending()->Request, core::host_profile_id{ 3U }), "reentrant cancellation defeats account choice");

	// [[BPR-0987]]. A link to the pull request the receiving window already
	// shows reads nothing again, and says so, so the window keeps its view.
	port.OnPresent = { };
	port.Profiles  = { { core::host_profile_id{ 1U }, "github.com", true } };
	port.Navigate  = true;
	host.submit("o/r#45");
	expect(port.Projection.Opened && port.Projection.Navigated && port.Projection.Notice.empty(), "a destination the window already shows is reported as navigated");
	port.Navigate = false;
	host.submit("o/r#46");
	expect(port.Projection.Opened && !port.Projection.Navigated, "a new read is not navigated");

	return EXIT_SUCCESS;
}
