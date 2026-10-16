// RFC-BPR-0084. The `resolve_body_images` GraphQL read: its closed request
// shape, outbound validation, and the normalization of `bodyHTML` into served
// image addresses that never leave the adapter as HTML.
#include "github_host_profile.hpp"
#include "github_paged_read.hpp"
#include "github_read_contract.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto profile(const application::github_server_contract aServer) -> validated_github_host_profile
	{
		const auto ghes   = aServer == application::github_server_contract::ghes_3_18_2022_11_28;
		const auto result = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 22U },
			.Revision       = application::github_profile_revision{ 3U },
			.Server         = aServer,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = ghes ? "ghe.bpr.invalid" : "github.com",
			.RestRoot       = ghes ? "https://ghe.bpr.invalid/api/v3" : "https://api.github.com",
			.GraphqlRoot    = ghes ? "https://ghe.bpr.invalid/api/graphql" : "https://api.github.com/graphql",
		});
		expect(result.has_value(), "profile");
		return *result;
	}

	auto semantic(std::vector<std::string> aNodes) -> semantic_read_request
	{
		auto request              = semantic_read_request{ };
		request.Server            = server_contract::github_com;
		request.Operation         = read_operation::resolve_body_images;
		request.Owner             = "octo-org";
		request.Repository        = "review-repository";
		request.PullRequestNumber = 17U;
		request.Page              = 1U;
		request.Nodes             = std::move(aNodes);
		return request;
	}

	void request_shape()
	{
		const auto compiled = compile_read_request(semantic({ "PR_kwDOA", "IC_kwDOB" }));
		expect(compiled.has_value(), "compiles");
		expect(compiled->Method == http_method::post && compiled->Path == "/graphql", "a GraphQL POST");
		expect(compiled->Body.starts_with(R"({"query":"query ResolveBodyImages($ids:[ID!]!,$name:String!,$number:Int!,$owner:String!))"), "the closed query document");
		expect(compiled->Body.ends_with(R"("variables":{"ids":["PR_kwDOA","IC_kwDOB"],"name":"review-repository","number":17,"owner":"octo-org"}})"), "the variables, in canonical order");
		expect(compiled->Body.find("bodyHTML") != std::string::npos && compiled->Body.find("mutation") == std::string::npos, "reads bodyHTML, mutates nothing");
		expect(validate_outbound_read_request(*compiled).has_value(), "the outbound validator admits it");
		auto ghes   = semantic({ "PR_kwDOA" });
		ghes.Server = server_contract::ghes_3_18;
		expect(compile_read_request(ghes)->Path == "/api/graphql", "GHES path");

		for (const auto& nodes : std::vector<std::vector<std::string>>{ { }, { "a b" }, { "x\"y" }, { std::string(129, 'a') }, { "PR_1", "PR_1" } }) {
			expect(!compile_read_request(semantic(nodes)).has_value(), "invalid node lists refuse");
		}
		std::vector<std::string> many;
		for (int index = 0; index <= 25; ++index) {
			many.push_back("IC_" + std::to_string(index));
		}
		expect(!compile_read_request(semantic(many)).has_value(), "more than 25 nodes refuse");
		auto other      = semantic({ "PR_kwDOA" });
		other.Operation = read_operation::get_pull_request;
		expect(!compile_read_request(other).has_value(), "nodes on another read refuse");

		auto tampered = *compiled;
		tampered.Body.replace(tampered.Body.find("IC_kwDOB"), 8, "IC_kwDOC");
		expect(!validate_outbound_read_request(tampered).has_value(), "a body that differs from the semantic request refuses");
		tampered = *compiled;
		tampered.Body.replace(tampered.Body.find("query ResolveBodyImages"), 5, "mutation");
		expect(!validate_outbound_read_request(tampered).has_value(), "a mutation document refuses");
	}

	class canned_transport final : public github_wire_transport_port
	{
	public:
		explicit canned_transport(std::string aBody)
			: Body{ std::move(aBody) }
		{
		}

		auto perform(const github_wire_request& aRequest, std::stop_token) -> github_wire_completion override
		{
			Requests.push_back(aRequest);
			return github_wire_success{
				.Correlation = aRequest.Correlation,
				.Response    = {
					.Status       = 200,
					.Headers      = { { "Content-Type", "application/json; charset=utf-8" }, { "X-RateLimit-Limit", "5000" }, { "X-RateLimit-Remaining", "4990" }, { "X-RateLimit-Reset", "2000000000" }, { "X-RateLimit-Resource", "graphql" } },
					.Body         = Body,
					.Completeness = page_completeness::complete,
				},
			};
		}

		std::string                      Body;
		std::vector<github_wire_request> Requests;
	};

	auto request(const validated_github_host_profile& aProfile) -> application::markdown_image_resolution_request
	{
		auto result    = application::markdown_image_resolution_request{ };
		result.Context = core::pull_request_request_context{
			.Request             = core::request_id{ 1 },
			.ApplicationSession  = core::application_session_id{ 2 },
			.Profile             = aProfile.profile(),
			.Host                = aProfile.host(),
			.Account             = core::github_account_id{ 3 },
			.Repository          = { .Owner = "octo-org", .Name = "review-repository" },
			.Number              = core::pull_request_number{ 17 },
			.ExpectedRepository  = core::repository_id{ 4 },
			.ExpectedPullRequest = core::pull_request_id{ 5 },
			.Capabilities        = core::capability_revision{ 6 },
			.IdentityGeneration  = core::github_identity_generation{ 7 },
			.Generation          = core::request_generation{ 8 },
		};
		result.Nodes           = { "PR_node", "IC_deleted", "IC_node" };
		result.RepositoryNode  = "R_node";
		result.PullRequestNode = "PR_node";
		return result;
	}

	auto answer(std::string_view aRepository, std::string_view aPull, std::string aNodes) -> std::string
	{
		return std::string{ R"({"data":{"repository":{"id":")" } + std::string{ aRepository } + R"(","pullRequest":{"id":")" + std::string{ aPull } + R"("}},"nodes":)" + aNodes + "}}";
	}

	void normalization()
	{
		const auto dotcom = profile(application::github_server_contract::github_com_2022_11_28);
		const auto nodes  = std::string{ R"([{"__typename":"PullRequest","id":"PR_node","bodyHTML":"<p><a href=\"x\"><img src=\"https://private-user-images.githubusercontent.com/1/2-25addea2-357a-42c0-a1c8-fba0163382cc.png?jwt=a.b&amp;c=d\" alt=\"shot\"></a></p>"},null,)" }
			+ R"({"__typename":"IssueComment","id":"IC_node","bodyHTML":"<img src=\"https://camo.githubusercontent.com/h/x\" data-canonical-src=\"https://e.example/a.png\"><code>&lt;img src=\"no\"&gt;</code>"}])";
		auto       wire   = canned_transport{ answer("R_node", "PR_node", nodes) };
		const auto result = perform_markdown_image_resolution(wire, dotcom, request(dotcom), { });
		expect(result.has_value() && result->Bodies.size() == 3U, "every requested body answered in order");
		expect(result->Bodies[0].Node == "PR_node" && result->Bodies[0].Served.size() == 1U
				   && result->Bodies[0].Served[0].Source == "https://private-user-images.githubusercontent.com/1/2-25addea2-357a-42c0-a1c8-fba0163382cc.png?jwt=a.b&c=d",
			   "the signed served address, references decoded");
		expect(result->Bodies[1].Served.empty(), "a deleted body serves nothing");
		expect(result->Bodies[2].Served.size() == 1U && result->Bodies[2].Served[0].Canonical == "https://e.example/a.png", "camo with its canonical source; escaped markup is not an image");
		expect(result->RateLimits.Graphql.has_value() && result->RateLimits.Graphql->Remaining == 4990U, "GraphQL rate observation kept");
		expect(wire.Requests.size() == 1U && wire.Requests[0].Wire.Operation == read_operation::resolve_body_images && wire.Requests[0].Wire.Semantic.Nodes == request(dotcom).Nodes, "one request naming exactly the nodes");

		const auto refused = [&](std::string aBody) {
			auto transport = canned_transport{ std::move(aBody) };
			return !perform_markdown_image_resolution(transport, dotcom, request(dotcom), { }).has_value();
		};
		expect(refused(answer("R_other", "PR_node", nodes)), "another repository refuses");
		expect(refused(answer("R_node", "PR_other", nodes)), "another pull request refuses");
		expect(refused(answer("R_node", "PR_node", R"([{"__typename":"PullRequest","id":"PR_node","bodyHTML":""}])")), "a missing answer refuses");
		expect(refused(answer("R_node", "PR_node", R"([{"__typename":"PullRequest","id":"IC_node","bodyHTML":""},null,{"__typename":"IssueComment","id":"PR_node","bodyHTML":""}])")), "answers out of order refuse");
		expect(refused(answer("R_node", "PR_node", R"([{"__typename":"Commit","id":"PR_node","bodyHTML":""},null,{"__typename":"IssueComment","id":"IC_node","bodyHTML":""}])")), "an unexpected type refuses");
		expect(refused(answer("R_node", "PR_node", R"([{"__typename":"PullRequest","id":"PR_node","bodyHTML":7},null,null])")), "a non-string body refuses");
		expect(refused(R"({"errors":[{"type":"NOT_FOUND"}],"data":null})"), "GraphQL errors refuse");
	}
} // namespace

int main()
{
	request_shape();
	normalization();
	std::cout << "markdown-image-resolution:pass\n";
	return EXIT_SUCCESS;
}
