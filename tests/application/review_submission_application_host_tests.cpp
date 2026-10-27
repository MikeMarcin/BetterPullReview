#include "../support/insights_observer.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_submission_application_host.hpp"
#include "review_submission_execution_authority.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
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

	struct provider_state
	{
		bool                                                        InEffect = false;
		std::size_t                                                 Created = 0U, Destroyed = 0U, Submitted = 0U, Reconciled = 0U, Tours = 0U;
		std::vector<application::review_submission_request>         Pending;
		std::function<void(application::review_submission_result&)> Answer;
		/// [[BPR-1345]]. What each key's lookup answers.
		std::map<std::string, std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure>> Admitted;
		std::size_t                                                                                                                       Lookups = 0U;
		/// [[BPR-1541]]. Why each key was last left unknown, as its record keeps it.
		std::map<std::string, core::review_submission_unknown_diagnosis> Kept;
	};
	struct provider final : application::review_submission_port
	{
		std::shared_ptr<provider_state> State;
		explicit provider(std::shared_ptr<provider_state> aState)
			: State{ std::move(aState) }
		{
			expect(State->InEffect, "provider construction runs off decision lane");
			++State->Created;
		}
		~provider() override
		{
			expect(State->InEffect, "provider/storage teardown runs off decision lane");
			++State->Destroyed;
		}
		auto publish_tour(const application::staged_review_plan& aPlan, std::stop_token, application::staged_child_authorizer aAuthorize) -> application::staged_publication_result override
		{
			expect(State->InEffect, "tour publication stays on effect lane");
			++State->Tours;
			application::staged_publication_record record{ .Plan = aPlan, .Pending = application::staged_initial_request(aPlan) };
			auto                                   changed = record;
			changed.Plan.Generation++;
			expect(!aAuthorize(changed), "child grant refuses different draft generation");
			changed = record;
			changed.Pending->Body += "changed";
			expect(!aAuthorize(changed), "child grant refuses unpreviewed body");
			expect(aAuthorize(record) && !aAuthorize(record), "one exact durable child revision consumes once");
			return { application::staged_publication_state::ready, record };
		}
		auto pending(std::stop_token) -> application::review_submission_discovery override
		{
			expect(State->InEffect, "pending discovery runs on effect lane");
			return State->Pending;
		}
		auto last_unknown(const std::string& aKey, std::stop_token) -> std::optional<core::review_submission_unknown_diagnosis> override
		{
			expect(State->InEffect, "a kept reason is read on the effect lane");
			const auto kept = State->Kept.find(aKey);
			return kept != State->Kept.end() ? std::optional{ kept->second } : std::nullopt;
		}
		auto result(const application::review_submission_request& aRequest) -> application::review_submission_result
		{
			expect(State->InEffect, "provider submission runs on effect lane");
			auto value                 = application::review_submission_result{ .Request     = aRequest,
																				.Publication = application::review_submission_publication::published,
																				.Receipt     = receipt() };
			value.Receipt->Disposition = aRequest.Proposal.Disposition;
			if (State->Answer) {
				State->Answer(value);
			}
			return value;
		}
		auto submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token) -> application::review_submission_result override
		{
			++State->Submitted;
			return result(aRequest);
		}
		auto reconcile(const application::review_submission_request& aRequest, std::stop_token) -> application::review_submission_result override
		{
			++State->Reconciled;
			return result(aRequest);
		}
		auto admitted(const std::string& aKey, std::stop_token) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure> override
		{
			expect(State->InEffect, "a lookup runs on the effect lane");
			++State->Lookups;
			const auto found = State->Admitted.find(aKey);
			return found == State->Admitted.end() ? std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure>{ std::nullopt } : found->second;
		}
	};
	struct effects final : application::effect_executor
	{
		application::manual_executor    Queue;
		std::shared_ptr<provider_state> State;
		bool                            Throw = false;
		explicit effects(std::shared_ptr<provider_state> aState)
			: State{ std::move(aState) }
		{ }
		void post(std::function<void()> aTask) override
		{
			if (Throw) {
				throw std::runtime_error{ "queue refused" };
			}
			Queue.post([this, task = std::move(aTask)] { State->InEffect = true; task(); State->InEffect = false; });
		}
	};
	struct projection final : application::review_submission_projection_port
	{
		std::shared_ptr<provider_state>                           State;
		std::vector<application::review_submission_host_terminal> Terminals;
		std::function<void()>                                     Feedback;
		explicit projection(std::shared_ptr<provider_state> aState)
			: State{ std::move(aState) }
		{ }
		void publish(const application::review_submission_host_terminal& aTerminal) override
		{
			expect(!State->InEffect, "terminals publish only on decision lane");
			Terminals.push_back(aTerminal);
			if (Feedback) {
				Feedback();
			}
		}
	};
	struct fixture
	{
		std::shared_ptr<provider_state>                 State = std::make_shared<provider_state>();
		effects                                         Effects{ State };
		application::manual_executor                    Decisions;
		projection                                      Projection{ State };
		core::application_session_id                    Session{ 1U };
		core::request_generation                        Generation{ 2U };
		std::shared_ptr<tests::insights_observer>       Insights = std::make_shared<tests::insights_observer>();
		application::review_submission_application_host Host{
			[state = State]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> { return std::make_unique<provider>(state); },
			*request().LiveBinding,
			Session,
			Generation,
			Effects,
			Decisions,
			Projection,
			{ Insights, { } }
		};
		auto grant(const application::review_submission_request& aRequest = request()) -> application::review_submission_execution_grant
		{
			return application::make_review_submission_testing_grant(Session, Generation, aRequest);
		}
		void drain()
		{
			Effects.Queue.drain();
			Decisions.drain();
		}
		auto outcome() const -> const application::review_submission_result&
		{
			return std::get<application::review_submission_result>(Projection.Terminals.back().Outcome);
		}
		~fixture()
		{
			Host.shutdown();
			drain();
		}
	};
	void test_insights_confirmed_publication_only()
	{
		fixture f;
		f.State->Answer = [](auto& aResult) { aResult.Publication=application::review_submission_publication::indeterminate; aResult.Receipt.reset(); };
		expect(f.Host.submit(request(), f.grant()).has_value(), "indeterminate fixture admitted");
		f.drain();
		expect(f.Insights->reviews().empty(), "indeterminate submission is never counted");
		f.State->Answer = { };
		expect(f.Host.reconcile(request()).has_value(), "recovery admitted");
		f.drain();
		auto events = f.Insights->reviews();
		expect(events.size() == 1 && events.front().Kind == core::insights_review_kind::submitted && events.front().Identity == "42" && events.front().Disposition == 0, "confirmed recovery counts the exact approved review identity");
		f.State->Answer = [](auto& aResult) { aResult.Duplicate = true; };
		expect(f.Host.reconcile(request()).has_value(), "duplicate recovery admitted");
		f.drain();
		expect(f.Insights->reviews().size() == 2 && f.Insights->reviews().back().Identity == events.front().Identity, "recovery retries retain the same durable dedup identity");
		f.State->Answer = [](auto& aResult) { aResult.Receipt->ActingAccount = core::github_account_id{ 999 }; };
		expect(f.Host.reconcile(request()).has_value(), "hostile fixture admitted");
		f.drain();
		expect(f.Insights->reviews().size() == 2, "invalid receipts cannot add statistics");
	}

	void test_staged_authority()
	{
		auto input                 = request();
		input.Proposal.Disposition = core::review_disposition::comment;
		input.Proposal.Body        = "Tour summary";
		auto fingerprint           = std::array<std::uint8_t, 32>{ };
		fingerprint.fill(1);
		application::staged_review_plan plan{ .Submission = input, .Owner = { core::bpr_profile_id{ 1 }, input.Proposal.Target.Host, core::host_authority_fingerprint{ fingerprint }, input.Proposal.ActingAccount, input.Proposal.Target.Repository, input.Proposal.Target.PullRequest }, .Generation = 1, .Stops = { { core::tour_stop_id{ 1 }, { .Commit = input.Proposal.Target.ExpectedHead, .Path = "a.cpp" }, "First", "Why", "https://github.com/fixture-owner/fixture-repo/blob/exact/a.cpp#L1" } } };
		expect(application::valid(plan), "valid bounded tour plan");
		fixture f;
		expect(!f.Host.publish_tour(plan, { }), "preview alone cannot publish");
		expect(!f.Host.publish_tour(plan, f.grant(input)), "summary authority is not tour authority");
		expect(!application::detail::review_submission_user_authority_issuer::issue(f.Session, f.Generation, plan), "test build cannot issue production tour authority");
		auto grant   = application::make_review_submission_testing_grant(f.Session, f.Generation, plan);
		auto changed = plan;
		changed.Generation++;
		expect(!f.Host.publish_tour(changed, grant), "changed draft generation cannot consume parent");
		expect(!f.Host.submit(input, grant), "tour grant cannot authorize ordinary submission");
		expect(f.Host.publish_tour(plan, grant).has_value() && f.State->Tours == 0, "Publish queues exact immutable plan without inline write");
		f.drain();
		expect(f.State->Tours == 1 && std::holds_alternative<application::staged_publication_result>(f.Projection.Terminals.back().Outcome), "exact tour reaches one decision-lane terminal");
		expect(!f.Host.publish_tour(plan, grant), "bounded parent cannot replay");
		auto expired = application::make_review_submission_testing_grant(f.Session, f.Generation, plan, std::chrono::steady_clock::now() - std::chrono::seconds{ 1 });
		expect(!f.Host.publish_tour(plan, expired), "expired parent refuses");
		auto revoked = application::make_review_submission_testing_grant(f.Session, f.Generation, plan);
		revoked->revoke();
		expect(!f.Host.publish_tour(plan, revoked), "revoked parent refuses");
		auto       stopped = application::make_review_submission_testing_grant(f.Session, f.Generation, plan);
		const auto queued  = f.Host.publish_tour(plan, stopped);
		expect(queued.has_value(), "fresh explicit Resume grants only new bounded work");
		f.Host.cancel(*queued);
		f.drain();
		expect(f.State->Tours == 1, "Stop revokes parent before any effect");
	}

	void test_exact_authority_and_async_delivery()
	{
		for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
			fixture f;
			auto    value              = request();
			value.Proposal.Disposition = disposition;
			value.Proposal.Body        = disposition == core::review_disposition::approve ? "" : "Literal review summary";
			expect(!f.Host.submit(value, { }), "no authority refuses before effect");
			auto grant            = f.grant(value);
			auto changed          = value;
			changed.Proposal.Body = "Different summary";
			expect(!f.Host.submit(changed, grant), "summary mismatch cannot consume authority");
			changed                      = value;
			changed.Proposal.Disposition = disposition == core::review_disposition::approve ? core::review_disposition::comment : core::review_disposition::approve;
			expect(!f.Host.submit(changed, grant), "disposition mismatch cannot consume authority");
			const auto context = f.Host.submit(value, grant);
			expect(context && context->Session == f.Session && context->Generation == f.Generation && !f.Host.is_quiescent(), "accepted request has exact host context");
			expect(f.State->Created == 0U && f.Projection.Terminals.empty(), "acceptance performs no inline effects");
			expect(!f.Host.submit(value, grant) && !f.Host.discover(), "duplicate and concurrent gestures queue nothing");
			f.Effects.Queue.drain();
			expect(f.State->Submitted == 1U && f.State->Destroyed == 1U && f.Projection.Terminals.empty() && !f.Host.is_quiescent(), "effect and teardown precede serialized terminal");
			f.Decisions.drain();
			expect(f.Host.is_quiescent() && f.Projection.Terminals.size() == 1U && f.outcome().Request == value && f.outcome().Publication == application::review_submission_publication::published, "one exact result reaches decision lane");
			expect(!f.Host.submit(value, grant), "consumed authority cannot replay");
		}
		fixture f;
		auto    revoked = f.grant();
		revoked->revoke();
		expect(!f.Host.submit(request(), revoked), "revoked authority refuses");
		auto expired = application::make_review_submission_testing_grant(f.Session, f.Generation, request(), std::chrono::steady_clock::time_point::min());
		expect(!f.Host.submit(request(), expired), "expired authority refuses");
		auto otherSession    = application::make_review_submission_testing_grant(core::application_session_id{ 99U }, f.Generation, request());
		auto otherGeneration = application::make_review_submission_testing_grant(f.Session, core::request_generation{ 99U }, request());
		expect(!f.Host.submit(request(), otherSession) && !f.Host.submit(request(), otherGeneration), "session/generation cannot reuse grant");
		expect(!application::detail::review_submission_user_authority_issuer::issue(f.Session, f.Generation, request()), "test build refuses production user authority issuance");
	}
	void test_recovery_and_bad_results()
	{
		fixture f;
		f.State->Pending = { request() };
		expect(f.Host.discover().has_value(), "discovery accepted");
		f.drain();
		expect(std::get<application::review_submission_discovery>(f.Projection.Terminals.back().Outcome).value() == f.State->Pending, "discovery preserves original request/key");
		expect(f.Projection.Terminals.back().Unknown.empty(), "[[BPR-1541]] with no kept reason the discovery carries none");
		{
			// [[BPR-1541]]. A restored attempt brings the reason its record
			// kept; an invalid one, or one for a key not discovered, does not.
			fixture kept;
			kept.State->Pending = { request() };
			auto reason         = core::review_submission_unknown_diagnosis{ .Reason = core::review_submission_unknown_reason::comments_mismatch, .CommentsExpected = 3U, .CommentsFound = 3U, .CommentsMatched = 2U };
			kept.State->Kept.emplace(request().IdempotencyKey, reason);
			kept.State->Kept.emplace("another-key", reason);
			expect(kept.Host.discover().has_value(), "discovery with a kept reason accepted");
			kept.drain();
			const auto& terminal = kept.Projection.Terminals.back();
			expect(terminal.Unknown.size() == 1U && terminal.Unknown.at(request().IdempotencyKey) == reason, "the discovery carries the discovered attempt's kept reason only");
			reason.CommentsMatched = 9U;
			kept.State->Kept.clear();
			kept.State->Kept.emplace(request().IdempotencyKey, reason);
			expect(kept.Host.discover().has_value(), "a second discovery accepted");
			kept.drain();
			expect(kept.Projection.Terminals.back().Unknown.empty(), "an invalid kept reason is not carried");
		}
		expect(f.Host.reconcile(request()).has_value(), "read-only recovery requires no new publication grant");
		f.drain();
		expect(f.State->Submitted == 0U && f.State->Reconciled == 1U, "recovery never chooses submit effect");
		f.State->Pending.front().LiveBinding->ActingIdentity.NodeId = "another-actor";
		expect(f.Host.discover().has_value(), "invalid discovery still terminates");
		f.drain();
		expect(!std::get<application::review_submission_discovery>(f.Projection.Terminals.back().Outcome), "host rejects pending identity mismatch");
		for (const auto mode : { 0, 1, 2, 3 }) {
			fixture bad;
			bad.State->Answer = [mode](auto& aResult) {
				if (mode == 0) {
					aResult.Request.Proposal.Attempt = core::review_submission_attempt_id{ 99U };
					aResult.Request.Proposal.Body.assign(1024U * 1024U, '\xFF');
				}
				if (mode == 1) {
					aResult.Receipt.reset();
				}
				if (mode == 2) {
					aResult.Receipt->ActingAccount = core::github_account_id{ 99U };
				}
				if (mode == 3) {
					throw std::runtime_error{ "unknown outcome" };
				}
			};
			expect(bad.Host.submit(request(), bad.grant()).has_value(), "fault course accepted");
			bad.drain();
			expect(bad.Projection.Terminals.size() == 1U && bad.outcome().Request == request() && !bad.outcome().Receipt && bad.outcome().Publication == application::review_submission_publication::indeterminate, "bad or missing terminal stays correlated and indeterminate");
		}
	}
	/// [[BPR-1345]]. A held draft's submission is looked up by its key: the
	/// request admitted under it, none, or an unavailable read, each one
	/// typed terminal, read only. A request for another account is refused;
	/// a key that is no key queues nothing.
	void test_lookup_by_key()
	{
		fixture f;
		f.State->Admitted.emplace("fixture-review-key", std::optional{ request() });
		f.State->Admitted.emplace("unreadable", std::unexpected(application::review_mutation_failure::unavailable));
		auto foreign                               = request();
		foreign.IdempotencyKey                     = "foreign";
		foreign.LiveBinding->ActingIdentity.NodeId = "another-actor";
		f.State->Admitted.emplace("foreign", std::optional{ foreign });
		const auto look = [&f](std::string aKey) -> application::review_submission_lookup {
			expect(f.Host.lookup(aKey).has_value(), "a lookup is accepted");
			f.drain();
			return std::get<application::review_submission_lookup>(f.Projection.Terminals.back().Outcome);
		};
		const auto found = look("fixture-review-key");
		expect(found.Key == "fixture-review-key" && found.Request && *found.Request == request(), "the admitted request comes back exactly");
		const auto absent = look("never-admitted");
		expect(absent.Request && !*absent.Request, "a key nothing was admitted under answers none");
		expect(!look("unreadable").Request, "an unavailable read is its own answer");
		expect(!look("foreign").Request, "a request admitted for another account is refused");
		expect(f.State->Submitted == 0U && f.State->Reconciled == 0U && f.State->Lookups == 4U, "a lookup never submits or reconciles");
		expect(!f.Host.lookup("") && !f.Host.lookup(std::string(300U, 'k')) && !f.Host.lookup("two words"), "a key that is no key queues nothing");
	}
	void test_cancellation_shutdown_and_reentrancy()
	{
		for (const auto mode : { 0, 1, 2 }) {
			fixture f;
			if (mode == 2) {
				f.Effects.Throw = true;
			}
			const auto context = f.Host.submit(request(), f.grant());
			expect(context.has_value(), "cancellation/failure course accepted");
			if (mode == 0) {
				f.Host.cancel(*context);
			}
			if (mode == 1) {
				f.Host.shutdown();
			}
			f.drain();
			expect(f.Projection.Terminals.size() == 1U && f.Host.is_quiescent() && f.State->Submitted == 0U && f.outcome().Publication == application::review_submission_publication::definitely_not_published, "pre-effect stop/failure has one definite terminal");
		}
		fixture f;
		f.State->Answer = [&](auto&) { f.Host.shutdown(); };
		expect(f.Host.submit(request(), f.grant()).has_value(), "shutdown-during-effect accepted");
		f.drain();
		expect(f.outcome().Publication == application::review_submission_publication::published && f.Host.is_quiescent(), "shutdown cannot erase confirmed publication");
		fixture reentrant;
		bool    feedback              = false;
		reentrant.Projection.Feedback = [&] { if (!feedback) { feedback = true; expect(reentrant.Host.discover().has_value(), "projection may enqueue next typed operation"); } };
		expect(reentrant.Host.discover().has_value(), "reentrant course begins");
		reentrant.drain();
		reentrant.drain();
		expect(reentrant.Projection.Terminals.size() == 2U && reentrant.Host.is_quiescent(), "reentrant feedback remains exactly once");
	}
	/// [[BPR-1782]]. The tour's working panel names the captured plan's own
	/// stages, in order, and the published tour's address once it is published.
	void test_publication_stages_and_link()
	{
		auto plan  = application::staged_review_plan{ };
		plan.Stops = { { .Title = "First" }, { .Title = "Second" } };
		expect(application::staged_publication_stages(plan) == std::vector<std::string>{ "The tour comment", "Stop 1, \u201cFirst\u201d", "Stop 2, \u201cSecond\u201d" }, "native plan: the tour comment, then each stop");
		plan.Compatibility = true;
		plan.Description   = application::staged_description_edit{ };
		expect(application::staged_publication_stages(plan) == std::vector<std::string>{ "The index comment", "Stop 1, \u201cFirst\u201d", "Stop 2, \u201cSecond\u201d", "The description link" }, "compatibility plan: the index comment, each stop, the description link");
		auto record  = application::staged_publication_record{ .Plan = plan };
		record.Index = application::staged_review_comment{ .WebUrl = "https://github.com/o/r/pull/1#issuecomment-7" };
		expect(application::staged_published_link(record).empty(), "no address before the record is published");
		record.Published = true;
		expect(application::staged_published_link(record) == "https://github.com/o/r/pull/1#issuecomment-7", "compatibility: the index comment's address");
		record.Index.reset();
		record.Review = application::staged_review_entity{ .WebUrl = "https://github.com/o/r/pull/1#pullrequestreview-5" };
		expect(application::staged_published_link(record) == "https://github.com/o/r/pull/1#pullrequestreview-5", "native: the review's address");
	}
	/// [[BPR-1786]]. The owner asked for the tour link at the top of the
	/// description, not the end; the description's own bytes follow it.
	void test_description_link_leads()
	{
		const auto link = std::string{ "https://github.com/o/r/pull/1#pullrequestreview-5" };
		auto       edit = application::staged_description_edit{ .Baseline = "Old", .Desired = "## Problem\n\nWhy\n" };
		expect(application::staged_description_body(edit, link) == "## Problem\n\nWhy\n", "no link when the plan adds none");
		edit.AppendIndexLink = true;
		expect(application::staged_description_body(edit, link) == "[Review tour](" + link + ")\n\n## Problem\n\nWhy\n", "the link leads the description");
		edit.Desired = "Template\r\n\r\n## Problem\r\n";
		expect(application::staged_description_body(edit, link) == "[Review tour](" + link + ")\r\n\r\nTemplate\r\n\r\n## Problem\r\n", "a CRLF description keeps CRLF");
	}
}
auto main() -> int
{
	static_assert(!std::is_convertible_v<bpr::application::review_mutation_execution_grant, bpr::application::review_submission_execution_grant>);
	static_assert(!std::is_default_constructible_v<bpr::application::review_submission_execution_authority>);
	test_insights_confirmed_publication_only();
	test_staged_authority();
	test_exact_authority_and_async_delivery();
	test_recovery_and_bad_results();
	test_lookup_by_key();
	test_cancellation_shutdown_and_reentrancy();
	test_publication_stages_and_link();
	test_description_link_leads();
	std::cout << "review-submission-application-host-tests:pass\n";
	return EXIT_SUCCESS;
}
