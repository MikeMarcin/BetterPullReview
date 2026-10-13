#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "live_review_submission_provider.hpp"
#include "review_submission_session.hpp"
#if defined(__APPLE__)
	#include "../support/strict_cli_fixture.hpp"
#endif
#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github::detail;
	using bpr::tests::expect;
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
	// [[BPR-0904]]. The fixture transport answers by the shape of the
	// request rather than by the operation the product says it is making:
	// method, path, and for a GraphQL root the operation name in the body. A
	// request no key names has no answer, and the row fails at that request,
	// naming it. Every consumed answer pins the request it answers.
	// The keys and the diagnostic follow the mutation suite's transport; the
	// answers stay the stateful simulated server this suite already had, so
	// every row still scripts by flag and the measurement is the fixture
	// author's intent.
	struct scripted_request
	{
		http_method Method = http_method::get;
		std::string Path;
		/// The GraphQL operation name, empty for a REST request.
		std::string Operation;

		auto operator==(const scripted_request&) const -> bool = default;
	};
	// The compiled GraphQL envelope is `{"query":"<kind> <Name>(...`; a body
	// of any other shape, including an empty one, names no operation.
	[[nodiscard]] auto graphql_operation_name(const std::string_view aBody) -> std::string
	{
		constexpr auto marker = std::string_view{ "{\"query\":\"" };
		if (!aBody.starts_with(marker)) {
			return { };
		}
		auto       document = aBody.substr(marker.size());
		const auto word     = [&document]() {
			auto length = std::size_t{ };
			while (length < document.size() && (std::isalnum(static_cast<unsigned char>(document[length])) != 0 || document[length] == '_')) {
				++length;
			}
			auto value = std::string{ document.substr(0, length) };
			document.remove_prefix(length);
			return value;
		};
		const auto kind = word();
		if (kind != "query" && kind != "mutation") {
			return { };
		}
		while (!document.empty() && document.front() == ' ') {
			document.remove_prefix(1);
		}
		return word();
	}
	[[nodiscard]] auto scripted_key(const review_mutation_wire_request& aWire) -> scripted_request
	{
		return { .Method = aWire.Method, .Path = aWire.Path, .Operation = graphql_operation_name(aWire.Body) };
	}
	[[nodiscard]] auto method_name(const http_method aMethod) -> std::string_view
	{
		return aMethod == http_method::post ? "POST" : "GET";
	}
	[[nodiscard]] auto operation_name(const review_mutation_operation aOperation) -> std::string_view
	{
		switch (aOperation) {
		case review_mutation_operation::staged_list_reviews:           return "staged_list_reviews";
		case review_mutation_operation::staged_read_review:            return "staged_read_review";
		case review_mutation_operation::staged_list_comments:          return "staged_list_comments";
		case review_mutation_operation::staged_create_pending:         return "staged_create_pending";
		case review_mutation_operation::staged_update_comment:         return "staged_update_comment";
		case review_mutation_operation::staged_update_summary:         return "staged_update_summary";
		case review_mutation_operation::staged_submit_comment:         return "staged_submit_comment";
		case review_mutation_operation::staged_discard_pending:        return "staged_discard_pending";
		case review_mutation_operation::staged_update_description:     return "staged_update_description";
		case review_mutation_operation::staged_list_issue_comments:    return "staged_list_issue_comments";
		case review_mutation_operation::staged_create_issue_comment:   return "staged_create_issue_comment";
		case review_mutation_operation::staged_update_issue_comment:   return "staged_update_issue_comment";
		case review_mutation_operation::staged_update_pending_comment: return "staged_update_pending_comment";

		case review_mutation_operation::observe_pull_request:           return "observe_pull_request";
		case review_mutation_operation::observe_viewer:                 return "observe_viewer";
		case review_mutation_operation::observe_review_comments:        return "observe_review_comments";
		case review_mutation_operation::create_review_comment:          return "create_review_comment";
		case review_mutation_operation::create_review:                  return "create_review";
		case review_mutation_operation::observe_reviews:                return "observe_reviews";
		case review_mutation_operation::reply_review_thread:            return "reply_review_thread";
		case review_mutation_operation::resolve_review_thread:          return "resolve_review_thread";
		case review_mutation_operation::unresolve_review_thread:        return "unresolve_review_thread";
		case review_mutation_operation::observe_review_thread:          return "observe_review_thread";
		case review_mutation_operation::observe_review_thread_comments: return "observe_review_thread_comments";
		}
		return "unknown";
	}
	[[nodiscard]] auto describe(const scripted_request& aRequest) -> std::string
	{
		auto text = std::string{ method_name(aRequest.Method) } + " " + aRequest.Path;
		if (!aRequest.Operation.empty()) {
			text += " " + aRequest.Operation;
		}
		return text;
	}
	// The requests the fixture answers, named by the server resource each
	// answer describes. The paths are GitHub's REST paths for the fixture
	// target, not a copy of the product's compiled path.
	[[nodiscard]] auto viewer_get() -> scripted_request
	{
		return { .Method = http_method::get, .Path = "/user" };
	}
	[[nodiscard]] auto pull_request_get() -> scripted_request
	{
		return { .Method = http_method::get, .Path = "/repos/MikeMarcin/BPR-Test/pulls/1" };
	}
	[[nodiscard]] auto reviews_get() -> scripted_request
	{
		return { .Method = http_method::get, .Path = "/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100" };
	}
	[[nodiscard]] auto review_post() -> scripted_request
	{
		return { .Method = http_method::post, .Path = "/repos/MikeMarcin/BPR-Test/pulls/1/reviews" };
	}
	struct transport final : github_mutation_wire_transport
	{
		application::review_submission_request Request = request();
		std::vector<std::string>               Reviews;
		std::vector<review_mutation_operation> Trace;
		std::size_t                            Requests        = 0U;
		std::size_t                            Posts           = 0U;
		std::size_t                            NewCount        = 1U;
		bool                                   WrongRepository = false, MovedContext = false;
		bool                                   DefinitelyUnsent = false, LoseResponse = false, MalformedResponse = false, ThrowAfterSend = false, WrongActor = false, WrongHead = false, Incomplete = false, WitnessRejected = false;
		std::stop_source*                      CancelOnPost = nullptr;
		/// [[BPR-1345]]. The pull request's review comments, review 45's
		/// among them, as its comment collection answers them, and a 422
		/// refusal of the next review.
		std::optional<std::string> Comments45;
		bool                       Refuse422 = false;
		std::function<void()>      OnTarget;
		std::function<void()>      OnPublished;
		auto                       perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			// [[BPR-1121]]. The session calls this and catches what it throws, so a
			// failure here, or in the hooks it calls, still ends the process rather
			// than unwinding into a typed transport failure ([[BPR-0904]]).
			const bpr::tests::exit_on_failure insideTheSession;
			expect(validate_outbound_review_mutation_request(aRequest.Wire).has_value(), "all session requests are canonical");
			++Requests;
			Trace.push_back(aRequest.Wire.Operation);
			const auto key = scripted_key(aRequest.Wire);
			if (key == viewer_get()) {
				auto response = viewer_response();
				if (WrongActor) {
					std::get<github_mutation_wire_success>(response).Response.Body = "{\"id\":23,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"}";
				}
				return response;
			}
			if (key == pull_request_get()) {
				if (OnTarget) {
					OnTarget();
				}
				auto response = target_response(WrongRepository ? 999U : target().RepositoryDatabaseId, WrongHead ? commit(0x22U) : target().Semantic.ExpectedHead);
				if (MovedContext) {
					auto& body = std::get<github_mutation_wire_success>(response).Response.Body;
					for (const auto& [from, to] : std::array<std::pair<std::string_view, std::string_view>, 4>{ { { "1010101010101010101010101010101010101010", "3333333333333333333333333333333333333333" },
																												  { "codex/disposable-live-review-20260810", "moved-head" },
																												  { "main", "moved-base" },
																												  { "open", "closed" } } }) {
						body.replace(body.find(from), from.size(), to);
					}
				}
				return response;
			}
			if (key == reviews_get()) {
				// Built by statement: GCC 15.2 misreads a header list temporary in a ?: arm as dangling (BPR-0816).
				auto headers = std::vector<http_header>{ };
				if (Incomplete) {
					headers.push_back(http_header{ .Name = "lInK", .Value = "next" });
				}
				return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = std::move(headers), .Body = list(Reviews) } };
			}
			if (Comments45 && key == scripted_request{ .Method = http_method::get, .Path = "/repos/MikeMarcin/BPR-Test/pulls/1/comments?per_page=100&page=1" }) {
				return github_mutation_wire_success{ .Response = { .Status = 200, .Body = *Comments45 } };
			}
			if (key == review_post()) {
				++Posts;
				if (DefinitelyUnsent) {
					return github_mutation_wire_error{ };
				}
				if (Refuse422) {
					expect(static_cast<bool>(aWitness) && aWitness(), "a refused POST was still witnessed first");
					return github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate, .HttpStatus = 422U };
				}
				expect(static_cast<bool>(aWitness), "POST requires durable witness");
				WitnessRejected = !aWitness();
				for (auto index = std::size_t{ }; index < NewCount; ++index) {
					Reviews.push_back(review(Request, 45U + index));
				}
				if (OnPublished) {
					OnPublished();
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
			}
			fail_unscripted(aRequest.Wire, key);
		}
		// The row fails here, at the request the product made, with the
		// requests the transport does answer beside it. It exits, as `expect`
		// does inside the transport; unwinding would be caught by the session
		// and reported as a typed transport failure, which is the silence
		// [[BPR-0904]] names.
		[[noreturn]] void fail_unscripted(const review_mutation_wire_request& aWire, const scripted_request& aKey) const
		{
			std::cerr << "FAILED: unscripted request #" << Requests << ": " << describe(aKey) << " (operation " << operation_name(aWire.Operation) << ", body " << (aWire.Body.empty() ? "empty" : "of " + std::to_string(aWire.Body.size()) + " bytes") << ")\n  this transport answers:";
			for (const auto& answered : { viewer_get(), pull_request_get(), reviews_get(), review_post() }) {
				std::cerr << "\n    " << describe(answered);
			}
			std::cerr << '\n';
			const bpr::tests::exit_on_failure insideTheSession;
			bpr::tests::fail();
		}
	};
	struct temporary_directory
	{
		std::filesystem::path Path = std::filesystem::temp_directory_path() / ("bpr-review-submission-" + std::to_string(std::random_device{ }()));
		temporary_directory()
		{
			expect(std::filesystem::create_directory(Path), "temporary root created");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Path };
			expect(adapters::sqlite::make_storage_root_owner_private(Path), "temporary root private");
			made.release();
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
		std::function<void()>                                         AfterAdmit, AfterMark, AfterResolve;
		explicit failing_store(std::unique_ptr<application::review_submission_recovery_port> aInner)
			: Inner{ std::move(aInner) }
		{ }
		auto pending() -> std::expected<std::vector<application::review_submission_recovery_record>, application::review_mutation_recovery_failure> override { return Inner->pending(); }
		auto load(const std::string& aKey) -> std::expected<std::optional<application::review_submission_recovery_record>, application::review_mutation_recovery_failure> override { return Inner->load(aKey); }
		auto admit(const application::review_submission_recovery_record& aRecord) -> std::expected<application::review_submission_recovery_admission, application::review_mutation_recovery_failure> override
		{
			auto result = Inner->admit(aRecord);
			if (result && AfterAdmit) {
				AfterAdmit();
			}
			return result;
		}
		auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<application::review_submission_recovery_record, application::review_mutation_recovery_failure> override
		{
			if (FailMark) {
				return std::unexpected(application::review_mutation_recovery_failure::unavailable);
			}
			auto result = Inner->mark_possibly_transmitted(aKey, aFingerprint);
			if (result && AfterMark) {
				AfterMark();
			}
			return result;
		}
		auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const application::review_submission_receipt& aReceipt) -> std::expected<application::review_submission_recovery_record, application::review_mutation_recovery_failure> override
		{
			if (FailResolve) {
				return std::unexpected(application::review_mutation_recovery_failure::unavailable);
			}
			auto result = Inner->resolve(aKey, aFingerprint, aReceipt);
			if (result && AfterResolve) {
				AfterResolve();
			}
			return result;
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
#if defined(__APPLE__)
	void test_strict_sqlite_recovery()
	{
		for (const bool confirmed : { false, true }) {
			temporary_directory root;
			transport           remote;
			remote.NewCount = confirmed ? 1U : 0U;
			std::stop_source          stop;
			tests::strict_cli_fixture process{ root.Path };
			process.Respond   = [&](const auto& aWire) { return remote.perform_review_mutation(aWire, { }, [] { return true; }); };
			process.Completed = [&](const auto& aWire) {
				if (aWire.Wire.Method == http_method::post) {
					process.available(false);
					stop.request_stop();
				}
			};
			const auto profile = tests::strict_cli_profile(credential_context());
			const auto base    = process.configuration();
			expect(profile && base, "strict fixture profile and actual child configuration are admitted");
			const auto pinned = pin_gh_cli_configuration(*base, "github.com", identity().Login);
			expect(pinned.has_value(), "strict fixture pins its exact selected login");
			gh_cli_transport        strict{ *profile, *pinned, process };
			tests::strict_cli_relay relay{ strict, process };
			{
				review_submission_session initial{ relay, configuration(), std::make_shared<lease>(), storage(root.Path) };
				const auto                result = initial.submit_and_reconcile(remote.Request, stop.get_token());
				std::cerr << "strict review: publication=" << static_cast<unsigned>(result.Publication) << " failure=" << static_cast<unsigned>(result.Failure) << " probes=" << process.Probes << " acquisitions=" << process.Acquisitions << " api=" << process.ApiCalls << " writes=" << process.Writes << '\n';
				expect(result.Publication == (confirmed ? application::review_submission_publication::published : application::review_submission_publication::indeterminate), "whole-review strict POST cancellation reconciles with the acquired credential despite removed lookup");
				expect(process.Acquisitions == 1U && process.Writes == 1U && remote.Posts == 1U, "whole review selects strict acquisition once through witness, POST and GET reconciliation");
			}
			expect(process.Credential.expired(), "whole review retains no credential after operation drain");
			const auto calls = process.ApiCalls;
			for (const auto failure : { github_cli_process_failure::nonzero_exit, github_cli_process_failure::cancelled,
										github_cli_process_failure::timeout, github_cli_process_failure::process_crash, github_cli_process_failure::reap_failure }) {
				process.AcquisitionFailure = failure == github_cli_process_failure::nonzero_exit ? std::nullopt : std::optional{ failure };
				review_submission_session restarted{ relay, configuration(), std::make_shared<lease>(), storage(root.Path) };
				const auto                result = restarted.submit_and_reconcile(remote.Request);
				expect(result.Request == remote.Request && result.Publication == (confirmed ? application::review_submission_publication::published : application::review_submission_publication::indeterminate)
						   && process.ApiCalls == calls && process.Writes == 1U,
					   "same-key SQLite publication truth outranks credential refusal/cancellation/tool errors and never repeats POST");
				const auto record = storage(root.Path)->load(remote.Request.IdempotencyKey);
				expect(record && *record && (**record).Request == remote.Request && (**record).Transmission == application::review_mutation_recovery_transmission::possibly_transmitted
						   && (**record).Receipt.has_value() == confirmed,
					   "exact SQLite draft and attempted/confirmed state survive reacquisition failure");
				const auto pending = restarted.pending();
				expect(pending && pending->size() == (confirmed ? 0U : 1U), "unknown attempts remain reconciliation obligations; confirmed receipts stay complete");
			}
			process.AcquisitionFailure.reset();
			process.available(true);
			process.Completed = { };
			remote.Reviews    = { review(remote.Request) };
			review_submission_session recovered{ relay, configuration(), std::make_shared<lease>(), storage(root.Path) };
			expect(recovered.reconcile(remote.Request).Publication == application::review_submission_publication::published && process.Writes == 1U, "restored strict lookup reconciles same-key unknown read-only without a second POST");
			process.verify_persistence();
		}
		for (const auto failure : { github_cli_process_failure::nonzero_exit, github_cli_process_failure::cancelled, github_cli_process_failure::timeout }) {
			temporary_directory       root;
			tests::strict_cli_fixture process{ root.Path };
			process.AcquisitionFailure = failure == github_cli_process_failure::nonzero_exit ? std::nullopt : std::optional{ failure };
			process.available(false);
			const auto profile = tests::strict_cli_profile(credential_context());
			const auto base    = process.configuration();
			expect(profile && base, "new review strict fixture is admitted");
			const auto pinned = pin_gh_cli_configuration(*base, "github.com", identity().Login);
			expect(pinned.has_value(), "new review strict fixture login is pinned");
			gh_cli_transport          strict{ *profile, *pinned, process };
			review_submission_session session{ strict, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                original = request();
			const auto                result   = session.submit_and_reconcile(original);
			const auto                expected = failure == github_cli_process_failure::nonzero_exit ? application::review_mutation_failure::credential_unavailable
				: failure == github_cli_process_failure::cancelled                                   ? application::review_mutation_failure::cancelled
																									 : application::review_mutation_failure::credential_tool_failed;
			expect(result.Request == original && result.Publication == application::review_submission_publication::definitely_not_published
					   && result.Failure == expected && process.ApiCalls == 0U && !*storage(root.Path)->load(original.IdempotencyKey),
				   "new whole-review refusal keeps the draft and typed cause before any API or journaled attempt");
		}
	}
#endif
	void test_historical_recovery()
	{
		for (const auto mode : { 0, 1, 2, 3 }) {
			temporary_directory root;
			transport           wire;
			wire.NewCount     = 0U;
			wire.LoseResponse = true;
			{
				review_submission_session initial{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
				expect(initial.submit_and_reconcile(wire.Request).Publication == application::review_submission_publication::indeterminate, "lost response retains original attempt");
			}
			wire.Reviews = { review(wire.Request) };
			wire.Trace.clear();
			wire.Posts           = 0U;
			wire.WrongHead       = true;
			wire.MovedContext    = true;
			wire.WrongActor      = mode == 2;
			wire.WrongRepository = mode == 3;
			auto current         = configuration();
			if (mode == 1) {
				current.Target.Semantic.ExpectedHead = commit(0x22U);
				current.Target.BaseOid               = commit(0x33U);
				current.Target.BaseRef               = "moved-base";
				current.Target.HeadRef               = "moved-head";
			}
			review_submission_session recovery{ wire, current, std::make_shared<lease>(), storage(root.Path) };
			const auto                pending = recovery.pending();
			expect(pending && *pending == std::vector{ wire.Request } && wire.Trace.empty(), "discovery preserves original request without network effects after revision movement");
			const auto result = recovery.reconcile(pending->front());
			expect(wire.Posts == 0U && result.Request == wire.Request, "historical recovery preserves exact request and cannot POST");
			expect((result.Publication == application::review_submission_publication::published) == (mode < 2), "only matching stable actor and PR may recover original-head review");
			if (mode < 2) {
				expect(result.Receipt && result.Receipt->Target == wire.Request.Proposal.Target, "receipt retains original head");
				expect(recovery.pending()->empty(), "resolved attempt leaves pending discovery");
			}
			if (mode == 1) {
				auto fresh           = wire.Request;
				fresh.IdempotencyKey = "new-key-at-old-head";
				expect(recovery.submit_and_reconcile(fresh).Publication == application::review_submission_publication::definitely_not_published && wire.Posts == 0U, "historical allowance never permits fresh old-head POST");
			}
		}
		temporary_directory root;
		auto                store    = storage(root.Path);
		const auto          original = request();
		const auto          prepared = application::review_submission_recovery_record{
			.Request              = original,
			.RequestFingerprint   = application::review_submission_request_fingerprint(original),
			.AuthorityFingerprint = application::review_submission_authority_fingerprint(*original.LiveBinding),
			.BaselineReviewIds    = std::vector<application::review_database_id>{ },
			.Transmission         = application::review_mutation_recovery_transmission::prepared
		};
		expect(store->admit(prepared).has_value(), "prepared discovery fixture admitted");
		transport wire;
		auto      current             = configuration();
		current.ActingIdentity.NodeId = "different-node";
		review_submission_session wrongActor{ wire, current, std::make_shared<lease>(), std::move(store) };
		expect(!wrongActor.pending() && wire.Trace.empty(), "discovery refuses another exact actor identity even within semantic scope");
		expect(wrongActor.reconcile(original).Publication == application::review_submission_publication::indeterminate && wire.Trace.empty(), "wrong recovery authority cannot claim definite nonpublication");
		temporary_directory       absentRoot;
		review_submission_session absent{ wire, configuration(), std::make_shared<lease>(), storage(absentRoot.Path) };
		expect(absent.reconcile(original).Publication == application::review_submission_publication::indeterminate && wire.Trace.empty(), "missing local attempt cannot prove nonpublication or issue a POST");
	}
	void write_fixture_file(const std::filesystem::path& aPath, std::string_view aText)
	{
		std::ofstream output{ aPath, std::ios::binary | std::ios::app };
		output << aText;
		output.close();
		expect(output.good(), "fixture file persisted before checkpoint");
	}
	void kill_checkpoint(const std::filesystem::path& aRoot, std::string_view aPhase)
	{
		// The parent keeps stdin open and kills this process here. Returning or
		// unwinding would make this an orderly-close test instead.
		write_fixture_file(aRoot / "kill-ready", aPhase);
		static_cast<void>(std::cin.get());
		expect(false, "kill checkpoint must never resume");
	}
	void kill_publication(std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		transport wire;
		wire.Reviews = { review(wire.Request, 40U) };
		auto store   = std::make_unique<failing_store>(storage(aRoot));
		auto pause   = [&] { kill_checkpoint(aRoot, aPhase); };
		if (aPhase == "--kill-prepared") {
			store->AfterAdmit = pause;
		} else if (aPhase == "--kill-witnessed") {
			store->AfterMark = pause;
		} else if (aPhase == "--kill-resolved") {
			store->AfterResolve = pause;
		} else {
			expect(aPhase == "--kill-published", "known kill course");
		}
		wire.OnPublished = [&] {
			expect(!wire.WitnessRejected, "server accepts only after successful durable witness");
			// This external fixture journal outlives the killed client. Recovery
			// derives server truth from it, not from the requested test outcome.
			write_fixture_file(aRoot / "server-reviews", "45\n");
			if (aPhase == "--kill-published") {
				pause();
			}
		};
		review_submission_session session{ wire, configuration(), std::make_shared<lease>(), std::move(store) };
		static_cast<void>(session.submit_and_reconcile(wire.Request));
		expect(false, "publication must reach its kill checkpoint");
	}
	void recover_killed_publication(std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		const auto prepared  = aPhase == "--recover-killed-prepared";
		const auto witnessed = aPhase == "--recover-killed-witnessed";
		const auto resolved  = aPhase == "--recover-killed-resolved";
		const auto moved     = aPhase == "--recover-killed-published-moved";
		expect(prepared || witnessed || resolved || moved || aPhase == "--recover-killed-published", "known killed recovery course");
		transport wire;
		wire.Reviews = { review(wire.Request, 40U) };
		std::ifstream server{ aRoot / "server-reviews" };
		std::string   serverId;
		while (std::getline(server, serverId)) {
			expect(serverId == "45", "fixed external fixture review identity");
			wire.Reviews.push_back(review(wire.Request));
		}
		expect(wire.Reviews.size() == (prepared || witnessed ? 1U : 2U), "external server journal agrees with actual publication boundary");
		wire.OnPublished = [&] { write_fixture_file(aRoot / "server-reviews", "45\n"); };
		auto store       = storage(aRoot);
		auto expected    = application::review_submission_recovery_record{
			.Request              = wire.Request,
			.RequestFingerprint   = application::review_submission_request_fingerprint(wire.Request),
			.AuthorityFingerprint = application::review_submission_authority_fingerprint(*wire.Request.LiveBinding),
			.BaselineReviewIds    = std::vector{ application::review_database_id{ 40U } },
			.Transmission         = prepared ? application::review_mutation_recovery_transmission::prepared : application::review_mutation_recovery_transmission::possibly_transmitted,
		};
		const auto receipt = application::review_submission_receipt{
			.ReviewDatabaseId = application::review_database_id{ 45U },
			.ReviewNodeId     = "PRR_45",
			.Target           = wire.Request.Proposal.Target,
			.ActingAccount    = wire.Request.Proposal.ActingAccount,
			.Disposition      = wire.Request.Proposal.Disposition,
		};
		if (resolved) {
			expected.State   = application::review_mutation_recovery_state::succeeded;
			expected.Receipt = receipt;
		}
		const auto loaded = store->load(wire.Request.IdempotencyKey);
		// [[BPR-1345]]. An attempt killed before its witness was never sent:
		// the first recovery says so and withdraws it, so a later one finds
		// no row, which alone proves nothing and sends nothing.
		if (prepared && loaded && !*loaded) {
			review_submission_session again{ wire, configuration(), std::make_shared<lease>(), std::move(store) };
			expect(again.pending() && again.pending()->empty() && wire.Trace.empty(), "a withdrawn attempt is not discovered again");
			const auto repeat = again.reconcile(wire.Request);
			expect(repeat.Publication == application::review_submission_publication::indeterminate && !repeat.Receipt && wire.Posts == 0U,
				   "with its row withdrawn, a later recovery claims nothing and sends nothing");
			std::cout << aPhase << ":pass\n";
			return;
		}
		expect(loaded && *loaded && **loaded == expected, "abrupt restart retains exact request, authority, baseline, witness and receipt");
		auto current = configuration();
		if (moved) {
			wire.WrongHead                       = true;
			wire.MovedContext                    = true;
			current.Target.Semantic.ExpectedHead = commit(0x22U);
			current.Target.BaseOid               = commit(0x33U);
			current.Target.BaseRef               = "moved-base";
			current.Target.HeadRef               = "moved-head";
		}
		review_submission_session session{ wire, current, std::make_shared<lease>(), std::move(store) };
		const auto                pending = session.pending();
		expect(pending && *pending == (resolved ? std::vector<application::review_submission_request>{ } : std::vector{ wire.Request }) && wire.Trace.empty(), "fresh discovery returns only original unresolved attempt without effects");
		const auto result      = session.reconcile(wire.Request);
		const auto publication = prepared ? application::review_submission_publication::definitely_not_published
			: witnessed                   ? application::review_submission_publication::indeterminate
										  : application::review_submission_publication::published;
		expect(result.Request == wire.Request && result.Publication == publication && wire.Posts == 0U, "kill recovery has exact outcome without POST");
		expect(result.Receipt == (prepared || witnessed ? std::optional<application::review_submission_receipt>{ } : receipt), "kill recovery cannot manufacture or retarget receipt");
		if (prepared) {
			const auto remaining = session.pending();
			expect(remaining && remaining->empty(), "an attempt never sent is withdrawn, not left pending");
			std::cout << aPhase << ":pass\n";
			return;
		}
		const auto retry = session.submit_and_reconcile(wire.Request);
		expect(retry.Request == result.Request && retry.Publication == result.Publication && retry.Receipt == result.Receipt && wire.Posts == 0U, "explicit original-attempt retry cannot duplicate publication");
		const auto remaining = session.pending();
		expect(remaining && *remaining == (witnessed ? std::vector{ wire.Request } : std::vector<application::review_submission_request>{ }), "only unresolved attempts stay pending after recovery");
		std::cout << aPhase << ":pass\n";
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
		auto current = configuration();
		if (aPhase == "--recover-moved") {
			wire.WrongHead                       = true;
			wire.MovedContext                    = true;
			current.Target.Semantic.ExpectedHead = commit(0x22U);
			current.Target.BaseOid               = commit(0x33U);
			current.Target.BaseRef               = "moved-base";
			current.Target.HeadRef               = "moved-head";
		}
		review_submission_session session{ wire, current, authority, storage(aRoot) };
		auto                      restored = wire.Request;
		if (aPhase == "--recover-prepared") {
			// [[BPR-1345]]. The first process withdrew the attempt it never
			// sent; nothing is discovered, and a recovery sends nothing.
			const auto pending = session.pending();
			expect(pending && pending->empty() && wire.Trace.empty(), "an attempt refused before its witness is withdrawn, not discovered");
		} else if (aPhase.starts_with("--recover-")) {
			const auto pending = session.pending();
			expect(pending && *pending == std::vector{ wire.Request } && wire.Trace.empty(), "fresh process discovers exact original request and retry key");
			restored = pending->front();
		}
		const auto result = aPhase.starts_with("--recover-") ? session.reconcile(restored) : session.submit_and_reconcile(restored);
		if (aPhase == "--write-unresolved") {
			expect(result.Publication == application::review_submission_publication::indeterminate && wire.Posts == 1U, "first process durably unresolved");
		} else if (aPhase == "--recover-unresolved" || aPhase == "--recover-moved") {
			expect(result.Publication == application::review_submission_publication::published && wire.Posts == 0U, "fresh process resolves without POST");
		} else if (aPhase == "--write-prepared") {
			expect(wire.Posts == 0U && result.Publication == application::review_submission_publication::definitely_not_published, "prepared phase sends nothing and says so");
		} else {
			expect(result.Publication == application::review_submission_publication::indeterminate && wire.Trace.empty(), "prepared restart cannot adopt unrelated later review");
		}
		std::cout << aPhase << ":pass\n";
	}
	/// [[BPR-1345]]. One review comment as GitHub lists it in review 45.
	[[nodiscard]] auto listed_comment(const std::uint64_t aId, const core::review_submission_comment& aComment) -> std::string
	{
		const auto side = std::string{ aComment.Side == core::diff_side::left ? "LEFT" : "RIGHT" };
		auto       text = std::string{ "{\"id\":" } + std::to_string(aId) + ",\"node_id\":\"PRRC_" + std::to_string(aId) + "\",\"html_url\":\"https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r" + std::to_string(aId)
			+ "\",\"pull_request_url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1\",\"body\":\"" + aComment.Body + "\",\"path\":\"" + aComment.Path
			+ "\",\"commit_id\":\"1111111111111111111111111111111111111111\",\"user\":{\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"},\"pull_request_review_id\":45,\"line\":"
			+ std::to_string(std::uint64_t{ aComment.Line } + 1U) + ",\"side\":\"" + side + "\",\"subject_type\":\"line\"";
		if (aComment.StartLine) {
			text += ",\"start_line\":" + std::to_string(std::uint64_t{ *aComment.StartLine } + 1U) + ",\"start_side\":\"" + side + "\"";
		}
		return text + "}";
	}
	/// [[BPR-1345]]. A review that carries pending drafts: published only when
	/// the one new review GitHub lists holds exactly those comments; a single
	/// comment's review, or another comment set, is not it; a 422 is a
	/// definite refusal and its attempt is withdrawn; what was admitted under
	/// a key is read without any request.
	void test_a_review_carries_its_comments_exactly()
	{
		auto first          = core::review_submission_comment{ };
		first.Draft         = core::review_draft_id{ 7U };
		first.DraftRevision = 3U;
		first.Path          = "src/engine/renderer.cpp";
		first.Line          = 430U;
		first.Body          = "question: Where does 0.01 come from?";
		auto range          = first;
		range.Draft         = core::review_draft_id{ 8U };
		range.Side          = core::diff_side::left;
		range.Line          = 12U;
		range.StartLine     = 10U;
		range.Body          = "nit: one line would do.";
		const auto carried  = [&] {
			auto value              = request();
			value.IdempotencyKey    = "review-with-comments";
			value.Proposal.Body     = { };
			value.Proposal.Comments = { first, range };
			return value;
		};
		{
			temporary_directory root;
			transport           wire;
			wire.Request    = carried();
			wire.Comments45 = "[" + listed_comment(901U, range) + "," + listed_comment(902U, first) + "]";
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			expect(session.admitted(wire.Request.IdempotencyKey) && !*session.admitted(wire.Request.IdempotencyKey), "nothing is admitted under an unused key");
			const auto result = session.submit_and_reconcile(wire.Request);
			expect(result.Publication == application::review_submission_publication::published && result.Receipt && result.Receipt->ReviewDatabaseId.value() == 45U && wire.Posts == 1U,
				   "the review is published when GitHub holds exactly its comments, in any order");
			expect(std::ranges::count(wire.Trace, review_mutation_operation::observe_review_comments) == 1, "its comments are read once, after the POST");
			const auto admitted = session.admitted(wire.Request.IdempotencyKey);
			expect(admitted && *admitted && **admitted == wire.Request, "what was admitted under its key is read back exactly");
		}
		for (const auto& listed : { std::string{ "[]" }, "[" + listed_comment(902U, first) + "]",
									"[" + listed_comment(901U, first) + "," + listed_comment(902U, first) + "]" }) {
			temporary_directory root;
			transport           wire;
			wire.Request    = carried();
			wire.Comments45 = listed;
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                result = session.submit_and_reconcile(wire.Request);
			expect(result.Publication == application::review_submission_publication::indeterminate && !result.Receipt && wire.Posts == 1U,
				   "a new review without exactly these comments (a single comment's review) is not this one");
			expect(session.pending() && session.pending()->size() == 1U, "and the attempt stays to be reconciled");
		}
		{
			temporary_directory root;
			transport           wire;
			wire.Request   = carried();
			wire.NewCount  = 0U;
			wire.Refuse422 = true;
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                result = session.submit_and_reconcile(wire.Request);
			expect(result.Publication == application::review_submission_publication::definitely_not_published && wire.Posts == 1U,
				   "GitHub's 422, with no new review of mine after it, is a definite refusal");
			expect(session.pending() && session.pending()->empty() && session.admitted(wire.Request.IdempotencyKey) && !*session.admitted(wire.Request.IdempotencyKey),
				   "and its attempt is withdrawn, not restored as unfinished");
		}
		{
			temporary_directory root;
			transport           wire;
			wire.Request     = carried();
			wire.Refuse422   = true;
			wire.NewCount    = 1U;
			wire.OnPublished = nullptr;
			// A 422 answer but a new exact review listed after it: not a refusal.
			wire.Reviews.push_back(review(wire.Request, 45U));
			wire.Comments45     = "[" + listed_comment(901U, first) + "," + listed_comment(902U, range) + "]";
			const auto baseline = wire.Reviews;
			wire.Reviews        = { };
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			wire.OnTarget     = [&] { wire.Reviews = baseline; };
			const auto result = session.submit_and_reconcile(wire.Request);
			expect(result.Publication != application::review_submission_publication::definitely_not_published, "a 422 is no refusal when a new exact review is listed after it");
		}
	}
	// [[BPR-1345]]. The live probe's two halves, run by hand against
	// responses a probe of MikeMarcin/BPR-Test saved to a scratch folder:
	// `--compile-live DIR` writes the exact review bodies BPR sends, and
	// `--replay-live DIR` runs the production session over GitHub's own
	// answers. Not registered as a row: it needs the probe's folder.
	struct live_probe
	{
		std::map<std::string, std::string> Values;
		std::filesystem::path              Folder;
		explicit live_probe(const std::filesystem::path& aFolder)
			: Folder{ aFolder }
		{
			std::ifstream input{ aFolder / "state.env" };
			for (std::string line; std::getline(input, line);) {
				const auto equals = line.find('=');
				if (equals != std::string::npos) {
					Values.emplace(line.substr(0U, equals), line.substr(equals + 1U));
				}
			}
			expect(Values.contains("head") && Values.contains("number"), "the probe's state is read");
		}
		[[nodiscard]] auto text(const char* aKey) const -> std::string { return Values.at(aKey); }
		[[nodiscard]] auto number(const char* aKey) const -> std::uint64_t { return std::stoull(Values.at(aKey)); }
		[[nodiscard]] auto file(const char* aName) const -> std::string
		{
			std::ifstream      input{ Folder / aName, std::ios::binary };
			std::ostringstream bytes;
			bytes << input.rdbuf();
			return bytes.str();
		}
		void write(const char* aName, const std::string& aBytes) const
		{
			std::ofstream output{ Folder / aName, std::ios::binary };
			output << aBytes;
		}
		[[nodiscard]] auto binding() const -> application::review_submission_live_binding
		{
			auto target                     = ::target();
			target.Semantic.ExpectedHead    = *core::parse_git_commit_oid(text("head"));
			target.Number                   = core::pull_request_number{ static_cast<std::uint32_t>(number("number")) };
			target.RepositoryDatabaseId     = number("repo_id");
			target.RepositoryNodeId         = text("repo_node");
			target.PullRequestDatabaseId    = number("pr_id");
			target.PullRequestNodeId        = text("pr_node");
			target.BaseRepositoryDatabaseId = number("repo_id");
			target.BaseOid                  = *core::parse_git_commit_oid(text("base"));
			target.BaseRef                  = text("base_ref");
			target.HeadRef                  = text("head_ref");
			auto actor                      = identity();
			actor.DatabaseId                = number("user_id");
			actor.NodeId                    = text("user_node");
			actor.Login                     = text("login");
			return { .Target = target, .ActingIdentity = actor };
		}
		[[nodiscard]] auto request(std::vector<core::review_submission_comment> aComments, std::string aKey) const -> application::review_submission_request
		{
			auto value              = ::request();
			value.IdempotencyKey    = std::move(aKey);
			value.LiveBinding       = binding();
			value.Proposal.Target   = value.LiveBinding->Target.Semantic;
			value.Proposal.Body     = { };
			value.Proposal.Comments = std::move(aComments);
			return value;
		}
		[[nodiscard]] auto store(const std::filesystem::path& aRoot) const -> std::unique_ptr<application::review_submission_recovery_port>
		{
			auto value = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aRoot, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = static_cast<std::int64_t>(number("number")) });
			expect(value.has_value(), "the probe's submission store opens");
			return std::move(*value);
		}
		[[nodiscard]] auto comments() const -> std::vector<core::review_submission_comment>
		{
			// Head line 10 changed, head lines 19 to 21 added, base line 5 removed.
			auto changed          = core::review_submission_comment{ };
			changed.Draft         = core::review_draft_id{ 1U };
			changed.DraftRevision = 2U;
			changed.Path          = text("path");
			changed.Line          = 9U;
			changed.Body          = "BPR-1345 live probe: one line on the head.";
			auto added            = changed;
			added.Draft           = core::review_draft_id{ 2U };
			added.Line            = 20U;
			added.StartLine       = 18U;
			added.Body            = "BPR-1345 live probe: a range of added lines.";
			auto removed          = changed;
			removed.Draft         = core::review_draft_id{ 3U };
			removed.Side          = core::diff_side::left;
			removed.Line          = 4U;
			removed.Body          = "BPR-1345 live probe: a removed base line.";
			return { changed, added, removed };
		}
		[[nodiscard]] auto refused() const -> std::vector<core::review_submission_comment>
		{
			auto outside = comments().front();
			outside.Line = 30U;
			outside.Body = "BPR-1345 live probe: a line outside the diff.";
			return { outside };
		}
	};
	void compile_live(const std::filesystem::path& aFolder)
	{
		const live_probe probe{ aFolder };
		const auto       body = [](const application::review_submission_request& aRequest) {
			auto input = review_submission_request{ .Target = aRequest.LiveBinding->Target, .ActingIdentity = aRequest.LiveBinding->ActingIdentity, .Disposition = aRequest.Proposal.Disposition, .Body = aRequest.Proposal.Body, .Comments = aRequest.Proposal.Comments };
			auto wire  = compile_review_submission_request(input);
			expect(wire.has_value(), "the probe's review compiles");
			return wire->Body;
		};
		probe.write("review-body.json", body(probe.request(probe.comments(), "live-probe-review")));
		probe.write("refused-body.json", body(probe.request(probe.refused(), "live-probe-refused")));
		std::cout << "compile-live:pass\n";
	}
	struct replay_transport final : github_mutation_wire_transport
	{
		const live_probe& Probe;
		std::string       Before, After, Posted, Comments;
		int               PostStatus = 200;
		bool              Sent       = false;
		std::string       SentBody;
		explicit replay_transport(const live_probe& aProbe)
			: Probe{ aProbe }
		{ }
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			const bpr::tests::exit_on_failure insideTheSession;
			const auto&                       wire = aRequest.Wire;
			const auto                        pull = "/repos/MikeMarcin/BPR-Test/pulls/" + Probe.text("number");
			const auto                        ok   = [](std::string aBody) { return github_mutation_wire_success{ .Response = { .Status = 200, .Body = std::move(aBody) } }; };
			if (wire.Method == http_method::get && wire.Path == "/user") {
				return ok("{\"login\":\"" + Probe.text("login") + "\",\"id\":" + Probe.text("user_id") + ",\"node_id\":\"" + Probe.text("user_node") + "\"}");
			}
			if (wire.Method == http_method::get && wire.Path == pull) {
				return ok(Probe.file("pull-observed.json"));
			}
			if (wire.Method == http_method::get && wire.Path == pull + "/reviews?per_page=100") {
				return ok(Sent ? After : Before);
			}
			if (wire.Method == http_method::get && wire.Path == pull + "/comments?per_page=100&page=1") {
				return ok(Comments);
			}
			if (wire.Method == http_method::post && wire.Path == pull + "/reviews") {
				expect(aWitness && aWitness(), "the replayed POST is witnessed first");
				Sent     = true;
				SentBody = wire.Body;
				if (PostStatus != 200) {
					return github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate, .HttpStatus = static_cast<std::uint16_t>(PostStatus) };
				}
				return ok(Posted);
			}
			std::cerr << "FAILED: unreplayed request " << wire.Path << '\n';
			bpr::tests::fail();
		}
	};
	void replay_live(const std::filesystem::path& aFolder)
	{
		const live_probe probe{ aFolder };
		{
			temporary_directory root;
			replay_transport    wire{ probe };
			wire.Before                                       = probe.file("reviews-before.json");
			wire.After                                        = probe.file("reviews-after.json");
			wire.Posted                                       = probe.file("review-post.json");
			wire.Comments                                     = probe.file("pr-comments.json");
			auto configuration                                = ::configuration();
			configuration.Target                              = probe.binding().Target;
			configuration.ActingIdentity                      = probe.binding().ActingIdentity;
			configuration.CredentialContext.Authority.Account = configuration.ActingIdentity.Account;
			review_submission_session session{ wire, configuration, std::make_shared<lease>(), probe.store(root.Path) };
			const auto                request = probe.request(probe.comments(), "live-probe-review");
			const auto                result  = session.submit_and_reconcile(request);
			if (wire.SentBody != probe.file("review-body.json")) {
				std::cerr << "sent: " << wire.SentBody << "\nlive: " << probe.file("review-body.json") << '\n';
			}
			expect(wire.SentBody == probe.file("review-body.json"), "the body replayed is the body the probe sent live");
			expect(result.Publication == application::review_submission_publication::published && result.Receipt
					   && result.Receipt->ReviewDatabaseId.value() == probe.number("review_id"),
				   "GitHub's own review list and comments resolve the review with comments, exactly");
		}
		{
			temporary_directory root;
			replay_transport    wire{ probe };
			wire.Before                  = probe.file("reviews-after.json");
			wire.After                   = probe.file("reviews-after-refusal.json");
			wire.PostStatus              = std::stoi(probe.file("refused-post.status"));
			auto configuration           = ::configuration();
			configuration.Target         = probe.binding().Target;
			configuration.ActingIdentity = probe.binding().ActingIdentity;
			review_submission_session session{ wire, configuration, std::make_shared<lease>(), probe.store(root.Path) };
			const auto                request = probe.request(probe.refused(), "live-probe-refused");
			const auto                result  = session.submit_and_reconcile(request);
			expect(wire.SentBody == probe.file("refused-body.json"), "the refused body replayed is the one sent live");
			expect(wire.PostStatus == 422 && result.Publication == application::review_submission_publication::definitely_not_published
					   && session.pending() && session.pending()->empty(),
				   "GitHub's 422 and its unchanged review list are a definite refusal, withdrawn");
		}
		std::cout << "replay-live:pass\n";
	}
	struct row
	{
		std::string_view Name;
		void (*Run)();
	};
	// [[BPR-0904]]. Rows by name, so one can run alone: a row fails by
	// exiting, and a measurement of which rows fail needs each in isolation.
	constexpr auto rows = std::array{
		row{ "dispositions_and_old_reviews", &test_dispositions_and_old_reviews },
		row{ "uncertainty_and_cancellation", &test_uncertainty_and_cancellation },
		row{ "historical_recovery", &test_historical_recovery },
		row{ "review_carries_its_comments_exactly", &test_a_review_carries_its_comments_exactly },
	};
}
auto main(int aArgc, char** aArgv) -> int
try {
	QCoreApplication application{ aArgc, aArgv };
#if defined(__APPLE__)
	if (std::filesystem::path{ aArgv[0] }.filename() == "gh") {
		return tests::strict_cli_child(QCoreApplication::arguments());
	}
	// macOS refuses nested sandbox initialization. Like gh_cli_git_smart_transport,
	// this explicit selector runs only the closed same-executable local fixture.
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--strict-cli-fixture") {
		test_strict_sqlite_recovery();
		std::cout << "strict-sqlite-recovery:pass\n";
		return EXIT_SUCCESS;
	}
