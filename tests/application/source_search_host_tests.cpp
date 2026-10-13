#include "../support/source_regex_engine_fixture.hpp"
#include "../support/source_search_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/source_search_host.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

	using namespace bpr;
	using namespace bpr::application;
	using namespace bpr::tests;

	struct execution final : source_search_execution_port
	{
		std::vector<source_search_work>                                                    Calls;
		std::function<source_search_execution(const source_search_work&, std::stop_token)> Answer;
		auto                                                                               execute(const source_search_work& aWork, const std::stop_token aStop) -> source_search_execution override
		{
			Calls.push_back(aWork);
			return Answer ? Answer(aWork, aStop) : source_search_execution{ search_terminal(aWork.Request) };
		}
	};

	struct projections final : source_search_projection_port
	{
		std::vector<source_search_projection>                Published;
		std::function<void(const source_search_projection&)> Feedback;
		void                                                 publish(const source_search_projection& aProjection) override
		{
			Published.push_back(aProjection);
			if (Feedback) {
				Feedback(aProjection);
			}
		}
	};

	struct fixture
	{
		manual_executor    Decisions;
		manual_executor    Effects;
		execution          Execution;
		projections        Projection;
		source_search_host Host{ Decisions, Effects, Execution, Projection };

		fixture() { search_expect(Host.set_scope(search_scope()), "scope admitted"); }
		void drain()
		{
			while (Effects.pending() || Decisions.pending()) {
				Effects.drain();
				Decisions.drain();
			}
		}
	};

	void test_public_query_and_decision_path()
	{
		fixture pane;
		fixture files;
		search_expect(files.Host.set_scope(search_scope(core::source_search_target::files)), "independent PR scope admitted");
		search_expect(pane.Host.open() && pane.Host.clear() && pane.Effects.pending() == 0U, "opening and clearing empty input does no source work");
		search_expect(pane.Host.set_query("pane\n😀") && files.Host.set_query("files"), "independent queries admitted");
		pane.Effects.drain();
		search_expect(pane.Execution.Calls.size() == 1U && pane.Execution.Calls.front().Query == "pane\n😀", "immutable exact work reaches off-lane port");
		search_expect(pane.Host.state().Status == core::source_search_status::searching && pane.Decisions.pending() == 1U && !pane.Host.is_quiescent(), "worker cannot accept or publish inline");
		pane.Decisions.drain();
		files.drain();
		search_expect(pane.Host.state().Summary.Matches == 3U && pane.Host.projection().Query == "pane\n😀" && files.Host.projection().Query == "files", "accepted terminal returns to own session");
		const auto calls = pane.Execution.Calls.size();
		for (unsigned index = 0U; index < 1000U; ++index) {
			search_expect(pane.Host.navigate(core::change_navigation_direction::next), "next admitted");
		}
		search_expect(pane.Execution.Calls.size() == calls && pane.Effects.pending() == 0U && pane.Host.is_quiescent(), "traversal never calls worker or starts any effect");
		search_expect(pane.Host.set_scope(search_scope()) && pane.Execution.Calls.size() == calls, "same exact scope refresh does no scan");
	}

	void test_coalescing_retires_every_admitted_request()
	{
		fixture f;
		search_expect(f.Host.set_query("first"), "first query admitted");
		f.Effects.drain(); // First terminal is already queued, but not accepted.
		for (unsigned index = 0U; index < 100U; ++index) {
			search_expect(f.Host.set_query("query" + std::to_string(index)), "rapid query admitted");
			search_expect(f.Host.retained_work_packets() == 2U && f.Effects.pending() == 0U, "one active terminal and one latest replacement bound residency");
		}
		search_expect(f.Host.counters().Admitted == 101U && f.Host.counters().Terminals == 99U, "replaced queued packets each receive cancelled terminal");
		const auto publications = f.Projection.Published.size();
		f.Decisions.drain();
		search_expect(f.Host.state().Summary.Matches == 0U && f.Effects.pending() == 1U && f.Projection.Published.size() == publications, "late first result publishes nothing before latest work starts");
		f.drain();
		search_expect(f.Execution.Calls.size() == 2U && f.Execution.Calls.back().Query == "query99", "intermediate queries execute no source read or matcher");
		search_expect(f.Host.counters().Admitted == f.Host.counters().Terminals && f.Host.counters().StaleTerminals == 100U && f.Host.is_quiescent(), "every admitted request has one terminal through supersession");
	}

	void test_invalid_queries_and_qualified_empty()
	{
		fixture f;
		search_expect(f.Host.set_query(std::string(4096U, 'a')), "boundary query admitted");
		f.drain();
		search_expect(f.Host.set_query(std::string(4097U, 'a')), "oversize input observation accepted");
		search_expect(f.Host.projection().Input == core::source_search_input::input_limit && f.Host.state().Summary.Matches == 0U && f.Effects.pending() == 0U, "input limit clears current hits before any work");
		search_expect(f.Host.projection().Query.size() == 4096U, "oversized seed cannot replace retained bounded query");
		search_expect(f.Host.set_query(std::string("\xf0\x9f")) && f.Host.projection().Input == core::source_search_input::invalid_utf8 && f.Effects.pending() == 0U, "invalid UTF-8 never reaches worker");
		f.Execution.Answer = [](const auto& aWork, std::stop_token) {
			auto terminal   = search_terminal(aWork.Request, 0U);
			terminal.Status = core::source_search_status::invalid_pattern;
			return terminal;
		};
		search_expect(f.Host.set_query("[", { .RegularExpression = true }), "pattern query admitted");
		f.drain();
		search_expect(f.Host.state().Status == core::source_search_status::invalid_pattern && !f.Host.projection().NoResults && !f.Host.projection().CurrentMatch, "invalid regex is distinct and keeps no old current hits");
		f.Execution.Answer = [](const auto& aWork, std::stop_token) {
			auto terminal                  = search_terminal(aWork.Request, 0U);
			terminal.Summary.SearchedFiles = 1U;
			return terminal;
		};
		search_expect(f.Host.set_query("empty"), "partial query admitted");
		f.drain();
		search_expect(f.Host.state().Status == core::source_search_status::partial && !f.Host.projection().NoResults, "unsearched membership is not No results");
		f.Execution.Answer = [](const auto& aWork, std::stop_token) { return search_terminal(aWork.Request, 0U); };
		search_expect(f.Host.set_query("absent"), "complete query admitted");
		f.drain();
		search_expect(f.Host.projection().NoResults, "completely searched empty corpus alone says No results");
	}

	void test_close_clear_reset_and_authority_gap()
	{
		fixture f;
		search_expect(f.Host.set_query("retained", { .MatchCase = true }) && f.Host.close(), "close accepted before work executes");
		f.drain();
		search_expect(f.Execution.Calls.empty() && !f.Host.projection().Open && f.Host.projection().Query == "retained" && f.Host.projection().Options.MatchCase, "close stops new source work and retains query/options");
		search_expect(f.Host.open(), "reopen admitted");
		f.drain();
		search_expect(f.Execution.Calls.size() == 1U && f.Host.state().Summary.Matches == 3U, "reopen searches retained query");
		search_expect(f.Host.clear() && f.Host.projection().Query.empty() && f.Host.state().Summary.Matches == 0U && f.Effects.pending() == 0U, "clear empties without work");
		search_expect(f.Host.set_query("account-private") && f.Host.set_scope(std::nullopt), "corpus unavailability accepted");
		auto other              = search_scope();
		other.Authority.Account = core::github_account_id{ 2U };
		search_expect(f.Host.set_scope(other) && f.Host.projection().Query.empty(), "account change across unavailable gap clears private query");
		f.drain();
		search_expect(f.Host.reset() && !f.Host.projection().Scope && !f.Host.projection().Options.MatchCase && f.Host.is_quiescent(), "reset clears all authority/query state");
	}

	void test_projection_feedback_and_shutdown()
	{
		fixture f;
		bool    once          = false;
		f.Projection.Feedback = [&f, &once](const auto& aProjection) {
			if (!once && aProjection.Status == core::source_search_status::searching) {
				once = true;
				for (unsigned index = 0U; index < 64U; ++index) {
					search_expect(f.Host.set_query("feedback" + std::to_string(index)), "bounded reentrant query queued");
				}
				search_expect(!f.Host.set_query("overflow"), "reentrant ingress at capacity refused before policy mutation");
			}
		};
		search_expect(f.Host.set_query("first") && f.Host.counters().IngressRefusals == 1U && f.Effects.pending() == 1U, "feedback deferred and coalesced before executing worker");
		f.drain();
		search_expect(f.Execution.Calls.size() == 1U && f.Execution.Calls.front().Query == "feedback63" && f.Host.counters().Admitted == f.Host.counters().Terminals, "reentrant edits leave one exact terminal for each admitted packet");
		f.Projection.Feedback = [](const auto&) { throw std::runtime_error("projection refused"); };
		search_expect(f.Host.set_query("despite projection failure"), "projection throw cannot veto effect");
		f.drain();
		search_expect(f.Host.counters().ProjectionFailures == 2U && f.Host.state().Summary.Matches == 3U && f.Host.is_quiescent(), "projection exceptions cannot veto acceptance or quiescence");
		f.Projection.Feedback = [&f](const auto& aProjection) {
			if (aProjection.Status == core::source_search_status::searching) {
				f.Host.shutdown();
			}
		};
		search_expect(f.Host.set_query("shutdown during projection"), "gesture admitted before shutdown callback");
		search_expect(f.Host.is_quiescent() && f.Effects.pending() == 0U && f.Host.counters().Admitted == f.Host.counters().Terminals, "reentrant shutdown retires unscheduled work without a source read");
		search_expect(!f.Host.set_query("closed"), "shutdown closes later ingress");
	}

	void test_stop_reentrancy_and_terminal_failures()
	{
		fixture f;
		using callback = std::stop_callback<std::function<void()>>;
		std::unique_ptr<callback> stopped;
		unsigned                  callbacks = 0U;
		f.Execution.Answer                  = [&f, &stopped, &callbacks](const auto& aWork, const std::stop_token aStop) {
			stopped = std::make_unique<callback>(aStop, [&f, &callbacks] { ++callbacks; f.Host.shutdown(); });
			return search_terminal(aWork.Request);
		};
		search_expect(f.Host.set_query("pending"), "pending query admitted");
		f.Effects.drain();
		search_expect(f.Host.set_query("replacement"), "superseding gesture admitted");
		search_expect(callbacks == 1U && !f.Host.is_quiescent(), "stop callback can reenter shutdown without deadlock; posted terminal remains owed");
		f.Decisions.drain();
		search_expect(f.Host.is_quiescent() && f.Host.counters().Admitted == f.Host.counters().Terminals && f.Host.state().Summary.Matches == 0U, "shutdown drains stale terminal without old publication");
		stopped.reset();
		fixture throwing;
		throwing.Execution.Answer = [](const source_search_work&, std::stop_token) -> core::source_search_terminal { throw std::runtime_error("effect refused"); };
		search_expect(throwing.Host.set_query("throws"), "throwing job admitted");
		throwing.drain();
		search_expect(throwing.Host.state().Status == core::source_search_status::failure && throwing.Host.counters().Terminals == 1U && throwing.Host.is_quiescent(), "effect exception receives one failure terminal");
		throwing.Execution.Answer = [](const auto& aWork, std::stop_token) {
			auto terminal = search_terminal(aWork.Request);
			++terminal.Request.Generation;
			return terminal;
		};
		search_expect(throwing.Host.set_query("foreign"), "foreign terminal job admitted");
		throwing.drain();
		search_expect(throwing.Host.state().Status == core::source_search_status::failure && !throwing.Host.state().Pending && throwing.Host.is_quiescent(), "malformed worker identity fails own request, never leaves searching stuck");
	}

	void test_effect_submission_refusal()
	{
		struct refusing final : effect_executor
		{
			void post(std::function<void()>) override { throw std::runtime_error("post refused before retention"); }
		};
		manual_executor    decisions;
		refusing           effects;
		execution          worker;
		projections        output;
		source_search_host host{ decisions, effects, worker, output };
		search_expect(host.set_scope(search_scope()) && host.set_query("refused"), "job admitted before executor refusal");
		search_expect(!host.is_quiescent() && decisions.pending() == 1U, "refused submission still owes a decision terminal");
		decisions.drain();
		search_expect(host.state().Status == core::source_search_status::failure && host.counters().Admitted == 1U && host.counters().Terminals == 1U && worker.Calls.empty() && host.is_quiescent(), "submission failure ends exactly once without worker or stale searching");
	}
	void test_real_matcher_records_and_stale_payload()
	{
		fixture    f;
		auto       entry   = core::source_match_entry{ };
		const auto scope   = search_scope();
		entry.File         = scope.Document->File;
		entry.Target       = core::source_search_target::right;
		entry.Right        = core::source_match_side{ *scope.Document->RightBlob, "café\r\ncafé" };
		f.Execution.Answer = [&entry](const auto& aWork, std::stop_token aStop) { return execute_source_pane_match(search_engine(), aWork, entry, aStop); };
		search_expect(f.Host.set_query("CAFÉ"), "real matcher query admitted");
		f.Effects.drain();
		search_expect(!f.Host.projection().Matches, "worker records not published before decision");
		f.Decisions.drain();
		const auto accepted = f.Host.projection().Matches;
		search_expect(accepted && accepted->size() == 2U && accepted->at(1U).Match.Locations[1]->Span == core::byte_span{ 7U, 12U }, "exact original records accepted through serialized host");
		for (auto index = 0U; index < 1000U; ++index) {
			(void)f.Host.navigate(core::change_navigation_direction::next);
		}
		search_expect(f.Host.projection().Matches == accepted && f.Execution.Calls.size() == 1U, "navigation reuses immutable exact records");
		search_expect(f.Host.set_query("CAFÉ") && !f.Host.projection().Matches, "query edit synchronously clears old records");
		f.Effects.drain();
		search_expect(f.Host.set_query("missing"), "replacement before late matcher terminal");
		f.Decisions.drain();
		search_expect(!f.Host.projection().Matches, "stale exact record collection never publishes");
		f.drain();
		search_expect(f.Host.projection().Matches && f.Host.projection().Matches->empty() && f.Host.projection().NoResults, "real complete empty has exact empty collection");
		search_expect(f.Host.set_query("[", { .RegularExpression = true }), "real invalid regex admitted");
		f.drain();
		search_expect(f.Host.state().Status == core::source_search_status::invalid_pattern && !f.Host.projection().Matches, "invalid regex cannot retain apparently current records");
		auto bounded      = scope;
		bounded.Selection = core::source_search_range{ core::diff_side::right, { 7U, 12U } };
		entry.Selection   = std::pair{ core::diff_side::right, core::byte_span{ 7U, 12U } };
		search_expect(f.Host.set_scope(bounded) && f.Host.set_query("CAFÉ"), "fixed selection identity admitted");
		f.drain();
		search_expect(f.Host.projection().Matches->size() == 1U && f.Host.state().Scope->Selection == bounded.Selection, "selection range stays fixed after worker and navigation");
		(void)f.Host.navigate(core::change_navigation_direction::next);
		search_expect(f.Host.state().Scope->Selection == bounded.Selection, "match navigation never moves captured bounds");
		entry.Right->Blob = *scope.Document->LeftBlob;
		search_expect(f.Host.set_query("CAFÉ"), "wrong material test admitted");
		f.drain();
		search_expect(f.Host.state().Status == core::source_search_status::unavailable && !f.Host.projection().Matches, "wrong blob cannot publish a pane span");
		f.Execution.Answer = [](const auto& aWork, std::stop_token) {
			auto output                    = source_search_execution{ search_terminal(aWork.Request, 1U) };
			auto records                   = std::make_shared<std::vector<source_search_match>>(1U);
			records->front().File          = search_identity<core::file_identity>(5U);
			records->front().Match.Preview = std::string(961U, 'x');
			output.Matches                 = std::move(records);
			return output;
		};
		(void)f.Host.set_query("payload cap");
		f.drain();
		search_expect(f.Host.state().Status == core::source_search_status::failure && !f.Host.projection().Matches, "oversized or location-free worker payload cannot be current");
		f.Host.shutdown();
		search_expect(f.Host.is_quiescent(), "real matcher owner quiesces");
	}

}

int main()
{
	test_public_query_and_decision_path();
	test_coalescing_retires_every_admitted_request();
	test_invalid_queries_and_qualified_empty();
	test_close_clear_reset_and_authority_gap();
	test_projection_feedback_and_shutdown();
	test_stop_reentrancy_and_terminal_failures();
	test_effect_submission_refusal();
	test_real_matcher_records_and_stale_payload();
	std::cout << "source-search-host:pass\n";
}
