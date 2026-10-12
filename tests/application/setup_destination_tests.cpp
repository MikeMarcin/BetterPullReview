#include "bpr/application/manual_executor.hpp"
#include "bpr/application/repository_addition_input.hpp"
#include "bpr/application/setup_destination.hpp"
#include "bpr/core/repository_addition.hpp"

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
			if (aDestination.Kind == core::setup_destination_kind::repository) {
				return application::setup_destination_opening::repository_prepared;
			}
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
namespace {
	void test_repository_addition_input_and_admission()
	{
		using problem   = application::repository_addition_input_problem;
		const auto bare = application::parse_repository_addition(" \tOcto/repo.name\r\n", "GHE.EXAMPLE");
		expect(bare && bare->Hostname == "ghe.example" && bare->Repository.Owner == "Octo" && bare->Repository.Name == "repo.name", "bare repository uses displayed host and preserves locator spelling");
		const auto url = application::parse_repository_addition("https://GITHUB.COM/Octo/repo/", "ghe.example");
		expect(url && url->Hostname == "github.com" && url->Repository.Owner == "Octo" && url->Repository.Name == "repo", "URL explicitly names its own host without routing to displayed host");
		for (const auto* input : { "http://github.com/o/r", "https://github.com/o/r/pull/1", "https://github.com/o/r/tree/main", "https://github.com/o/r?x=1", "https://github.com/o/r#readme", "https://github.com/o/r//", "o/r/", "o/r/extra", "o/r#1", "o/r?x", "repo" }) {
			expect(!application::parse_repository_addition(input, "github.com"), "non-root shapes and PR grammar never name an addition target");
		}
		for (const auto* input : { "https://user@github.com/o/r", "https://github.com:443/o/r", "https://-bad.example/o/r", "https://bad-.example/o/r", "https://bad..example/o/r", "https://github.com./o/r" }) {
			const auto parsed = application::parse_repository_addition(input, "github.com");
			expect(!parsed && parsed.error() == problem::invalid_host, "unsafe/noncanonical host requires correction, never configuration");
		}
		expect(!application::parse_repository_addition("o/%72", "github.com") && !application::parse_repository_addition("o/..", "github.com") && !application::parse_repository_addition(std::string{ "o/r\0x", 5U }, "github.com"), "escaped, traversal and NUL repository input is refused");
		expect(!application::parse_repository_addition(std::string(4097U, 'a'), "github.com"), "addition input is bounded before normalization");
		expect(!application::parse_repository_addition("o/r", "") && !application::parse_repository_addition("o/r", "github.com:443"), "bare input needs a canonical displayed host");
		expect(core::accept_repository_addition_completion(false, true, true) && !core::accept_repository_addition_completion(true, true, true) && !core::accept_repository_addition_completion(false, false, true) && !core::accept_repository_addition_completion(false, true, false), "only exact live destination/source accepts terminal");
		using status    = core::repository_addition_status;
		const auto good = core::repository_addition_observation{ true, true, true, true, true, false, false, true };
		expect(core::admit_repository_addition(good) == status::saving, "exact admitted addition may start a save");
		auto facts     = good;
		facts.SignedIn = false;
		expect(core::admit_repository_addition(facts) == status::sign_in_required, "public preview or no accepted account cannot save");
		facts                    = good;
		facts.DestinationCurrent = false;
		expect(core::admit_repository_addition(facts) == status::destination_changed, "stale destination cannot save");
		facts               = good;
		facts.WindowCurrent = false;
		expect(core::admit_repository_addition(facts) == status::destination_changed, "closed/replaced originating window cannot save");
		facts            = good;
		facts.StoreReady = false;
		expect(core::admit_repository_addition(facts) == status::unavailable, "unknown/refused durable state cannot save");
		facts            = good;
		facts.InputValid = false;
		expect(core::admit_repository_addition(facts) == status::invalid_input, "invalid input cannot save");
		facts         = good;
		facts.Pending = true;
		expect(core::admit_repository_addition(facts) == status::busy, "only one mutation proceeds");
		facts              = good;
		facts.AlreadyAdded = true;
		facts.HasCapacity  = false;
		expect(core::admit_repository_addition(facts) == status::already_added, "duplicate is no-write success even at collection capacity");
		facts             = good;
		facts.HasCapacity = false;
		expect(core::admit_repository_addition(facts) == status::collection_limit, "new records obey existing bound");
	}
}