#endif
	static_assert(std::is_base_of_v<application::review_submission_port, bpr::adapters::github::live_review_submission_provider>);
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--list") {
		for (const auto& selected : rows) {
			std::cout << selected.Name << '\n';
		}
		return EXIT_SUCCESS;
	}
	if (aArgc == 3 && std::string_view{ aArgv[1] } == "--only") {
		const auto selected = std::ranges::find(rows, std::string_view{ aArgv[2] }, &row::Name);
		if (selected == rows.end()) {
			std::cerr << "FAILED: no row named " << aArgv[2] << '\n';
			return EXIT_FAILURE;
		}
		selected->Run();
		std::cout << selected->Name << ":pass\n";
		return EXIT_SUCCESS;
	}
	if (aArgc == 3) {
		const auto phase = std::string_view{ aArgv[1] };
		if (phase == "--compile-live") {
			compile_live(aArgv[2]);
		} else if (phase == "--replay-live") {
			replay_live(aArgv[2]);
		} else if (phase.starts_with("--kill-")) {
			kill_publication(phase, aArgv[2]);
		} else if (phase.starts_with("--recover-killed-")) {
			recover_killed_publication(phase, aArgv[2]);
		} else {
			restart(phase, aArgv[2]);
		}
		return EXIT_SUCCESS;
	}
	for (const auto& selected : rows) {
		selected.Run();
	}
	std::cout << "live-review-submission-provider-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
