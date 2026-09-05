#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "live_review_submission_provider.hpp"
#include "review_submission_session.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <type_traits>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github::detail;
	void expect(bool aCondition, std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
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
			.Hostname                 = application::github_canonical_hostname{ "github.com" },
			.ServerContract           = application::github_server_contract::github_com_2022_11_28,
			.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.Number                   = core::pull_request_number{ 1U },
			.RepositoryDatabaseId     = 1'328'277'631U,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestDatabaseId    = 4'246'446'933U,
			.PullRequestNodeId        = "PR_kwDOTyvof879G6NV",
			.BaseRepositoryDatabaseId = 1'328'277'631U,
			.BaseOid                  = commit(0x10U),
			.BaseRef                  = "main",
			.HeadRef                  = "codex/disposable-live-review-20260810",
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
					+ "\",\"ref\":\"codex/disposable-live-review-20260810\"},"
					  "\"base\":{\"sha\":\""
					+ std::string{ core::canonical_hex(target().BaseOid).view() }
					+ "\",\"ref\":\"main\",\"repo\":{\"id\":"
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

	class lease final : public application::review_mutation_transport_lease
	{ };
	auto request() -> application::review_submission_request
	{
		return { .Proposal = { .Draft = core::review_submission_draft_id{ 41U }, .Attempt = core::review_submission_attempt_id{ 1U }, .Target = target().Semantic, .ActingAccount = identity().Account, .Body = "Review summary.", .Disposition = core::review_disposition::comment }, .IdempotencyKey = "review-attempt-1", .LiveBinding = application::review_submission_live_binding{ .Target = target(), .ActingIdentity = identity() } };
	}
	auto review(const application::review_submission_request& aRequest, std::uint64_t aId = 45U, std::string_view aState = { }) -> std::string
	{
		const auto state = !aState.empty() ? aState : aRequest.Proposal.Disposition == core::review_disposition::approve ? "APPROVED"
			: aRequest.Proposal.Disposition == core::review_disposition::request_changes                                 ? "CHANGES_REQUESTED"
																														 : "COMMENTED";
		return std::string{ "{\"id\":" } + std::to_string(aId) + ",\"node_id\":\"PRR_" + std::to_string(aId) + "\",\"state\":\"" + std::string{ state } + "\",\"body\":\"" + aRequest.Proposal.Body + "\",\"commit_id\":\"1111111111111111111111111111111111111111\",\"pull_request_url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1\",\"user\":{\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"}" + (state == "PENDING" ? "}" : ",\"submitted_at\":\"2026-09-05T12:34:56Z\"}");
	}
	auto list(const std::vector<std::string>& aReviews) -> std::string
	{
		std::string body = "[";
		for (const auto& entry : aReviews) {
			if (body.size() > 1U) {
				body += ',';
			}
			body += entry;
		}
		body += ']';
		return body;
	}
	struct transport final : github_mutation_wire_transport
	{
		application::review_submission_request Request = request();
		std::vector<std::string>               Reviews;
		std::vector<review_mutation_operation> Trace;
		std::size_t                            Posts            = 0U;
		std::size_t                            NewCount         = 1U;
		bool                                   DefinitelyUnsent = false, LoseResponse = false, MalformedResponse = false, ThrowAfterSend = false, WrongActor = false, WrongHead = false, Incomplete = false, WitnessRejected = false;
		std::stop_source*                      CancelOnPost = nullptr;
		std::function<void()>                  OnTarget;
		auto                                   perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			expect(validate_outbound_review_mutation_request(aRequest.Wire).has_value(), "all session requests are canonical");
			Trace.push_back(aRequest.Wire.Operation);
			switch (aRequest.Wire.Operation) {
			case review_mutation_operation::observe_viewer: {
				auto response = viewer_response();
				if (WrongActor) {
					std::get<github_mutation_wire_success>(response).Response.Body = "{\"id\":23,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"}";
				}
				return response;
			}
			case review_mutation_operation::observe_pull_request:
				if (OnTarget) {
					OnTarget();
				}
				return target_response(target().RepositoryDatabaseId, WrongHead ? commit(0x22U) : target().Semantic.ExpectedHead);
			case review_mutation_operation::observe_reviews: return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = Incomplete ? std::vector<http_header>{ { .Name = "lInK", .Value = "next" } } : std::vector<http_header>{ }, .Body = list(Reviews) } };
			case review_mutation_operation::create_review:
				++Posts;
				if (DefinitelyUnsent) {
					return github_mutation_wire_error{ };
				}
				expect(static_cast<bool>(aWitness), "POST requires durable witness");
				WitnessRejected = !aWitness();
				for (auto index = std::size_t{ }; index < NewCount; ++index) {
					Reviews.push_back(review(Request, 45U + index));
				}
				if (CancelOnPost) {
					CancelOnPost->request_stop();
				}
				if (ThrowAfterSend) {
					throw std::runtime_error{ "lost completion" };
				}
				if (LoseResponse || WitnessRejected) {
					return github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate };
				}
				return github_mutation_wire_success{ .Response = { .Status = 200, .Body = MalformedResponse ? "{" : review(Request) } };
			default: expect(false, "unexpected transport operation"); return github_mutation_wire_error{ };
			}
		}
	};
	struct temporary_directory
	{
		std::filesystem::path Path = std::filesystem::temp_directory_path() / ("bpr-review-submission-" + std::to_string(std::random_device{ }()));
		temporary_directory()
		{
			expect(std::filesystem::create_directory(Path), "temporary root created");
			expect(adapters::sqlite::make_storage_root_owner_private(Path), "temporary root private");
		}
		~temporary_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
	};
	auto storage(const std::filesystem::path& aRoot) -> std::unique_ptr<application::review_submission_recovery_port>
	{
		auto value = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aRoot, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(value.has_value(), "submission store opens");
		return std::move(*value);
	}
	class failing_store final : public application::review_submission_recovery_port
	{
	public:
		std::unique_ptr<application::review_submission_recovery_port> Inner;
		bool                                                          FailMark = false, FailResolve = false;
		explicit failing_store(std::unique_ptr<application::review_submission_recovery_port> aInner)
			: Inner{ std::move(aInner) }
		{ }
		auto load(const std::string& aKey) -> std::expected<std::optional<application::review_submission_recovery_record>, application::review_mutation_recovery_failure> override { return Inner->load(aKey); }
		auto admit(const application::review_submission_recovery_record& aRecord) -> std::expected<application::review_submission_recovery_admission, application::review_mutation_recovery_failure> override { return Inner->admit(aRecord); }
		auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<application::review_submission_recovery_record, application::review_mutation_recovery_failure> override
		{
			if (FailMark) {
				return std::unexpected(application::review_mutation_recovery_failure::unavailable);
			}
			return Inner->mark_possibly_transmitted(aKey, aFingerprint);
		}
		auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const application::review_submission_receipt& aReceipt) -> std::expected<application::review_submission_recovery_record, application::review_mutation_recovery_failure> override
		{
			if (FailResolve) {
				return std::unexpected(application::review_mutation_recovery_failure::unavailable);
			}
			return Inner->resolve(aKey, aFingerprint, aReceipt);
		}
	};
	void test_dispositions_and_old_reviews()
	{
		for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
			temporary_directory root;
			transport           wire;
			wire.Request.Proposal.Disposition = disposition;
			if (disposition == core::review_disposition::approve) {
				wire.Request.Proposal.Body.clear();
			}
			wire.Reviews = { review(wire.Request, 40U), review(wire.Request, 41U, "PENDING"), review(wire.Request, 42U, "DISMISSED") };
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                result = session.submit_and_reconcile(wire.Request);
			expect(result.Publication == application::review_submission_publication::published && result.Request == wire.Request && result.Receipt && result.Receipt->ReviewDatabaseId.value() == 45U && result.Receipt->Disposition == disposition, "all dispositions publish exact new review with request correlation");
			expect(wire.Trace == std::vector{ review_mutation_operation::observe_reviews, review_mutation_operation::observe_viewer, review_mutation_operation::observe_pull_request, review_mutation_operation::create_review, review_mutation_operation::observe_reviews }, "viewer then exact head immediately precede POST followed by unconditional GET");
			const auto before    = wire.Trace.size();
			auto       duplicate = session.submit_and_reconcile(wire.Request);
			expect(duplicate.Receipt == result.Receipt && duplicate.Duplicate && wire.Trace.size() == before && wire.Posts == 1U, "durable success replays without another effect");
			auto changed          = wire.Request;
			changed.Proposal.Body = "changed";
			expect(session.submit_and_reconcile(changed).Publication == application::review_submission_publication::indeterminate && wire.Posts == 1U, "same-key changed content cannot submit again");
		}
	}
	void test_uncertainty_and_cancellation()
	{
		for (const auto mode : { 0, 1, 2, 3, 4, 5, 6, 7, 8 }) {
			temporary_directory root;
			transport           wire;
			std::stop_source    stop;
			auto                recovery = std::make_unique<failing_store>(storage(root.Path));
			auto*               fault    = recovery.get();
			if (mode == 0) {
				wire.LoseResponse = true;
			}
			if (mode == 1) {
				wire.MalformedResponse = true;
			}
			if (mode == 2) {
				wire.NewCount = 0U;
			}
			if (mode == 3) {
				wire.NewCount = 2U;
			}
			if (mode == 4) {
				fault->FailMark = true;
			}
			if (mode == 5) {
				fault->FailResolve = true;
			}
			if (mode == 6) {
				wire.CancelOnPost = &stop;
			}
			if (mode == 7) {
				wire.ThrowAfterSend = true;
			}
			if (mode == 8) {
				wire.DefinitelyUnsent = true;
			}
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), std::move(recovery) };
			const auto                result   = session.submit_and_reconcile(wire.Request, stop.get_token());
			const bool                resolved = mode == 0 || mode == 1 || mode == 6 || mode == 7;
			expect((result.Publication == application::review_submission_publication::published) == resolved, "post failures and cancellation resolve only through unique server truth");
			expect(wire.Posts == 1U && wire.Trace.back() == review_mutation_operation::observe_reviews, "every possible-send path attempts read reconciliation");
			fault->FailResolve = false;
			const auto retry   = session.submit_and_reconcile(wire.Request);
			expect(wire.Posts == 1U, "admitted attempts never repeat POST");
			if (mode == 8) {
				expect(result.Publication == application::review_submission_publication::definitely_not_published, "transport definite non-send is distinct from indeterminate");
			}
			if (mode == 5) {
				expect(retry.Publication == application::review_submission_publication::published, "failed durable resolution can recover later");
			}
			if (mode == 4) {
				expect(retry.Publication == application::review_submission_publication::indeterminate, "failed witness cannot adopt later review");
			}
		}
		for (const auto mode : { 0, 1, 2, 3, 4 }) {
			temporary_directory root;
			transport           wire;
			std::stop_source    stop;
			auto                authority = std::make_shared<lease>();
			if (mode == 0) {
				wire.WrongActor = true;
			}
			if (mode == 1) {
				wire.WrongHead = true;
			}
			if (mode == 2) {
				stop.request_stop();
			}
			if (mode == 3) {
				wire.OnTarget = [&] { authority->revoke(); };
			}
			if (mode == 4) {
				wire.Incomplete = true;
			}
			review_submission_session session{ wire, configuration(), authority, storage(root.Path) };
			expect(session.submit_and_reconcile(wire.Request, stop.get_token()).Publication == application::review_submission_publication::definitely_not_published && wire.Posts == 0U, "pre-send actor/head/cancellation/pagination failures publish nothing");
		}
	}
	void restart(std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		transport wire;
		auto      authority = std::make_shared<lease>();
		if (aPhase == "--write-unresolved") {
			wire.NewCount     = 0U;
			wire.LoseResponse = true;
		} else if (aPhase == "--write-prepared") {
			wire.OnTarget = [&] { authority->revoke(); };
		} else {
			wire.Reviews = { review(wire.Request) };
		}
		review_submission_session session{ wire, configuration(), authority, storage(aRoot) };
		const auto                result = session.submit_and_reconcile(wire.Request);
		if (aPhase == "--write-unresolved") {
			expect(result.Publication == application::review_submission_publication::indeterminate && wire.Posts == 1U, "first process durably unresolved");
		} else if (aPhase == "--recover-unresolved") {
			expect(result.Publication == application::review_submission_publication::published && wire.Posts == 0U, "fresh process resolves without POST");
		} else if (aPhase == "--write-prepared") {
			expect(wire.Posts == 0U, "prepared phase sends nothing");
		} else {
			expect(result.Publication == application::review_submission_publication::indeterminate && wire.Trace.empty(), "prepared restart cannot adopt unrelated later review");
		}
		std::cout << aPhase << ":pass\n";
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	static_assert(std::is_base_of_v<application::review_submission_port, bpr::adapters::github::live_review_submission_provider>);
	if (aArgc == 3) {
		restart(aArgv[1], aArgv[2]);
		return EXIT_SUCCESS;
	}
	test_dispositions_and_old_reviews();
	test_uncertainty_and_cancellation();
	std::cout << "live-review-submission-provider-tests:pass\n";
	return EXIT_SUCCESS;
}