int main()
{
	test_repository_addition_input_and_admission();
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
		expect(location(base + "/files#r123456").Kind == kind::review_comment && location(base + "/files#r123456").Identity == 123456U,
			   "a Files changed review comment fragment, as a tour links its stops");
		expect(location(base + "#pullrequestreview-4001").Kind == kind::review && location(base + "#pullrequestreview-4001").Identity == 4001U,
			   "a review fragment");
		expect(location(base + "#issuecomment-99").Kind == kind::issue_comment && location(base + "#issuecomment-99").Identity == 99U,
			   "a discussion comment fragment");
		for (const auto* fragment : { "#diff-abcd", "#diff-", "#discussion_r", "#discussion_r0", "#discussion_r12x", "#pullrequestreview-", "#issuecomment-018", "#top", "#diff-deadbeef", "#r", "#r0", "#r12x", "#readme" }) {
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

	// Repository preparation uses the exact setup continuation, refuses anonymous
	// preview and reports only preparedness, never a PR open or a save.
	port.Profiles                       = { { core::host_profile_id{ 1U }, "github.com", false, true } };
	const auto previewsBeforeRepository = port.Previews.size();
	const auto openedBeforeRepository   = port.Opened.size();
	host.submit_repository("octo/tools");
	expect(host.pending() && host.pending()->Stage == core::setup_destination_stage::sign_in
			   && host.pending()->Destination.Kind == core::setup_destination_kind::repository
			   && !host.pending()->Destination.Number.valid() && port.Previews.size() == previewsBeforeRepository,
		   "repository setup requires sign-in even on a preview-capable host and invents no PR");
	const auto repositoryRequest = host.pending()->Request;
	host.continue_setup(repositoryRequest);
	host.cancel();
	host.authenticated(repositoryRequest, core::host_profile_id{ 1U }, "github.com");
	expect(port.Opened.size() == openedBeforeRepository && !port.Projection.RepositoryPrepared, "cancelled repository sign-in completion is inert");
	const auto configurationsBeforeRepository = port.Configurations.size();
	host.submit_repository("https://unknown.example/octo/tools/");
	expect(host.pending() && host.pending()->Stage == core::setup_destination_stage::add_server && port.Configurations.size() == configurationsBeforeRepository,
		   "an unknown repository URL retains its host for explicit server setup");
	const auto unknownRequest = host.pending()->Request;
	host.continue_setup(unknownRequest);
	expect(port.Configurations.size() == configurationsBeforeRepository + 1U, "only explicit continuation configures the repository host");
	host.cancel();
	host.configured(unknownRequest);
	expect(!host.pending() && port.Opened.size() == openedBeforeRepository, "cancelled server setup cannot prepare or save a repository");
	port.Profiles = { { core::host_profile_id{ 1U }, "github.com", true } };
	host.submit_repository("https://github.com/octo/tools");
	expect(!host.pending() && port.Projection.RepositoryPrepared && !port.Projection.Opened
			   && port.Opened.size() == openedBeforeRepository + 1U && port.Opened.back().Kind == core::setup_destination_kind::repository,
		   "accepted exact-host setup returns to a repository draft without a pull-request open");
	host.submit_repository("https://github.com/octo/tools/pull/1");
	expect(!host.pending() && !port.Projection.RepositoryPrepared && port.Opened.size() == openedBeforeRepository + 1U,
		   "repository setup refuses PR-shaped input");

	// Remembered consent admits a new validation effect, never a read by itself.
	port.Profiles          = { { core::host_profile_id{ 1U }, "github.com", false, true }, { core::host_profile_id{ 4U }, "github.com", false, false, true } };
	const auto validations = port.Authentications.size();
	const auto reads       = port.Opened.size();
	host.submit("https://github.com/o/r/pull/51");
	const auto validationRequest = host.pending()->Request;
	expect(host.pending()->Stage == core::setup_destination_stage::authenticating && port.Authentications.size() == validations + 1U && port.Authentications.back().Profile == core::host_profile_id{ 4U } && port.Opened.size() == reads, "saved exact selection validates automatically without granting a read or anonymous fallback");
	host.cancel();
	host.authenticated(validationRequest, core::host_profile_id{ 4U }, "github.com");
	expect(port.Opened.size() == reads, "late remembered validation cannot open a cancelled destination");
	port.OnPresent = [&] { if (host.pending()) { host.cancel(); } };
	host.submit("https://github.com/o/r/pull/52");
	expect(!host.pending() && port.Authentications.size() == validations + 1U, "reentrant cancellation prevents remembered validation submission");
	port.OnPresent = { };

	return EXIT_SUCCESS;
}
