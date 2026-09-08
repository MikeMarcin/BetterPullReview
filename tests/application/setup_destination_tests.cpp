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
		std::vector<core::setup_destination>                Opened;
		application::setup_destination_projection           Projection;
		auto                                                profiles() const -> std::vector<application::setup_profile_observation> override { return Profiles; }
		auto                                                owner() const -> core::setup_owner_id override { return Owner; }
		auto                                                shorthand_hostname() const -> std::string override { return "github.com"; }
		void                                                configure(const core::pending_setup_destination& aPending) override { Configurations.push_back(aPending); }
		void                                                authenticate(const core::pending_setup_destination& aPending) override { Authentications.push_back(aPending); }
		void                                                open(core::host_profile_id, const core::setup_destination& aDestination) override { Opened.push_back(aDestination); }
		void                                                present(const application::setup_destination_projection& aProjection) override
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
	expect(port.Projection.Notice.find("location is unavailable") != std::string::npos, "unmapped location has explicit explanation");
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

	return EXIT_SUCCESS;
}
