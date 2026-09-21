#include "bpr/core/markdown_navigation.hpp"

#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr::core;
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
	const markdown_context          context{ { default_bpr_profile_id, bpr_window_id{ 2 }, bpr_window_generation{ 3 } }, application_session_id{ 4 }, github_identity_generation{ 5 }, provider_snapshot_id{ 6 }, 7 };
	const markdown_link_observation observation{ context, true, true, "ghe.example.invalid", "owner", "repository", "2222222222222222222222222222222222222222", pull_request_number{ 42 } };
	for (const auto* address : { "https://example.invalid/path?q=safe#section", "http://example.invalid:8080/doc", "HTTPS://EXAMPLE.INVALID/read", "https://example.invalid/%E6%97%A5" }) {
		const auto admitted = admit_markdown_link({ context, { }, address }, observation);
		expect(admitted && admitted->Kind == markdown_destination_kind::absolute_web && admitted->Target == address, "explicit web destination is preserved exactly");
	}
	for (const auto* address : { "", "javascript:alert(1)", "file:///tmp/a", "data:text/html,x", "qrc:/a", "mailto:person@example.invalid", "https://user:password@example.invalid/a", "https://user@example.invalid/a", "https://example.invalid\\@evil.invalid", "https://%65xample.invalid/a", "https:///path", "https://example.invalid:0/a", "https://example.invalid:65536/a", "https://example.invalid/%00", "https://example.invalid/%0d%0a", "https://example.invalid/?access_token=x", "https://example.invalid/?%74oken=x", "https://example.invalid/?client_secret=x", "https://example.invalid/a b", "https://example.invalid/%xz", "//evil.invalid/a", "/absolute/ambiguous", "../outside.md", "./%2e%2e/outside", "?query=ambiguous" }) {
		const auto admitted = admit_markdown_link({ context, { }, address }, observation);
		expect(!admitted && admitted.error() == markdown_link_result::refused, address);
	}
	const auto relative = admit_markdown_link({ context, "comment", "./docs/../guide.md#start" }, observation);
	expect(relative && relative->Kind == markdown_destination_kind::repository_path && relative->Target == "guide.md#start", "relative destination is normalized only with an exact accepted repository/head");
	const auto fragment = admit_markdown_link({ context, { }, "#discussion" }, observation);
	expect(fragment && fragment->Kind == markdown_destination_kind::pull_request_fragment && fragment->Target == "#discussion", "fragment identifies the exact accepted PR");
	auto missing     = observation;
	missing.Revision = { };
	expect(!admit_markdown_link({ context, { }, "readme.md" }, missing), "relative paths require a full exact revision");
	auto stale = context;
	++stale.Revision;
	expect(admit_markdown_link({ stale, { }, "https://example.invalid" }, observation).error() == markdown_link_result::stale, "stale publication refuses");
	stale               = context;
	stale.Window.Window = bpr_window_id{ 99 };
	expect(admit_markdown_link({ stale, { }, "https://example.invalid" }, observation).error() == markdown_link_result::stale, "sibling window refuses");
	missing               = observation;
	missing.BodyAvailable = false;
	expect(admit_markdown_link({ context, "missing", "https://example.invalid" }, missing).error() == markdown_link_result::stale, "missing body refuses");
	missing                  = observation;
	missing.BrowserAvailable = false;
	expect(admit_markdown_link({ context, { }, "https://example.invalid" }, missing).error() == markdown_link_result::unavailable, "absent capability is explicit");
	expect(!admit_markdown_link({ context, { }, std::string(4097, 'a') }, observation), "destination input is bounded");
	std::cout << "Markdown navigation policy passed\n";
}
