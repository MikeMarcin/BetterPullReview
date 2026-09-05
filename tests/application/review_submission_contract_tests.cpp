#include "bpr/application/review_submission_contract.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto request() -> application::review_submission_request
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(1U);
		const auto target = core::review_mutation_target{
			.Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = core::git_commit_oid{ core::git_hash_algorithm::sha256, bytes }
		};
		const auto draft = core::review_submission_draft{
			.Id = core::review_submission_draft_id{ 7U }, .Target = target, .ActingAccount = core::github_account_id{ 4U }, .Disposition = core::review_disposition::approve
		};
		const auto begun = core::apply(draft, { .Id = draft.Id, .Kind = core::review_submission_command_kind::begin_submit });
		return { .Proposal = *core::propose(*begun.Draft), .IdempotencyKey = "fixture-review-key", .LiveBinding = application::review_submission_live_binding{ .Target = { .Semantic = target, .Hostname = application::github_canonical_hostname{ "github.com" }, .Repository = { .Owner = "fixture-owner", .Name = "fixture-repo" }, .Number = core::pull_request_number{ 1U }, .RepositoryDatabaseId = 11U, .RepositoryNodeId = "R_fixture", .PullRequestDatabaseId = 12U, .PullRequestNodeId = "PR_fixture", .BaseRepositoryDatabaseId = 11U, .BaseOid = target.ExpectedHead, .BaseRef = "main", .HeadRef = "fixture-review", .Open = true }, .ActingIdentity = { .Account = draft.ActingAccount, .DatabaseId = 5U, .NodeId = "U_fixture", .Login = "fixture-reviewer" } } };
	}

	[[nodiscard]] auto receipt() -> application::review_submission_receipt
	{
		const auto proposal = request().Proposal;
		return { .ReviewDatabaseId = application::review_database_id{ 42U },
				 .ReviewNodeId     = "PRR_fixture",
				 .Target           = proposal.Target,
				 .ActingAccount    = proposal.ActingAccount,
				 .Disposition      = proposal.Disposition };
	}

	void test_request_contract()
	{
		const auto original = request();
		expect(application::valid(original), "core-created empty Approve with complete binding is valid");
		auto local = original;
		local.LiveBinding.reset();
		expect(application::valid(local), "fixture contract permits absent live binding");
		for (const auto mutation : std::array<void (*)(application::review_submission_request&), 17>{
				 [](auto& aRequest) { aRequest.Proposal.Draft = { }; },
				 [](auto& aRequest) { aRequest.Proposal.Attempt = { }; },
				 [](auto& aRequest) { aRequest.Proposal.Target.Host = { }; },
				 [](auto& aRequest) { aRequest.Proposal.ActingAccount = { }; },
				 [](auto& aRequest) { aRequest.Proposal.Disposition = static_cast<core::review_disposition>(255U); },
				 [](auto& aRequest) { aRequest.Proposal.Disposition = core::review_disposition::comment; },
				 [](auto& aRequest) { aRequest.Proposal.Disposition = core::review_disposition::request_changes; },
				 [](auto& aRequest) { aRequest.Proposal.Body = "\xED\xA0\x80"; },
				 [](auto& aRequest) { aRequest.Proposal.Body.assign(core::maximum_review_submission_body_bytes + 1U, 'x'); },
				 [](auto& aRequest) { aRequest.LiveBinding->Target.Semantic.Host = core::git_host_id{ 99U }; },
				 [](auto& aRequest) { aRequest.LiveBinding->Target.Semantic.Repository = core::repository_id{ 99U }; },
				 [](auto& aRequest) { aRequest.LiveBinding->Target.Semantic.PullRequest = core::pull_request_id{ 99U }; },
				 [](auto& aRequest) { auto bytes = aRequest.Proposal.Target.ExpectedHead.bytes(); bytes[0] = 2U; aRequest.LiveBinding->Target.Semantic.ExpectedHead = core::git_commit_oid{ core::git_hash_algorithm::sha256, bytes }; },
				 [](auto& aRequest) { aRequest.LiveBinding->ActingIdentity.Account = core::github_account_id{ 99U }; },
				 [](auto& aRequest) { aRequest.LiveBinding->Target.Open = false; },
				 [](auto& aRequest) { aRequest.LiveBinding->Target.RepositoryNodeId.clear(); },
				 [](auto& aRequest) { aRequest.LiveBinding->ActingIdentity.DatabaseId = 0U; } }) {
			auto changed = original;
			mutation(changed);
			expect(!application::valid(changed), "invalid proposal or mismatched/incomplete live authority refused");
		}
		for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve,
										core::review_disposition::request_changes }) {
			auto changed                 = original;
			changed.Proposal.Disposition = disposition;
			changed.Proposal.Body        = "Summary";
			expect(application::valid(changed), "all ordinary nonblank dispositions accepted");
		}
		for (const auto invalid : { "", "has space", "line\nbreak", "\x7F", "\xC2\xA0" }) {
			auto changed           = original;
			changed.IdempotencyKey = invalid;
			expect(!application::valid(changed), "empty/control/non-ASCII idempotency keys refused");
		}
		local.IdempotencyKey.assign(application::maximum_review_submission_idempotency_key_bytes, 'k');
		expect(application::valid(local), "exact idempotency key ceiling accepted");
		local.IdempotencyKey += 'k';
		expect(!application::valid(local), "idempotency key overflow refused");
	}

	void test_receipt_contract()
	{
		static_assert(!std::is_same_v<application::review_submission_receipt, application::review_mutation_receipt>);
		static_assert(!std::is_convertible_v<std::uint64_t, application::review_database_id>);
		const auto original = receipt();
		expect(application::valid(original, request()), "receipt matches exact request");
		for (const auto mutation : std::array<void (*)(application::review_submission_receipt&), 12>{
				 [](auto& aReceipt) { aReceipt.ReviewDatabaseId = { }; },
				 [](auto& aReceipt) { aReceipt.ReviewNodeId.clear(); },
				 [](auto& aReceipt) { aReceipt.ReviewNodeId = "bad\nnode"; },
				 [](auto& aReceipt) { aReceipt.ReviewNodeId.assign(application::maximum_review_submission_node_id_bytes + 1U, 'n'); },
				 [](auto& aReceipt) { aReceipt.Target.Host = core::git_host_id{ 99U }; },
				 [](auto& aReceipt) { aReceipt.Target.Repository = core::repository_id{ 99U }; },
				 [](auto& aReceipt) { aReceipt.Target.PullRequest = core::pull_request_id{ 99U }; },
				 [](auto& aReceipt) { auto bytes = aReceipt.Target.ExpectedHead.bytes(); bytes[0] = 2U; aReceipt.Target.ExpectedHead = core::git_commit_oid{ core::git_hash_algorithm::sha256, bytes }; },
				 [](auto& aReceipt) { aReceipt.ActingAccount = core::github_account_id{ 99U }; },
				 [](auto& aReceipt) { aReceipt.ActingAccount = { }; },
				 [](auto& aReceipt) { aReceipt.Disposition = core::review_disposition::comment; },
				 [](auto& aReceipt) { aReceipt.Disposition = static_cast<core::review_disposition>(255U); } }) {
			auto changed = original;
			mutation(changed);
			expect(!application::valid(changed, request()), "malformed or mismatched receipt refused");
		}
		auto boundary             = original;
		boundary.ReviewDatabaseId = application::review_database_id{ std::numeric_limits<std::uint64_t>::max() };
		boundary.ReviewNodeId.assign(application::maximum_review_submission_node_id_bytes, 'n');
		expect(application::valid(boundary, request()), "full positive uint64 review ID and exact node ceiling accepted");
		auto invalidRequest = request();
		invalidRequest.IdempotencyKey.clear();
		expect(!application::valid(original, invalidRequest), "matching receipt cannot validate an invalid request");
	}
} // namespace

int main()
{
	test_request_contract();
	test_receipt_contract();
	return EXIT_SUCCESS;
}
