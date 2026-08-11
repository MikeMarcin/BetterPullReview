#include "live_review_mutation_provider.hpp"

#include "bpr/application/review_mutation_coordinator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	static_assert(std::is_base_of_v<
				  application::review_mutation_port,
				  live_review_mutation_provider>);

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto target() -> application::review_mutation_target_binding
	{
		return {
			.Semantic = {
				.Host         = core::git_host_id{ 11U },
				.Repository   = core::repository_id{ 12U },
				.PullRequest  = core::pull_request_id{ 13U },
				.ExpectedHead = commit(0x11U),
			},
			.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.Number                   = core::pull_request_number{ 1U },
			.RepositoryDatabaseId     = 1'328'277'631U,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestDatabaseId    = 4'246'446'933U,
			.PullRequestNodeId        = "PR_kwDOTyvof879G6NV",
			.BaseRepositoryDatabaseId = 1'328'277'631U,
			.Open                     = true,
		};
	}

	[[nodiscard]] auto identity() -> application::review_mutation_acting_identity
	{
		return {
			.Account    = core::github_account_id{ 21U },
			.DatabaseId = 22U,
			.NodeId     = "MDQ6VXNlcjIy",
			.Login      = "fixture-reviewer",
		};
	}

	[[nodiscard]] auto credential_context()
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ 31U },
			.ApplicationSession = core::application_session_id{ 32U },
			.Authority          = {
				.Profile         = core::host_profile_id{ 33U },
				.ProfileRevision = application::github_profile_revision{ 34U },
				.Host            = target().Semantic.Host,
				.Hostname        = application::github_canonical_hostname{ "github.com" },
				.Account         = identity().Account,
				.ServerContract  = application::github_server_contract::github_com_2022_11_28,
			},
			.IdentityGeneration = core::github_identity_generation{ 35U },
		};
	}

	[[nodiscard]] auto configuration() -> live_review_mutation_configuration
	{
		return {
			.CredentialContext = credential_context(),
			.Target            = target(),
			.ActingIdentity    = identity(),
		};
	}

	[[nodiscard]] auto request() -> application::review_mutation_request
	{
		const auto proposal = core::review_publication_proposal{
			.Draft  = core::review_draft_id{ 41U },
			.Anchor = {
				.Comparison = opaque<core::file_comparison_digest>(0x22U),
				.Side       = core::diff_side::right,
				.Line       = 12U,
			},
			.Body        = "Please rename this.",
			.Disposition = core::review_disposition::comment,
		};
		return {
			.Proposal       = proposal,
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.IdempotencyKey = application::idempotency_key_for(proposal, target().Semantic),
			.LiveBinding    = application::review_mutation_live_binding{
				.Target         = target(),
				.ActingIdentity = identity(),
				.Anchor         = {
					.Semantic = proposal.Anchor,
					.Path     = "fixture/review.txt",
					.Commit   = target().Semantic.ExpectedHead,
				},
			},
		};
	}

	[[nodiscard]] auto request_at_path(std::string aPath)
		-> application::review_mutation_request
	{
		auto value                     = request();
		value.LiveBinding->Anchor.Path = std::move(aPath);
		return value;
	}

	[[nodiscard]] auto target_response(
		const std::uint64_t         aRepository = 1'328'277'631U,
		const core::git_commit_oid& aHead       = target().Semantic.ExpectedHead)
		-> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = std::string{ "{\"id\":4246446933,"
									   "\"node_id\":\"PR_kwDOTyvof879G6NV\",\"number\":1,"
									   "\"state\":\"open\",\"head\":{\"sha\":\"" }
					+ std::string{ core::canonical_hex(aHead).view() }
					+ "\"},\"base\":{\"repo\":{\"id\":"
					+ std::to_string(aRepository)
					+ ",\"node_id\":\"R_kgDOTyvofw\","
					  "\"full_name\":\"MikeMarcin/BPR-Test\"}}}",
			},
		};
	}

	[[nodiscard]] auto viewer_response() -> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = "{\"login\":\"fixture-reviewer\",\"id\":22,"
						  "\"node_id\":\"MDQ6VXNlcjIy\"}",
			},
		};
	}

	[[nodiscard]] auto comment_response() -> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 201,
				.Body   = std::string{ "{\"id\":44,\"node_id\":\"PRRC_fixture_44\","
									   "\"path\":\"fixture/review.txt\",\"commit_id\":\"" }
					+ std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() }
					+ "\",\"side\":\"RIGHT\",\"line\":13,"
					  "\"body\":\"Please rename this.\",\"user\":{"
					  "\"login\":\"fixture-reviewer\",\"id\":22,"
					  "\"node_id\":\"MDQ6VXNlcjIy\"}}",
			},
		};
	}

	class fake_transport final : public github_mutation_wire_transport
	{
	public:
		[[nodiscard]] auto perform_review_mutation(
			const github_mutation_wire_request& aRequest,
			std::stop_token) -> github_mutation_wire_completion override
		{
			Requests.push_back(aRequest);
			if (Completions.empty()) {
				return github_mutation_wire_error{
					.Failure = application::review_mutation_failure::unavailable,
				};
			}
			auto next = std::move(Completions.front());
			Completions.pop_front();
			return next;
		}

		void exact_submit_course()
		{
			Completions.push_back(viewer_response());
			Completions.push_back(target_response());
			Completions.push_back(comment_response());
		}

		std::deque<github_mutation_wire_completion> Completions;
		std::vector<github_mutation_wire_request>   Requests;
	};

	void test_observe_then_immediate_revalidate_then_submit()
	{
		auto transport = fake_transport{ };
		transport.exact_submit_course();
		auto       session = review_mutation_session{ transport, configuration() };
		const auto receipt = session.submit(request());
		expect(receipt.has_value() && receipt->ServerCommentId == "PRRC_fixture_44",
			   "exact publication succeeds");
		expect(transport.Requests.size() == 3U
				   && transport.Requests[0].Wire.Operation
					   == review_mutation_operation::observe_viewer
				   && transport.Requests[1].Wire.Operation
					   == review_mutation_operation::observe_pull_request
				   && transport.Requests[2].Wire.Operation
					   == review_mutation_operation::create_review_comment,
			   "target and acting identity are revalidated immediately before post");
		expect(std::ranges::all_of(transport.Requests, [](const auto& aWire) {
				   return aWire.Context == credential_context();
			   }),
			   "every operation carries the exact credential authority without material");
	}

	void test_invalid_bindings_submit_nothing()
	{
		for (auto variant = 0; variant < 29; ++variant) {
			auto transport = fake_transport{ };
			auto session   = review_mutation_session{ transport, configuration() };
			auto changed   = request();
			switch (variant) {
			case 0:
				changed.LiveBinding.reset();
				break;
			case 1:
				changed.LiveBinding->Target.Semantic.Host = core::git_host_id{ 99U };
				break;
			case 2:
				changed.LiveBinding->Target.Semantic.Repository = core::repository_id{ 99U };
				break;
			case 3:
				changed.LiveBinding->Target.Semantic.PullRequest = core::pull_request_id{ 99U };
				break;
			case 4:
				changed.LiveBinding->Target.Semantic.ExpectedHead = commit(0x44U);
				break;
			case 5:
				changed.LiveBinding->Target.Repository.Owner = "Other";
				break;
			case 6:
				changed.LiveBinding->Target.Repository.Name = "Other";
				break;
			case 7:
				changed.LiveBinding->Target.Number = core::pull_request_number{ 2U };
				break;
			case 8:
				changed.LiveBinding->Target.RepositoryDatabaseId += 1U;
				break;
			case 9:
				changed.LiveBinding->Target.RepositoryNodeId = "R_other";
				break;
			case 10:
				changed.LiveBinding->Target.PullRequestDatabaseId += 1U;
				break;
			case 11:
				changed.LiveBinding->Target.PullRequestNodeId = "PR_other";
				break;
			case 12:
				changed.LiveBinding->Target.BaseRepositoryDatabaseId += 1U;
				break;
			case 13:
				changed.LiveBinding->Target.Open = false;
				break;
			case 14:
				changed.LiveBinding->ActingIdentity.Account = core::github_account_id{ 99U };
				break;
			case 15:
				changed.LiveBinding->ActingIdentity.DatabaseId += 1U;
				break;
			case 16:
				changed.LiveBinding->ActingIdentity.NodeId = "MDQ6Other";
				break;
			case 17:
				changed.LiveBinding->ActingIdentity.Login = "other";
				break;
			case 18:
				changed.LiveBinding->Anchor.Semantic.Comparison
					= opaque<core::file_comparison_digest>(0x55U);
				break;
			case 19:
				changed.LiveBinding->Anchor.Semantic.Side = core::diff_side::left;
				break;
			case 20:
				changed.LiveBinding->Anchor.Semantic.Line += 1U;
				break;
			case 21:
				changed.LiveBinding->Anchor.Path = "/absolute";
				break;
			case 22:
				changed.LiveBinding->Anchor.Commit = commit(0x66U);
				break;
			case 23:
				changed.Proposal.Anchor.Line += 1U;
				break;
			case 24:
				changed.Target.Repository = core::repository_id{ 99U };
				break;
			case 25:
				changed.ActingIdentity = core::github_account_id{ 99U };
				break;
			case 26:
				changed.IdempotencyKey.assign(64U, '0');
				break;
			case 27:
				changed.Proposal.Disposition = core::review_disposition::approve;
				break;
			case 28:
				changed.Proposal.Body.clear();
				break;
			default:
				std::abort();
			}
			const auto refused = session.submit(changed);
			expect(!refused.has_value() && transport.Requests.empty(),
				   "invalid request produces zero transport submissions");
		}
	}

	void test_revalidation_drift_and_identity_failure_stop_before_write()
	{
		auto drift = fake_transport{ };
		drift.Completions.push_back(viewer_response());
		drift.Completions.push_back(target_response(1'328'277'632U));
		auto       driftSession = review_mutation_session{ drift, configuration() };
		const auto refused      = driftSession.submit(request());
		expect(!refused.has_value() && drift.Requests.size() == 2U,
			   "repository drift stops before write");

		auto movedHead = fake_transport{ };
		movedHead.Completions.push_back(viewer_response());
		movedHead.Completions.push_back(target_response(
			1'328'277'631U, commit(0x77U)));
		auto       movedSession = review_mutation_session{ movedHead, configuration() };
		const auto stale        = movedSession.submit(request());
		expect(!stale.has_value()
				   && stale.error() == application::review_mutation_failure::stale_head
				   && movedHead.Requests.size() == 2U,
			   "a fresh moved-head observation stops before write");

		auto identityFailure = fake_transport{ };
		identityFailure.Completions.emplace_back(github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = "{\"login\":\"other\",\"id\":22,"
						  "\"node_id\":\"MDQ6VXNlcjIy\"}",
			},
		});
		auto       identitySession = review_mutation_session{ identityFailure, configuration() };
		const auto wrongActor      = identitySession.submit(request());
		expect(!wrongActor.has_value() && identityFailure.Requests.size() == 1U,
			   "acting identity drift stops before write");
	}

	void test_idempotency_and_indeterminate_attempts_never_duplicate()
	{
		auto transport = fake_transport{ };
		transport.exact_submit_course();
		auto       session            = review_mutation_session{ transport, configuration() };
		const auto first              = session.submit(request());
		const auto requestsAfterFirst = transport.Requests.size();
		const auto duplicate          = session.submit(request());
		expect(first.has_value() && duplicate.has_value() && duplicate->Duplicate
				   && duplicate->ServerCommentId == first->ServerCommentId
				   && transport.Requests.size() == requestsAfterFirst,
			   "known success returns the same identity without a second post");
		const auto pathB = session.submit(request_at_path("fixture/other.txt"));
		expect(!pathB.has_value()
				   && transport.Requests.size() == requestsAfterFirst,
			   "a reused key with a different resolved path cannot reuse a receipt or submit");

		auto uncertain = fake_transport{ };
		uncertain.Completions.push_back(viewer_response());
		uncertain.Completions.push_back(target_response());
		uncertain.Completions.emplace_back(github_mutation_wire_error{
			.Failure = application::review_mutation_failure::unavailable,
		});
		auto       uncertainSession = review_mutation_session{ uncertain, configuration() };
		const auto lost             = uncertainSession.submit(request());
		const auto count            = uncertain.Requests.size();
		const auto retry            = uncertainSession.submit(request());
		const auto otherPath
			= uncertainSession.submit(request_at_path("fixture/other.txt"));
		expect(!lost.has_value() && !retry.has_value()
				   && !otherPath.has_value()
				   && uncertain.Requests.size() == count,
			   "an indeterminate post guards same-key retries and resolved-path substitution");
	}

	void test_typed_transport_outcomes_propagate_without_diagnostics()
	{
		auto refusedTransport = fake_transport{ };
		refusedTransport.Completions.emplace_back(github_mutation_wire_error{
			.Failure = application::review_mutation_failure::refused_by_server,
		});
		auto       refusedSession = review_mutation_session{ refusedTransport, configuration() };
		const auto refused        = refusedSession.observe_target();
		expect(!refused.has_value()
				   && refused.error() == application::review_mutation_failure::refused_by_server,
			   "server refusal remains typed");

		auto cancelledTransport = fake_transport{ };
		cancelledTransport.Completions.emplace_back(github_mutation_wire_cancelled{ });
		auto       cancelledSession = review_mutation_session{ cancelledTransport, configuration() };
		const auto cancelled        = cancelledSession.observe_target();
		expect(!cancelled.has_value()
				   && cancelled.error() == application::review_mutation_failure::cancelled,
			   "cancellation remains typed");
	}
} // namespace

auto main() -> int
{
	test_observe_then_immediate_revalidate_then_submit();
	test_invalid_bindings_submit_nothing();
	test_revalidation_drift_and_identity_failure_stop_before_write();
	test_idempotency_and_indeterminate_attempts_never_duplicate();
	test_typed_transport_outcomes_propagate_without_diagnostics();
	std::cout << "live-review-mutation-provider-tests:pass\n";
	return EXIT_SUCCESS;
}
