#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "github_review_list_shapes.hpp"
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
#include <set>
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
	namespace github_shapes = bpr::tests::github_shapes;
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
		case review_mutation_operation::observe_merge_base:            return "observe_merge_base";
		case review_mutation_operation::observe_recorded_merge_base:   return "observe_recorded_merge_base";
		case review_mutation_operation::observe_base_ancestry:         return "observe_base_ancestry";

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
	/// [[BPR-1572]]. A stacked pull request's base branch naming another
	/// merge base: it only moved forward from the recorded base (GitHub still
	/// numbers LEFT in the recorded base's merge base), or it was rewritten.
	enum class stacked_course : std::uint8_t { none,
											   forward,
											   rewritten,
											   count_differs,
											   reset };
	/// [[BPR-1560]]. GitHub's compare for the target's base branch and head:
	/// the merge base a LEFT line is numbered in, the target's own unless the
	/// base branch moved. [[BPR-1572]]: and the recorded base's compares.
	[[nodiscard]] auto merge_base_answer(const review_mutation_wire_request& aWire, const bool aMoved, const stacked_course aStacked = stacked_course::none)
		-> std::optional<github_mutation_wire_completion>
	{
		const auto head       = std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() };
		const auto recorded   = std::string{ core::canonical_hex(target().BaseOid).view() };
		const auto comparison = std::string{ core::canonical_hex(application::comparison_base(target())).view() };
		// The base branch's tip, which the ancestry pair is bound to.
		const auto tip       = std::string(40U, 'b');
		auto       url       = std::string{ "main..." } + head;
		auto       base      = tip;
		auto       mergeBase = aMoved || aStacked != stacked_course::none ? std::string(40U, 'c') : comparison;
		auto       ahead     = std::string{ };
		if (aWire.Operation == review_mutation_operation::observe_recorded_merge_base) {
			url       = recorded + "..." + head;
			base      = recorded;
			mergeBase = aMoved ? std::string(40U, 'c') : comparison;
			// The pull request reports two commits on the stacked courses.
			ahead = aStacked == stacked_course::count_differs ? R"("ahead_by":3,)" : R"("ahead_by":2,)";
		} else if (aWire.Operation == review_mutation_operation::observe_base_ancestry) {
			url       = recorded + "..." + tip;
			base      = recorded;
			mergeBase = aStacked == stacked_course::rewritten || aMoved ? std::string(40U, 'e') : recorded;
		} else if (aWire.Operation != review_mutation_operation::observe_merge_base) {
			return std::nullopt;
		}
		return github_mutation_wire_success{ .Response = { .Status = 200, .Body = "{\"url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/compare/" + url + "\"," + ahead + "\"base_commit\":{\"sha\":\"" + base + "\"},\"merge_base_commit\":{\"sha\":\"" + mergeBase + "\"}}" } };
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
		bool                                   MovedMergeBase   = false;
		stacked_course                         Stacked          = stacked_course::none;
		std::size_t                            TargetReads      = 0U;
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
			if (auto answer = merge_base_answer(aRequest.Wire, MovedMergeBase, Stacked)) {
				return *answer;
			}
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
				// [[BPR-1572]]. The stacked courses' pull request reports two
				// commits; on the reset course GitHub has replaced its recorded
				// base with the base branch's tip by the read that follows the
				// merge-base answers.
				if (Stacked != stacked_course::none) {
					auto& body = std::get<github_mutation_wire_success>(response).Response.Body;
					body.insert(body.find(R"("head")"), R"("commits":2,)");
					if (Stacked == stacked_course::reset && ++TargetReads == 2U) {
						const auto recorded = std::string{ core::canonical_hex(target().BaseOid).view() };
						body.replace(body.find(recorded), recorded.size(), std::string(40U, 'b'));
					}
				}
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
		// [[BPR-1540]]. A recovery run before this one may have recorded why
		// the witnessed attempt is still unknown: GitHub lists no review.
		auto       retained = loaded ? *loaded : std::nullopt;
		const auto why      = retained ? retained->LastUnknown : std::nullopt;
		if (retained) {
			retained->LastUnknown.reset();
		}
		expect(retained && *retained == expected, "abrupt restart retains exact request, authority, baseline, witness and receipt");
		expect(!why || (witnessed && why->Reason == core::review_submission_unknown_reason::review_not_listed), "only an unknown witnessed attempt keeps a reason, and it names the review not listed");
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
			// [[BPR-1560]]. A review with a LEFT comment asks GitHub's merge base
			// just before the POST; when the base branch moved it since the
			// review was written, nothing is sent.
			temporary_directory root;
			transport           wire;
			wire.Request        = carried();
			wire.MovedMergeBase = true;
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                result = session.submit_and_reconcile(wire.Request);
			expect(result.Publication == application::review_submission_publication::definitely_not_published && wire.Posts == 0U
					   && result.Failure == application::review_mutation_failure::target_moved
					   && std::ranges::count(wire.Trace, review_mutation_operation::observe_merge_base) == 1,
				   "a moved merge base refuses a review with a LEFT comment before its POST");
		}
		for (const auto stacked : { stacked_course::forward, stacked_course::rewritten, stacked_course::count_differs, stacked_course::reset }) {
			// [[BPR-1572]]. The base branch names another merge base. A base
			// branch that only moved forward from the recorded base leaves
			// GitHub numbering LEFT in the recorded base's merge base, so the
			// review is published. A rewritten branch, a commit count other than
			// the recorded base's, or GitHub replacing the recorded base with the
			// branch's tip while the answers were asked (the pull request read
			// again names the tip) refuses it before the POST.
			temporary_directory root;
			transport           wire;
			wire.Request    = carried();
			wire.Stacked    = stacked;
			wire.Comments45 = "[" + listed_comment(901U, range) + "," + listed_comment(902U, first) + "]";
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                result = session.submit_and_reconcile(wire.Request);
			const auto                asked  = std::ranges::count(wire.Trace, review_mutation_operation::observe_merge_base) == 1
				&& std::ranges::count(wire.Trace, review_mutation_operation::observe_recorded_merge_base) == 1
				&& std::ranges::count(wire.Trace, review_mutation_operation::observe_base_ancestry) == 1
				&& std::ranges::count(wire.Trace, review_mutation_operation::observe_pull_request) == 2;
			if (stacked == stacked_course::forward) {
				expect(asked && wire.Posts == 1U && result.Publication == application::review_submission_publication::published,
					   "a base branch that only moved forward keeps a LEFT comment's lines and the review is sent");
			} else {
				expect(asked && wire.Posts == 0U && result.Publication == application::review_submission_publication::definitely_not_published
						   && result.Failure == application::review_mutation_failure::target_moved,
					   "a rewritten base branch, a differing commit count or a replaced recorded base refuses a review with a LEFT comment before its POST");
			}
		}
		{
			// Without a LEFT line nothing asks for the merge base.
			temporary_directory root;
			transport           wire;
			wire.Request                   = carried();
			wire.Request.Proposal.Comments = { first };
			wire.Comments45                = "[" + listed_comment(901U, first) + "]";
			review_submission_session session{ wire, configuration(), std::make_shared<lease>(), storage(root.Path) };
			static_cast<void>(session.submit_and_reconcile(wire.Request));
			expect(std::ranges::count(wire.Trace, review_mutation_operation::observe_merge_base) == 0, "a review with RIGHT lines only does not ask for the merge base");
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
			// The workflow states a refusal by its failure since BPR-1429;
			// `unsupported` read "This connection does not support this operation".
			expect(result.Failure == application::review_mutation_failure::refused_by_server,
				   "and the server refused it, not an operation the connection lacks");
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
	// [[BPR-1538]]. A pull request with many review comments, as GitHub and a
	// GitHub Enterprise Server list them: complete objects of about 3.5 KB
	// and 62 JSON values each, 100 to a page, the next page named by the
	// repository's number. The owner's approval with comments landed on a real
	// pull request and stayed "Review status is unknown" on every Check
	// status: the session could not read a page of the comment collection, so
	// it could never confirm that the new review held exactly its comments.
	struct busy_server final : github_mutation_wire_transport
	{
		github_shapes::pull                    Pull;
		std::string                            Prefix;
		application::review_submission_request Request;
		github_shapes::user                    Me{ .Id = 22U, .NodeId = "MDQ6VXNlcjIy", .Login = "fixture-reviewer" };
		github_shapes::user                    Other{ .Id = 23U, .NodeId = "MDQ6VXNlcjIz", .Login = "another-reviewer" };
		std::vector<github_shapes::review>     Reviews;
		std::vector<github_shapes::comment>    Comments;
		std::vector<github_shapes::review>     Landed;
		std::vector<github_shapes::comment>    LandedComments;
		std::size_t                            Posts = 0U, Pages = 0U;
		/// The POST creates the review; GitHub answers it with a gateway
		/// timeout, as it answered a 50-comment review on #70.
		bool Lands = true, Answer504 = true;
		/// Whether the review it created is listed yet.
		bool Visible = true;
		/// [[BPR-1538]]. A push after the review: the head moves, one comment
		/// moves five lines down and one is left outdated.
		bool Pushed = false;
		/// The landed review holds one comment with other words.
		bool AlterOne = false;
		/// [[BPR-1540]]. The POST lands two identical reviews; the server names
		/// no original position; the account or pull request read back is
		/// another; no operation can begin.
		bool Twice = false, NoOriginal = false, OtherViewer = false, OtherPull = false, RefuseOperation = false;
		/// [[BPR-1540]]. Answers a request before the server does, when it
		/// returns one.
		std::function<std::optional<github_mutation_wire_completion>(const review_mutation_wire_request&)> Answer;
		auto                                                                                               begin_mutation_operation(std::stop_token aStop) -> github_mutation_operation override
		{
			if (RefuseOperation) {
				return std::unexpected{ application::review_mutation_failure::credential_unavailable };
			}
			return github_mutation_wire_transport::begin_mutation_operation(aStop);
		}

		[[nodiscard]] auto head() const -> std::string { return std::string{ core::canonical_hex(Request.Proposal.Target.ExpectedHead).view() }; }
		[[nodiscard]] auto moved() const -> std::string { return std::string{ core::canonical_hex(commit(0x33U)).view() }; }
		[[nodiscard]] auto listed_comments() const -> std::vector<github_shapes::comment>
		{
			auto all = Comments;
			if (Visible) {
				for (auto value : LandedComments) {
					value.OmitOriginal = NoOriginal;
					if (Pushed) {
						value.Commit = moved();
						if (value.Side == "LEFT") {
							value.Line.reset();
						} else {
							value.Line = *value.Line + 5U;
							if (value.StartLine) {
								value.StartLine = *value.StartLine + 5U;
							}
						}
					}
					all.push_back(value);
				}
			}
			return all;
		}
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			if (auto answer = merge_base_answer(aRequest.Wire, false)) {
				return *answer;
			}
			const bpr::tests::exit_on_failure insideTheSession;
			expect(validate_outbound_review_mutation_request(aRequest.Wire).has_value(), "all session requests are canonical");
			const auto& wire    = aRequest.Wire;
			const auto  pull    = Prefix + "/repos/MikeMarcin/BPR-Test/pulls/1";
			const auto  ok      = [](std::string aBody, std::vector<http_header> aHeaders = { }) { return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = std::move(aHeaders), .Body = std::move(aBody) } }; };
			const auto  pageKey = pull + "/comments?per_page=100&page=";
			if (Answer) {
				if (auto answered = Answer(wire)) {
					return std::move(*answered);
				}
			}
			if (wire.Method == http_method::get && wire.Path == Prefix + "/user") {
				return OtherViewer ? ok("{\"login\":\"fixture-reviewer\",\"id\":23,\"node_id\":\"MDQ6VXNlcjIy\"}") : viewer_response();
			}
			if (wire.Method == http_method::get && wire.Path == pull) {
				return target_response(OtherPull ? 999U : target().RepositoryDatabaseId, Pushed ? commit(0x33U) : Request.Proposal.Target.ExpectedHead);
			}
			if (wire.Method == http_method::get && wire.Path == pull + "/reviews?per_page=100") {
				auto all = Reviews;
				if (Visible) {
					all.insert(all.end(), Landed.begin(), Landed.end());
				}
				auto text = std::vector<std::string>{ };
				for (const auto& value : all) {
					text.push_back(github_shapes::review_json(Pull, value));
				}
				return ok(list(text));
			}
			if (wire.Method == http_method::get && wire.Path.starts_with(pageKey)) {
				++Pages;
				const auto page = static_cast<std::uint32_t>(std::stoul(wire.Path.substr(pageKey.size())));
				const auto all  = listed_comments();
				const auto last = std::max<std::size_t>(1U, (all.size() + 99U) / 100U);
				auto       text = std::vector<std::string>{ };
				for (auto index = (page - 1U) * 100U; index < std::min<std::size_t>(all.size(), page * 100U); ++index) {
					text.push_back(github_shapes::comment_json(Pull, all[index]));
				}
				auto links = std::vector<std::string>{ };
				if (page > 1U) {
					links.push_back("<" + Pull.comments_page(page - 1U) + ">; rel=\"prev\"");
				}
				if (page < last) {
					links.push_back("<" + Pull.comments_page(page + 1U) + ">; rel=\"next\"");
					links.push_back("<" + Pull.comments_page(static_cast<std::uint32_t>(last)) + ">; rel=\"last\"");
				}
				if (page > 1U) {
					links.push_back("<" + Pull.comments_page(1U) + ">; rel=\"first\"");
				}
				auto headers = std::vector<http_header>{ };
				if (!links.empty()) {
					auto value = links.front();
					for (auto index = std::size_t{ 1U }; index < links.size(); ++index) {
						value += ", " + links[index];
					}
					headers.push_back({ .Name = "Link", .Value = std::move(value) });
				}
				return ok(list(text), std::move(headers));
			}
			if (wire.Method == http_method::post && wire.Path == pull + "/reviews") {
				expect(static_cast<bool>(aWitness) && aWitness(), "the POST is witnessed first");
				++Posts;
				if (Lands) {
					Landed.push_back({ .Id = 900U, .Author = Me, .Body = Request.Proposal.Body, .State = "APPROVED", .Commit = head() });
					if (Twice) {
						Landed.push_back({ .Id = 901U, .Author = Me, .Body = Request.Proposal.Body, .State = "APPROVED", .Commit = head() });
					}
					auto id = std::uint64_t{ 9'001U };
					for (const auto& sent : Request.Proposal.Comments) {
						auto value              = github_shapes::comment{ };
						value.Id                = id++;
						value.Review            = 900U;
						value.Path              = sent.Path;
						value.Body              = sent.Body;
						value.Author            = Me;
						value.Commit            = head();
						value.OriginalCommit    = head();
						value.Line              = sent.Line + 1U;
						value.OriginalLine      = sent.Line + 1U;
						value.StartLine         = sent.StartLine ? std::optional{ *sent.StartLine + 1U } : std::nullopt;
						value.OriginalStartLine = value.StartLine;
						value.Side              = sent.Side == core::diff_side::left ? "LEFT" : "RIGHT";
						LandedComments.push_back(value);
					}
					if (AlterOne) {
						LandedComments.back().Body += " (edited)";
					}
				}
				if (Answer504) {
					return github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate, .HttpStatus = 504U };
				}
				return ok(github_shapes::review_json(Pull, Landed.back()));
			}
			std::cerr << "FAILED: unscripted request " << wire.Path << '\n';
			bpr::tests::fail();
		}
	};
	struct busy_case
	{
		std::string_view                            Name;
		application::review_mutation_target_binding Target;
		github_shapes::server                       Server;
		std::string                                 Prefix;
	};
	[[nodiscard]] auto busy_cases() -> std::vector<busy_case>
	{
		auto enterprise           = target();
		enterprise.Hostname       = application::github_canonical_hostname{ "ghe.example.invalid" };
		enterprise.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		return { { .Name = "github.com", .Target = target(), .Server = github_shapes::github_com(), .Prefix = "" },
				 { .Name = "GitHub Enterprise Server", .Target = enterprise, .Server = github_shapes::enterprise("ghe.example.invalid"), .Prefix = "/api/v3" } };
	}
	[[nodiscard]] auto busy_configuration(const busy_case& aCase, const core::git_commit_oid& aHead) -> live_review_mutation_configuration
	{
		auto value                                       = configuration();
		value.Target                                     = aCase.Target;
		value.Target.Semantic.ExpectedHead               = aHead;
		value.CredentialContext.Authority.Hostname       = aCase.Target.Hostname;
		value.CredentialContext.Authority.ServerContract = aCase.Target.ServerContract;
		return value;
	}
	[[nodiscard]] auto busy_server_for(const busy_case& aCase) -> std::unique_ptr<busy_server>
	{
		auto server                = std::make_unique<busy_server>();
		server->Pull               = { .Server = aCase.Server, .Owner = "MikeMarcin", .Name = "BPR-Test", .RepositoryId = target().RepositoryDatabaseId, .Number = 1U };
		server->Prefix             = aCase.Prefix;
		auto changed               = core::review_submission_comment{ };
		changed.Draft              = core::review_draft_id{ 7U };
		changed.DraftRevision      = 2U;
		changed.Path               = "src/many.txt";
		changed.Line               = 149U;
		changed.Body               = "question: Is this the bound we want?";
		auto range                 = changed;
		range.Draft                = core::review_draft_id{ 8U };
		range.Line                 = 162U;
		range.StartLine            = 160U;
		range.Body                 = "nit: these three lines could be one.";
		auto removed               = changed;
		removed.Draft              = core::review_draft_id{ 9U };
		removed.Path               = "src/base.txt";
		removed.Side               = core::diff_side::left;
		removed.Line               = 4U;
		removed.Body               = "praise: good riddance.";
		auto value                 = request();
		value.IdempotencyKey       = "busy-approval";
		value.LiveBinding          = application::review_submission_live_binding{ .Target = aCase.Target, .ActingIdentity = identity() };
		value.Proposal.Target      = aCase.Target.Semantic;
		value.Proposal.Body        = { };
		value.Proposal.Disposition = core::review_disposition::approve;
		value.Proposal.Comments    = { changed, range, removed };
		server->Request            = value;
		// Two earlier reviews by another reviewer: 130 comments and a reply,
		// so the collection is two pages and the first is about 450 KB.
		for (auto review = std::uint64_t{ 500U }; review < 502U; ++review) {
			server->Reviews.push_back({ .Id = review, .Author = server->Other, .Body = "An earlier review.", .Commit = server->head() });
		}
		for (auto index = std::uint32_t{ 0U }; index < 130U; ++index) {
			auto noise           = github_shapes::comment{ };
			noise.Id             = 1'000U + index;
			noise.Review         = index < 65U ? 500U : 501U;
			noise.Path           = "src/many.txt";
			noise.Body           = "An earlier comment on this line, long enough to look like a real one.";
			noise.Author         = server->Other;
			noise.Commit         = server->head();
			noise.OriginalCommit = server->head();
			noise.Line           = index + 1U;
			noise.OriginalLine   = index + 1U;
			if (index == 129U) {
				noise.InReplyTo = 1'000U;
			}
			server->Comments.push_back(noise);
		}
		return server;
	}
	[[nodiscard]] auto busy_storage(const busy_case& aCase, const std::filesystem::path& aRoot) -> std::unique_ptr<application::review_submission_recovery_port>
	{
		auto value = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aRoot, .Host = std::string{ aCase.Target.Hostname.value() }, .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(value.has_value(), "submission store opens");
		return std::move(*value);
	}
	void test_a_review_with_comments_reconciles_on_a_busy_pull_request()
	{
		using publication = application::review_submission_publication;
		for (const auto& selected : busy_cases()) {
			const auto label = std::string{ selected.Name } + ": ";
			{
				// The page is as large as GitHub's: the fixture would not
				// show the bound otherwise.
				const auto  server = busy_server_for(selected);
				const auto  page   = server->perform_review_mutation({ .Context = configuration().CredentialContext, .Wire = *compile_review_comment_collection_request(selected.Target, selected.Target.ServerContract, 1U) }, { }, { });
				const auto& body   = std::get<github_mutation_wire_success>(page).Response.Body;
				expect(body.size() > maximum_review_mutation_response_bytes, label + "a page of 100 real comments is larger than the ordinary response bound");
			}
			{
				// Approve with three comments; GitHub creates it and answers
				// the POST with a gateway timeout.
				temporary_directory       root;
				const auto                server = busy_server_for(selected);
				review_submission_session session{ *server, busy_configuration(selected, server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(selected, root.Path) };
				const auto                result = session.submit_and_reconcile(server->Request);
				expect(result.Publication == publication::published && result.Receipt && result.Receipt->ReviewDatabaseId.value() == 900U && server->Posts == 1U && server->Pages == 2U,
					   label + "the approval GitHub holds with exactly its comments is confirmed, reading both pages of the collection");
				expect(session.pending() && session.pending()->empty(), label + "and nothing is left to reconcile");
			}
			{
				// Not yet listed when the answer is read; then the author
				// pushes. Check status in a later session finds it.
				temporary_directory root;
				const auto          server = busy_server_for(selected);
				server->Visible            = false;
				{
					review_submission_session session{ *server, busy_configuration(selected, server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(selected, root.Path) };
					const auto                result = session.submit_and_reconcile(server->Request);
					expect(result.Publication == publication::indeterminate && server->Posts == 1U, label + "a review not yet listed after a timeout is unknown");
				}
				server->Visible = true;
				server->Pushed  = true;
				review_submission_session later{ *server, busy_configuration(selected, commit(0x33U)), std::make_shared<lease>(), busy_storage(selected, root.Path) };
				const auto                pending = later.pending();
				expect(pending && pending->size() == 1U && pending->front() == server->Request, label + "the unknown attempt is restored as it was sent");
				const auto result = later.reconcile(server->Request);
				expect(result.Publication == publication::published && result.Receipt && result.Receipt->ReviewDatabaseId.value() == 900U && server->Posts == 1U,
					   label + "Check status confirms it by where its comments were written, after a push moved one and outdated another");
				expect(later.pending() && later.pending()->empty(), label + "and the confirmed attempt is no longer restored");
			}
			{
				// The POST never landed: Check status stays unknown and sends
				// nothing.
				temporary_directory root;
				const auto          server = busy_server_for(selected);
				server->Lands              = false;
				review_submission_session session{ *server, busy_configuration(selected, server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(selected, root.Path) };
				expect(session.submit_and_reconcile(server->Request).Publication == publication::indeterminate, label + "a timeout with no review is unknown");
				expect(session.reconcile(server->Request).Publication == publication::indeterminate && server->Posts == 1U, label + "and Check status keeps it unknown without sending again");
				expect(session.pending() && session.pending()->size() == 1U, label + "and the attempt is kept");
			}
			{
				// A review of mine whose comments are not exactly these is not
				// this one, on a busy pull request too.
				temporary_directory root;
				const auto          server = busy_server_for(selected);
				server->AlterOne           = true;
				review_submission_session session{ *server, busy_configuration(selected, server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(selected, root.Path) };
				const auto                result = session.submit_and_reconcile(server->Request);
				expect(result.Publication == publication::indeterminate && server->Posts == 1U && server->Pages == 2U, label + "a review holding other comments is not confirmed");
			}
		}
	}
	/// [[BPR-1540]]. Keeps every diagnostic event the session records.
	class recording_log final : public application::diagnostic_log_port
	{
	public:
		std::vector<application::diagnostic_event> Events;
		auto                                       record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			try {
				Events.push_back(aEvent);
			} catch (...) {
				return application::diagnostic_log_status::dropped;
			}
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
	};
	/// [[BPR-1540]]. One way a submission stays unknown: how to arrange it,
	/// whether it is found by Check status in a later session, and the
	/// reason it must name.
	struct unknown_case
	{
		std::string_view                         Name;
		core::review_submission_unknown_reason   Reason;
		bool                                     CheckStatus = false;
		std::function<void(busy_server&, bool&)> Arrange;
	};
	[[nodiscard]] auto answer_on(std::string aPathPart, github_mutation_wire_completion aCompletion, const bool aAfterPost = true)
		-> std::function<std::optional<github_mutation_wire_completion>(const review_mutation_wire_request&)>
	{
		auto posted = std::make_shared<bool>(false);
		return [part = std::move(aPathPart), completion = std::move(aCompletion), aAfterPost, posted](const review_mutation_wire_request& aWire) -> std::optional<github_mutation_wire_completion> {
			if (aWire.Method == http_method::post) {
				*posted = true;
				return std::nullopt;
			}
			if ((!aAfterPost || *posted) && aWire.Path.find(part) != std::string::npos) {
				return completion;
			}
			return std::nullopt;
		};
	}
	[[nodiscard]] auto wire_answer(const int aStatus, std::string aBody, std::vector<http_header> aHeaders = { }) -> github_mutation_wire_completion
	{
		return github_mutation_wire_success{ .Response = { .Status = aStatus, .Headers = std::move(aHeaders), .Body = std::move(aBody) } };
	}
	[[nodiscard]] auto unknown_cases() -> std::vector<unknown_case>
	{
		using reason      = core::review_submission_unknown_reason;
		const auto other  = std::string{ "/repositories/999/pulls/1/comments?per_page=100&page=2" };
		auto       result = std::vector<unknown_case>{ };
		const auto add    = [&result](std::string_view aName, reason aReason, bool aCheck, std::function<void(busy_server&, bool&)> aArrange) {
			result.push_back({ .Name = aName, .Reason = aReason, .CheckStatus = aCheck, .Arrange = std::move(aArrange) });
		};
		add("not listed", reason::review_not_listed, false, [](busy_server& aServer, bool& aOn) { aServer.Visible = !aOn; });
		add("several listed", reason::several_reviews_listed, false, [](busy_server& aServer, bool& aOn) { aServer.Twice = aOn; });
		add("list unreadable", reason::review_list_unreadable, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/reviews?per_page=100", wire_answer(200, "{"));
			}
		});
		add("list too large", reason::review_list_too_large, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/reviews?per_page=100", wire_answer(200, std::string(maximum_review_mutation_list_response_bytes + 1U, ' ')));
			}
		});
		add("list paged", reason::review_list_paged, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/reviews?per_page=100", wire_answer(200, "[]", { { .Name = "Link", .Value = "<https://api.github.com/repositories/1328277631/pulls/1/reviews?per_page=100&page=2>; rel=\"next\"" } }));
			}
		});
		add("page unreadable", reason::comment_page_unreadable, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/comments?per_page=100&page=1", wire_answer(200, "[{"));
			}
		});
		add("page too large", reason::comment_page_too_large, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/comments?per_page=100&page=1", wire_answer(200, std::string(maximum_review_mutation_list_response_bytes + 1U, ' ')));
			}
		});
		add("link refused", reason::comment_page_link_refused, false, [other](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/comments?per_page=100&page=1", wire_answer(200, "[]", { { .Name = "Link", .Value = "<https://api.github.com" + other + ">; rel=\"next\"" } }));
			}
		});
		add("collection too long", reason::comment_collection_too_long, false, [](busy_server& aServer, bool& aOn) {
			if (!aOn) {
				return;
			}
			auto posted    = std::make_shared<bool>(false);
			aServer.Answer = [&aServer, posted](const review_mutation_wire_request& aWire) -> std::optional<github_mutation_wire_completion> {
				if (aWire.Method == http_method::post) {
					*posted = true;
					return std::nullopt;
				}
				const auto key = std::string{ "/comments?per_page=100&page=" };
				const auto at  = aWire.Path.find(key);
				if (!*posted || at == std::string::npos) {
					return std::nullopt;
				}
				const auto page  = static_cast<std::uint32_t>(std::stoul(aWire.Path.substr(at + key.size())));
				auto       value = aServer.Comments.front();
				value.Id         = 50'000U + page;
				return wire_answer(200, "[" + github_shapes::comment_json(aServer.Pull, value) + "]", { { .Name = "Link", .Value = "<" + aServer.Pull.comments_page(page + 1U) + ">; rel=\"next\"" } });
			};
		});
		add("comments mismatch", reason::comments_mismatch, false, [](busy_server& aServer, bool& aOn) { aServer.AlterOne = aOn; });
		add("server error", reason::http_status, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/reviews?per_page=100", wire_answer(502, "{}"));
			}
		});
		add("rate limited", reason::rate_limited, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/comments?per_page=100&page=1", github_mutation_wire_error{ .ProviderFailure = application::github_provider_failure::rate_limit, .HttpStatus = 403U });
			}
		});
		add("timeout", reason::timeout, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/reviews?per_page=100", github_mutation_wire_error{ .ProviderFailure = application::github_provider_failure::timeout });
			}
		});
		add("network", reason::network, false, [](busy_server& aServer, bool& aOn) {
			if (aOn) {
				aServer.Answer = answer_on("/comments?per_page=100&page=2", github_mutation_wire_error{ .ProviderFailure = application::github_provider_failure::network });
			}
		});
		add("moved without original", reason::comment_moved_without_original_position, true, [](busy_server& aServer, bool& aOn) { aServer.NoOriginal = aOn; });
		add("account mismatch", reason::account_mismatch, true, [](busy_server& aServer, bool& aOn) { aServer.OtherViewer = aOn; });
		add("pull request mismatch", reason::pull_request_mismatch, true, [](busy_server& aServer, bool& aOn) { aServer.OtherPull = aOn; });
		add("connection unavailable", reason::connection_unavailable, true, [](busy_server& aServer, bool& aOn) { aServer.RefuseOperation = aOn; });
		// The two below are arranged by the row: no local record, and a
		// stopped Check status.
		add("local record unavailable", reason::local_record_unavailable, true, [](busy_server&, bool&) { });
		add("cancelled", reason::cancelled, true, [](busy_server&, bool&) { });
		return result;
	}
	/// [[BPR-1540]]. Each reason, for GitHub.com and GHES: the unknown result
	/// names it with its counts, the attempt keeps it, and the diagnostics log
	/// receives it with nothing but enums and counts. Each case's control,
	/// the same arrangement without its cause, is published and names, keeps
	/// and logs nothing.
	void test_unknown_reasons_are_named_kept_and_logged()
	{
		using publication = application::review_submission_publication;
		auto covered      = std::set<core::review_submission_unknown_reason>{ };
		for (const auto& selected : busy_cases()) {
			for (const auto& entry : unknown_cases()) {
				for (const auto cause : { true, false }) {
					const auto          label = std::string{ selected.Name } + ", " + std::string{ entry.Name } + (cause ? "" : " (control)") + ": ";
					temporary_directory root;
					temporary_directory spare;
					const auto          server = busy_server_for(selected);
					auto                log    = std::make_shared<recording_log>();
					auto                on     = cause;
					auto                result = application::review_submission_result{ };
					auto                stop   = std::stop_source{ };
					auto                head   = server->Request.Proposal.Target.ExpectedHead;
					if (!entry.CheckStatus) {
						entry.Arrange(*server, on);
						review_submission_session session{ *server, busy_configuration(selected, head), std::make_shared<lease>(), busy_storage(selected, root.Path), log };
						result = session.submit_and_reconcile(server->Request);
					} else {
						// Unknown first (not listed yet), then Check status in a
						// later session; the cause is arranged for Check status.
						server->Visible = false;
						{
							review_submission_session session{ *server, busy_configuration(selected, head), std::make_shared<lease>(), busy_storage(selected, root.Path) };
							expect(session.submit_and_reconcile(server->Request).Publication == publication::indeterminate, label + "the first answer is unknown");
						}
						server->Visible = true;
						entry.Arrange(*server, on);
						if (entry.Reason == core::review_submission_unknown_reason::comment_moved_without_original_position) {
							server->Pushed = true;
							head           = commit(0x33U);
						}
						auto store = busy_storage(selected, root.Path);
						if (cause && entry.Reason == core::review_submission_unknown_reason::local_record_unavailable) {
							store = busy_storage(selected, spare.Path);
						}
						if (cause && entry.Reason == core::review_submission_unknown_reason::cancelled) {
							stop.request_stop();
						}
						review_submission_session later{ *server, busy_configuration(selected, head), std::make_shared<lease>(), std::move(store), log };
						result = later.reconcile(server->Request, stop.get_token());
					}
					auto       check = busy_storage(selected, root.Path);
					const auto kept  = check->load(server->Request.IdempotencyKey);
					if (!cause) {
						expect(result.Publication == publication::published && !result.Unknown && log->Events.empty(), label + "without its cause the review is confirmed and nothing is named or logged");
						expect(kept && *kept && !(**kept).LastUnknown, label + "and the attempt keeps no reason");
						continue;
					}
					if (!result.Unknown || result.Unknown->Reason != entry.Reason) {
						std::cerr << label << "named " << (result.Unknown ? core::name(result.Unknown->Reason) : std::string_view{ "nothing" }) << '\n';
					}
					expect(result.Publication == publication::indeterminate && result.Unknown && result.Unknown->Reason == entry.Reason, label + "the unknown result names its reason");
					expect(result.Unknown->CommentsExpected == 3U && core::valid(*result.Unknown), label + "with the review's three comments counted");
					expect(server->Posts == 1U, label + "and nothing is sent again");
					covered.insert(entry.Reason);
					const auto local = entry.Reason == core::review_submission_unknown_reason::local_record_unavailable;
					expect(local || (kept && *kept && (**kept).LastUnknown == result.Unknown), label + "the attempt keeps the reason");
					expect(!log->Events.empty(), label + "the diagnostics log receives it");
					const auto& event = log->Events.back();
					expect(event.Operation == application::diagnostic_operation::review_submission && event.Outcome == application::diagnostic_outcome::failed && event.ReviewSubmission == result.Unknown
							   && event.Summary.State == application::diagnostic_summary_state::sanitized && event.Summary.text() == core::describe(*result.Unknown) && event.Request == 0U && event.HttpStatus == 0U,
						   label + "as one counts-only event");
					const auto text = event.Summary.text();
					expect(std::ranges::all_of(text, [](const char aCharacter) { return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '_' || aCharacter == '=' || aCharacter == ' '; }),
						   label + "whose words are only enum names, keys and digits");
					for (const auto* secret : { "MikeMarcin", "BPR-Test", "github.com", "ghe.example", "fixture-reviewer", "question:", "nit:", "praise:", "src/" }) {
						expect(text.find(secret) == std::string_view::npos, label + "with no host, repository, account, path or comment words");
					}
				}
			}
		}
		expect(covered.size() == core::review_submission_unknown_reason_count, "every reason is reached by a case");
		// [[BPR-1541]]. A later session reads the kept reason back for the
		// page, and none once the review is confirmed.
		{
			temporary_directory root;
			const auto          selected = busy_cases().back();
			const auto          server   = busy_server_for(selected);
			server->Visible              = false;
			auto first                   = application::review_submission_result{ };
			{
				review_submission_session session{ *server, busy_configuration(selected, server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(selected, root.Path) };
				first = session.submit_and_reconcile(server->Request);
			}
			review_submission_session later{ *server, busy_configuration(selected, server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(selected, root.Path) };
			expect(first.Unknown && later.last_unknown(server->Request.IdempotencyKey) == first.Unknown && !later.last_unknown("another-key"), "a restarted session reads the kept reason back by key");
			server->Visible = true;
			expect(later.reconcile(server->Request).Publication == application::review_submission_publication::published && !later.last_unknown(server->Request.IdempotencyKey),
				   "and none once Check status confirms the review");
		}
		// The counts say what was found.
		{
			temporary_directory root;
			const auto          server = busy_server_for(busy_cases().front());
			server->AlterOne           = true;
			review_submission_session session{ *server, busy_configuration(busy_cases().front(), server->Request.Proposal.Target.ExpectedHead), std::make_shared<lease>(), busy_storage(busy_cases().front(), root.Path) };
			const auto                result = session.submit_and_reconcile(server->Request);
			expect(result.Unknown && result.Unknown->CommentsFound == 3U && result.Unknown->CommentsMatched == 2U && result.Unknown->CandidateReviews == 1U && result.Unknown->PagesRead == 2U
					   && result.Unknown->PostAnswer == core::review_submission_post_answer::error_status && result.Unknown->PostStatusClass == 5U,
				   "a mismatch counts the comments held and matched, the new review, the pages read and the POST's 5xx answer");
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
			if (auto answer = merge_base_answer(aRequest.Wire, false)) {
				return *answer;
			}
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
					   && result.Failure == application::review_mutation_failure::refused_by_server
					   && session.pending() && session.pending()->empty(),
				   "GitHub's 422 and its unchanged review list are a definite refusal by the server, withdrawn");
		}
		std::cout << "replay-live:pass\n";
	}
	// [[BPR-1538]]. The BPR-1538 probe's two halves: an approval with three
	// comments on MikeMarcin/BPR-Test#70, which already held 100 review
	// comments. `--compile-busy-live DIR` writes the exact body BPR sends;
	// `--replay-busy-live DIR` runs the production session over GitHub's own
	// answers, headers included, saved by the probe. Not registered as a row.
	[[nodiscard]] auto busy_live_request(const live_probe& aProbe) -> application::review_submission_request
	{
		auto changed               = core::review_submission_comment{ };
		changed.Draft              = core::review_draft_id{ 1U };
		changed.DraftRevision      = 2U;
		changed.Path               = "bpr-1538-probe/many.txt";
		changed.Line               = 149U;
		changed.Body               = "BPR-1538 live probe: one added line.";
		auto range                 = changed;
		range.Draft                = core::review_draft_id{ 2U };
		range.Line                 = 162U;
		range.StartLine            = 160U;
		range.Body                 = "BPR-1538 live probe: a range of added lines.";
		auto removed               = changed;
		removed.Draft              = core::review_draft_id{ 3U };
		removed.Path               = "bpr-1538-probe/base.txt";
		removed.Side               = core::diff_side::left;
		removed.Line               = 4U;
		removed.Body               = "BPR-1538 live probe: a removed base line.";
		auto value                 = aProbe.request({ changed, range, removed }, "busy-live-probe");
		value.Proposal.Disposition = core::review_disposition::approve;
		return value;
	}
	void compile_busy_live(const std::filesystem::path& aFolder)
	{
		const live_probe probe{ aFolder };
		const auto       request = busy_live_request(probe);
		auto             input   = review_submission_request{ .Target = request.LiveBinding->Target, .ActingIdentity = request.LiveBinding->ActingIdentity, .Disposition = request.Proposal.Disposition, .Body = request.Proposal.Body, .Comments = request.Proposal.Comments };
		const auto       wire    = compile_review_submission_request(input);
		expect(wire.has_value(), "the probe's review compiles");
		probe.write("review-body.json", wire->Body);
		std::cout << "compile-busy-live:pass\n";
	}
	/// Replays the probe's saved answers. A `.headers` file beside an answer
	/// holds its response headers, one `Name: Value` per line.
	struct busy_replay final : github_mutation_wire_transport
	{
		const live_probe& Probe;
		/// The review list after the POST, and whether it is served yet;
		/// whether GitHub's answer to the POST is lost to a gateway timeout.
		bool Listed = true, Sent = false, Lose = false;
		/// Whether the author has pushed: GitHub's answers after the push.
		bool        Pushed = false;
		std::size_t Posts = 0U, Pages = 0U;
		explicit busy_replay(const live_probe& aProbe)
			: Probe{ aProbe }
		{ }
		[[nodiscard]] auto answer(const char* aName) const -> github_mutation_wire_completion
		{
			auto               headers = std::vector<http_header>{ };
			std::istringstream lines{ Probe.file((std::string{ aName } + ".headers").c_str()) };
			for (std::string line; std::getline(lines, line);) {
				if (const auto colon = line.find(": "); colon != std::string::npos) {
					headers.push_back({ .Name = line.substr(0U, colon), .Value = line.substr(colon + 2U) });
				}
			}
			return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = std::move(headers), .Body = Probe.file((std::string{ aName } + ".json").c_str()) } };
		}
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			if (auto answer = merge_base_answer(aRequest.Wire, false)) {
				return *answer;
			}
			const bpr::tests::exit_on_failure insideTheSession;
			const auto&                       wire = aRequest.Wire;
			const auto                        pull = "/repos/MikeMarcin/BPR-Test/pulls/" + Probe.text("number");
			if (wire.Method == http_method::get && wire.Path == "/user") {
				return answer("user");
			}
			if (wire.Method == http_method::get && wire.Path == pull) {
				return answer(Pushed ? "pushed-pull" : "pull");
			}
			if (wire.Method == http_method::get && wire.Path == pull + "/reviews?per_page=100") {
				return answer(Pushed ? "pushed-reviews" : Sent && Listed ? "reviews-after"
																		 : "reviews-before");
			}
			for (const auto page : { 1U, 2U }) {
				if (wire.Method == http_method::get && wire.Path == pull + "/comments?per_page=100&page=" + std::to_string(page)) {
					++Pages;
					return answer(page == 1U ? (Pushed ? "pushed-comments-1" : "comments-1") : (Pushed ? "pushed-comments-2" : "comments-2"));
				}
			}
			if (wire.Method == http_method::post && wire.Path == pull + "/reviews") {
				expect(aWitness && aWitness(), "the replayed POST is witnessed first");
				expect(wire.Body == Probe.file("review-body.json"), "the body replayed is the body the probe sent live");
				++Posts;
				Sent = true;
				if (Lose || Probe.file("review-post.status") != "200") {
					return github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate, .HttpStatus = 504U };
				}
				return answer("review-post");
			}
			std::cerr << "FAILED: unreplayed request " << wire.Path << '\n';
			bpr::tests::fail();
		}
	};
	void replay_busy_live(const std::filesystem::path& aFolder)
	{
		using publication = application::review_submission_publication;
		const live_probe probe{ aFolder };
		const auto       request                          = busy_live_request(probe);
		auto             configuration                    = ::configuration();
		configuration.Target                              = probe.binding().Target;
		configuration.ActingIdentity                      = probe.binding().ActingIdentity;
		configuration.CredentialContext.Authority.Account = configuration.ActingIdentity.Account;
		{
			temporary_directory       root;
			busy_replay               wire{ probe };
			review_submission_session session{ wire, configuration, std::make_shared<lease>(), probe.store(root.Path) };
			const auto                result = session.submit_and_reconcile(request);
			expect(result.Publication == publication::published && result.Receipt && result.Receipt->ReviewDatabaseId.value() == probe.number("review_id") && wire.Posts == 1U && wire.Pages == 2U,
				   "GitHub's own answers confirm the approval with its comments on a pull request with 100 earlier comments, reading both pages");
		}
		{
			temporary_directory root;
			busy_replay         wire{ probe };
			wire.Listed = false;
			wire.Lose   = true;
			{
				review_submission_session session{ wire, configuration, std::make_shared<lease>(), probe.store(root.Path) };
				expect(session.submit_and_reconcile(request).Publication == publication::indeterminate, "a review not yet listed after a lost answer is unknown");
			}
			wire.Listed = true;
			review_submission_session later{ wire, configuration, std::make_shared<lease>(), probe.store(root.Path) };
			const auto                result = later.reconcile(request);
			expect(result.Publication == publication::published && result.Receipt && result.Receipt->ReviewDatabaseId.value() == probe.number("review_id") && wire.Posts == 1U,
				   "Check status confirms it from GitHub's answers without sending again");
			expect(later.pending() && later.pending()->empty(), "and the attempt is no longer restored");
		}
		{
			// The author pushed after the review: GitHub moved its comments
			// to the new head and five lines down. Check status runs where
			// the window now observes that head.
			temporary_directory root;
			busy_replay         wire{ probe };
			wire.Listed = false;
			wire.Lose   = true;
			{
				review_submission_session session{ wire, configuration, std::make_shared<lease>(), probe.store(root.Path) };
				expect(session.submit_and_reconcile(request).Publication == publication::indeterminate, "a review not yet listed after a lost answer is unknown");
			}
			wire.Pushed                        = true;
			auto moved                         = configuration;
			moved.Target.Semantic.ExpectedHead = *core::parse_git_commit_oid(probe.text("pushed_head"));
			review_submission_session later{ wire, moved, std::make_shared<lease>(), probe.store(root.Path) };
			const auto                result = later.reconcile(request);
			expect(result.Publication == publication::published && result.Receipt && result.Receipt->ReviewDatabaseId.value() == probe.number("review_id") && wire.Posts == 1U,
				   "after the push, Check status confirms it by where its comments were written");
			expect(later.pending() && later.pending()->empty(), "and the attempt is no longer restored");
		}
		std::cout << "replay-busy-live:pass\n";
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
		row{ "review_with_comments_reconciles_on_a_busy_pull_request", &test_a_review_with_comments_reconciles_on_a_busy_pull_request },
		row{ "unknown_reasons_are_named_kept_and_logged", &test_unknown_reasons_are_named_kept_and_logged },
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
		} else if (phase == "--compile-busy-live") {
			compile_busy_live(aArgv[2]);
		} else if (phase == "--replay-busy-live") {
			replay_busy_live(aArgv[2]);
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
