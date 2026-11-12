/// \file
/// \brief RFC-BPR-0109 ([[BPR-2023]]). A local draft's evidence without any
/// file: claims revised with their history, the author's accept and reject
/// of what an import proposes, the one import format refused whole for
/// anything it does not define (an observed class included), binding by the
/// exact material a result was produced on, which results a later candidate
/// must have rechecked, what each method's result can speak for, the
/// selection a description or tour may read, and the stored form.

#include "bpr/core/local_draft_evidence.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto commit(const char aDigit) -> core::git_commit_oid
	{
		return *core::parse_git_commit_oid(std::string(40U, aDigit));
	}

	auto entry_oid(const char aDigit) -> core::git_entry_oid
	{
		return *core::parse_git_entry_oid(std::string(40U, aDigit));
	}

	auto side(const char aDigit, const bool aWorking = false) -> core::draft_side
	{
		return { .Mode = 0100644, .Object = entry_oid(aDigit), .Origin = aWorking ? core::draft_material_origin::working : core::draft_material_origin::committed };
	}

	/// Candidate 1: `src/engine.cpp` and `docs/notes.md` changed at commit a.
	auto first_candidate() -> core::local_draft_candidate
	{
		return { .Head      = commit('a'),
				 .TargetTip = commit('b'),
				 .Base      = commit('c'),
				 .Changes   = { { .Path = "docs/notes.md", .Change = core::draft_change::modified, .Left = side('6'), .Right = side('1') },
								{ .Path = "src/engine.cpp", .Change = core::draft_change::modified, .Left = side('2'), .Right = side('3') } } };
	}

	auto make_draft() -> core::local_draft
	{
		auto created = core::create_local_draft({ }, { std::string(32U, '1') },
												{ .Location = "/work/feature", .Repository = { .Volume = 1, .ObjectLow = 10 }, .Worktree = { .Volume = 1, .ObjectLow = 11 } }, "feature/x",
												{ .Target = { .Remote = "origin", .Repository = { .Host = "github.com", .Owner = "acme", .Name = "engine" }, .Branch = "main" } }, { },
												first_candidate(), 100);
		expect(created.has_value(), "a draft is created");
		return *created;
	}

	template <typename T>
	auto ok(T aResult, const std::string_view aMessage) -> core::local_draft
	{
		expect(aResult.has_value(), aMessage);
		return *aResult;
	}

	void test_claims_checks_and_questions()
	{
		auto draft       = make_draft();
		draft            = ok(core::add_claim(draft, draft.Version, core::claim_kind::bug_fix, "Cancelling a search no longer leaks its worker.", 110), "a claim");
		const auto claim = draft.Evidence.Claims[0].Id;
		expect(draft.Evidence.Claims[0].Disposition == core::evidence_disposition::accepted && draft.Evidence.Claims[0].Origin.Source == core::evidence_source::author,
			   "what the author enters is accepted and theirs");
		draft = ok(core::revise_claim(draft, draft.Version, claim, "Cancelling a search releases its worker within one tick.", 111), "a revision");
		expect(draft.Evidence.Claims[0].Earlier == std::vector<std::string>{ "Cancelling a search no longer leaks its worker." }, "the earlier wording is kept");
		expect(core::add_claim(draft, draft.Version - 1U, core::claim_kind::other, "late", 112).error() == core::draft_refusal::stale, "a competing write is stale");
		expect(core::add_claim(draft, draft.Version, core::claim_kind::other, "", 112).error() == core::draft_refusal::invalid, "an empty claim is refused");

		draft            = ok(core::add_check(draft, draft.Version, core::check_method::test, "search_cancel_test", "ctest -R search_cancel", { claim }, { }, 113), "a planned check");
		const auto check = draft.Evidence.Checks[0].Id;
		expect(core::evidence_for_claim(draft, claim, 1U).Planned == 1U, "a check without a result is planned");
		expect(core::add_check(draft, draft.Version, core::check_method::test, "t", "", { 999U }, { }, 114).error() == core::draft_refusal::unknown_record,
			   "a check of no claim is refused");

		draft               = ok(core::add_question(draft, draft.Version, "Does the timer path need the same fix?", claim, 115), "a question");
		const auto question = draft.Evidence.Questions[0].Id;
		draft               = ok(core::resolve_question(draft, draft.Version, question, true, 116), "resolved");
		draft               = ok(core::resolve_question(draft, draft.Version, question, false, 117), "reopened");
		expect(!draft.Evidence.Questions[0].Resolved, "a question can stay open");
		expect(core::decide_evidence(draft, draft.Version, check, core::evidence_disposition::proposed, 118).error() == core::draft_refusal::invalid, "nothing returns to proposed");
		expect(draft.Evidence.Claims[0].Id != check && check != question, "identities are never shared");
	}

	void test_currency()
	{
		auto draft       = make_draft();
		draft            = ok(core::add_claim(draft, draft.Version, core::claim_kind::preserved_behavior, "Search results keep their order.", 1), "claim");
		draft            = ok(core::add_check(draft, draft.Version, core::check_method::test, "order", "", { draft.Evidence.Claims[0].Id }, { }, 2), "check");
		const auto check = draft.Evidence.Checks[0].Id;
		// One result scoped to src/, one with no inputs, both on candidate 1.
		draft               = ok(core::record_result(draft, draft.Version, { .Check = check, .Outcome = core::evidence_outcome::pass, .Candidate = 1U, .Inputs = { "src/" } }, 3), "scoped");
		draft               = ok(core::record_result(draft, draft.Version, { .Check = check, .Outcome = core::evidence_outcome::pass, .Candidate = 1U }, 4), "unscoped");
		draft               = ok(core::record_result(draft, draft.Version, { .Check = check, .Outcome = core::evidence_outcome::fail }, 5), "unbound");
		const auto& results = draft.Evidence.Results;
		expect(core::currency(draft, results[0], 1U) == core::evidence_currency::current, "on its own candidate it is current");
		expect(core::currency(draft, results[2], 1U) == core::evidence_currency::unbound, "a result with no candidate is unbound");

		// Candidate 2 edits only the notes: text outside the scope.
		auto text             = first_candidate();
		text.Head             = commit('d');
		text.Changes[0].Right = side('4');
		draft                 = ok(core::add_candidate(draft, draft.Version, text, 6), "a text-only candidate");
		expect(core::currency(draft, draft.Evidence.Results[0], 2U) == core::evidence_currency::carried, "an unchanged input scope carries the code result");
		expect(core::currency(draft, draft.Evidence.Results[1], 2U) == core::evidence_currency::needs_recheck, "without inputs, a later candidate needs a recheck");
		expect(core::currency(draft, draft.Evidence.Results[0], 1U) == core::evidence_currency::current, "the earlier candidate keeps its history");

		// Candidate 3 changes the code.
		auto code             = text;
		code.Head             = commit('e');
		code.Changes[1].Right = side('5');
		draft                 = ok(core::add_candidate(draft, draft.Version, code, 7), "a code candidate");
		expect(core::currency(draft, draft.Evidence.Results[0], 3U) == core::evidence_currency::needs_recheck, "a changed input needs a recheck");
		const auto counts = core::evidence_for_claim(draft, draft.Evidence.Claims[0].Id, 3U);
		expect(counts.Passing == 0U && counts.NeedsRecheck == 2U && counts.Unbound == 1U, "a stale pass is not counted as passing");

		// Candidate 4 has the same scoped content on a new base.
		auto rebased = text;
		rebased.Head = commit('f');
		rebased.Base = commit('9');
		draft        = ok(core::add_candidate(draft, draft.Version, rebased, 8), "a rebased candidate");
		expect(core::currency(draft, draft.Evidence.Results[0], 4U) == core::evidence_currency::needs_recheck, "a new base is not assumed to preserve runtime evidence");
		expect(core::currency(draft, draft.Evidence.Results[0], 1U) == core::evidence_currency::current && draft.Evidence.Results.size() == 3U, "history is not overwritten");
	}

	auto import_text(const std::string_view aResults, const std::string_view aProducer = R"({"kind":"tool","name":"ctest-bridge","version":"1.2"})") -> std::string
	{
		return std::string{ R"({"format":"bpr.evidence-import","version":1,"producer":)" } + std::string{ aProducer }
		+ R"(,"claims":[{"key":"c1","kind":"bug_fix","text":"Cancel releases the worker."}],"checks":[{"key":"k1","method":"test","title":"cancel_test","command":"ctest -R cancel","claims":["c1"]}],"results":)"
			+ std::string{ aResults } + R"(,"questions":[{"text":"Is the timer path affected?","claim":"c1"}]})";
	}

	void test_import_refusals()
	{
		const auto refusal = [](const std::string& aText) { return core::parse_evidence_import(aText).error().Reason; };
		using enum core::evidence_import_refusal;
		expect(refusal(std::string(core::maximum_evidence_import_bytes + 1U, ' ')) == too_large, "an oversized document is refused unread");
		expect(refusal("{not json") == malformed, "malformed");
		expect(refusal(R"({"format":"junit","version":1})") == format, "another format");
		expect(refusal(R"({"format":"bpr.evidence-import","version":2,"anything":true})") == version, "another version, whatever it holds");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"pass","class":"reported","exit_code":0}])")) == unknown_field, "a field this version does not define");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"pass","class":"observed"}])")) == observed, "an import cannot say it was observed");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"pass"}])")) == invalid_value, "a result must say it is reported");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"verified","class":"reported"}])")) == invalid_value, "an unknown outcome");
		expect(refusal(import_text(R"([{"check":"nope","outcome":"pass","class":"reported"}])")) == reference, "a result of no check");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"pass","class":"reported","material":{"head":"abc"}}])")) == invalid_value, "a malformed commit");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"pass","class":"reported","artifacts":[{"name":"log","path":"../secret"}]}])")) == invalid_value,
			   "an artifact outside the import's folder");
		expect(refusal(import_text(R"([{"check":"k1","outcome":"pass","class":"reported","artifacts":[{"name":"x","url":"file:///etc/passwd"}]}])")) == invalid_value,
			   "a link must be a web link");
		expect(refusal(R"({"format":"bpr.evidence-import","version":1,"producer":{"kind":"tool","name":"x"}})") == empty, "an empty document");
		expect(refusal(import_text("[]", R"({"kind":"robot","name":"x"})")) == invalid_value, "an unknown producer kind");
	}

	void test_import_binding()
	{
		auto draft = make_draft();
		// Candidate 2 includes a working edit of src/engine.cpp over commit d.
		auto dirty             = first_candidate();
		dirty.Head             = commit('d');
		dirty.Included         = { "src/engine.cpp" };
		dirty.Changes[1].Right = side('7', true);
		draft                  = ok(core::add_candidate(draft, draft.Version, dirty, 10), "a dirty candidate");
		const auto d           = std::string(40U, 'd');
		const auto a           = std::string(40U, 'a');
		const auto seven       = std::string(40U, '7');

		const auto parsed = core::parse_evidence_import(import_text(
			"["
			R"({"check":"k1","outcome":"pass","class":"reported","material":{"head":")"
			+ d + R"("},"inputs":["src/"]},)"
				  R"({"check":"k1","outcome":"pass","class":"reported","material":{"head":")"
			+ d + R"(","working":[{"path":"src/engine.cpp","object":")" + seven + R"("}]},"artifacts":[{"name":"log","path":"logs/run.txt"},{"name":"CI","url":"https://ci.example/run/1"}]},)"
																				  R"({"check":"k1","outcome":"fail","class":"reported","material":{"head":")"
			+ a + R"("}},)"
				  R"({"check":"k1","outcome":"interrupted","class":"reported"})"
				  "]"));
		expect(parsed.has_value(), "a valid import parses");
		const auto requests = core::evidence_artifact_requests(*parsed);
		expect(requests.size() == 2U && requests[0].Path == "logs/run.txt" && requests[1].Url == "https://ci.example/run/1", "the host is asked for each artifact");
		const auto artifacts = std::vector<core::evidence_artifact>{ { .Name = "log", .State = core::artifact_state::missing, .Reference = "logs/run.txt" },
																	 { .Name = "CI", .State = core::artifact_state::link, .Reference = "https://ci.example/run/1" } };
		expect(core::import_evidence(draft, draft.Version, *parsed, std::span{ artifacts.data(), 1U }, 11).error().Reason == core::evidence_import_refusal::invalid_value,
			   "every named artifact must be resolved");
		expect(core::import_evidence(draft, draft.Version + 1U, *parsed, artifacts, 11).error().Reason == core::evidence_import_refusal::stale, "a stale draft is refused");
		auto        imported = ok(core::import_evidence(draft, draft.Version, *parsed, artifacts, 11), "the import applies");
		const auto& evidence = imported.Evidence;
		expect(evidence.Claims.size() == 1U && evidence.Claims[0].Disposition == core::evidence_disposition::proposed && evidence.Checks[0].Disposition == core::evidence_disposition::proposed
				   && evidence.Questions[0].Disposition == core::evidence_disposition::proposed,
			   "imported claims, checks and questions are proposed");
		expect(evidence.Results.size() == 4U && evidence.Results[0].Origin.Source == core::evidence_source::tool && evidence.Results[0].Origin.Producer == "ctest-bridge",
			   "results carry their producer");
		expect(evidence.Results[0].Candidate == 0U, "a commit alone does not bind a candidate with working files");
		expect(evidence.Results[1].Candidate == 2U, "the exact commit and working files bind the candidate");
		expect(evidence.Results[2].Candidate == 1U && core::currency(imported, evidence.Results[2], 2U) == core::evidence_currency::needs_recheck,
			   "a result of the earlier candidate is history for the later one");
		expect(evidence.Results[3].Candidate == 0U && !evidence.Results[3].Material, "a result naming no material stays unbound");
		expect(evidence.Results[1].Artifacts[0].State == core::artifact_state::missing && evidence.Results[1].Artifacts[1].State == core::artifact_state::link,
			   "a missing artifact and a link are recorded as such");
		expect(core::import_evidence(imported, imported.Version, *parsed, artifacts, 12).error().Reason == core::evidence_import_refusal::duplicate, "the same document twice");

		// A pass reported by a claimant never settles the claim.
		const auto claim  = evidence.Claims[0].Id;
		const auto counts = core::evidence_for_claim(imported, claim, 2U);
		expect(counts.Planned == 0U && counts.Passing == 0U, "a proposed check's results do not count for the claim");
		auto accepted = ok(core::decide_evidence(imported, imported.Version, evidence.Checks[0].Id, core::evidence_disposition::accepted, 13), "accept the check");
		expect(core::evidence_for_claim(accepted, claim, 2U).Passing == 1U && accepted.Evidence.Claims[0].Disposition == core::evidence_disposition::proposed,
			   "an accepted check's pass is counted; the claim stays the author's to accept");
		auto rejected = ok(core::decide_evidence(accepted, accepted.Version, claim, core::evidence_disposition::rejected, 14), "reject the claim");
		expect(rejected.Evidence.Claims[0].Disposition == core::evidence_disposition::rejected && rejected.Evidence.Results.size() == 4U, "a rejection keeps the history");
		expect(core::import_evidence(imported, imported.Version, *core::parse_evidence_import(import_text(R"([{"check_id":999,"outcome":"pass","class":"reported"}])")), { }, 15)
					   .error()
					   .Reason
				   == core::evidence_import_refusal::reference,
			   "a result of no check of the draft");
	}

	void test_agent_report()
	{
		// RFC-BPR-0109 AC07: an Agent tab agent's build on the pull request's
		// checkout arrives reported, bound to what it tested.
		auto       draft  = make_draft();
		const auto parsed = core::parse_evidence_import(
			import_text(std::string{ R"([{"check":"k1","outcome":"pass","class":"reported","material":{"head":")" } + std::string(40U, '8') + R"("},"environment":"scratch checkout"}])",
						R"({"kind":"agent","name":"Claude Code","version":"2.1"})"));
		expect(parsed.has_value(), "an agent's report parses");
		auto        imported = ok(core::import_evidence(draft, draft.Version, *parsed, { }, 20), "an agent's report imports");
		const auto& result   = imported.Evidence.Results[0];
		expect(result.Origin.Source == core::evidence_source::agent && result.Material && result.Material->Head == commit('8') && result.Candidate == 0U,
			   "bound to the checkout it tested, not to a candidate");
		expect(core::currency(imported, result, 1U) == core::evidence_currency::unbound, "it does not speak for the candidate");
		expect(core::encode_local_draft(imported).find("observed") == std::string::npos, "nothing is labelled observed");
	}

	void test_reach_and_selection()
	{
		expect(core::reach(core::check_method::model_check) == core::evidence_reach::model && core::reach(core::check_method::proof) == core::evidence_reach::property,
			   "a model check speaks for its model, a proof for its property");
		expect(core::reach(core::check_method::benchmark) == core::evidence_reach::environment && core::reach(core::check_method::test) == core::evidence_reach::material,
			   "a benchmark for its environment, a test for its material");
		auto draft       = make_draft();
		draft            = ok(core::add_claim(draft, draft.Version, core::claim_kind::formal_property, "The queue never loses an item.", 1), "claim");
		const auto claim = draft.Evidence.Claims[0].Id;
		draft            = ok(core::revise_claim(draft, draft.Version, claim, "The bounded queue never loses an item.", 2), "revised");
		draft            = ok(core::add_check(draft, draft.Version, core::check_method::model_check, "queue.tla", "tlc queue.tla", { claim },
											  { .Property = "NoLoss", .Subject = "queue.tla model", .Tool = "TLC 2.18", .Bounds = "3 producers, 4 slots" }, 3),
							  "a formal check");
		expect(draft.Evidence.Checks[0].Formal.Bounds == "3 producers, 4 slots", "bounds are kept");
		draft = ok(core::add_check(draft, draft.Version, core::check_method::test, "t", "", { }, { .Property = "ignored" }, 4), "a test");
		expect(draft.Evidence.Checks[1].Formal == core::formal_scope{ }, "a test has no formal scope");
		draft = ok(core::record_result(draft, draft.Version, { .Check = draft.Evidence.Checks[0].Id, .Outcome = core::evidence_outcome::pass, .Candidate = 1U }, 5), "result");
		draft = ok(core::record_result(draft, draft.Version, { .Check = draft.Evidence.Checks[1].Id, .Outcome = core::evidence_outcome::pass, .Candidate = 1U }, 6), "unselected");
		draft = ok(core::add_question(draft, draft.Version, "Is four slots enough?", claim, 7), "question");
		draft = ok(core::add_question(draft, draft.Version, "A private aside.", 0U, 8), "another question");
		for (const auto id : { claim, draft.Evidence.Results[0].Id, draft.Evidence.Questions[0].Id }) {
			draft = ok(core::select_evidence(draft, draft.Version, id, true, 9), "selected");
		}
		const auto selection = core::selected_evidence(draft);
		expect(selection.Claims.size() == 1U && selection.Claims[0].Earlier.empty(), "a selected claim without its earlier wordings");
		expect(selection.Results.size() == 1U && selection.Checks.size() == 1U && selection.Checks[0].Method == core::check_method::model_check, "only the selected result and its check");
		expect(selection.Questions.size() == 1U && selection.Questions[0].Text == "Is four slots enough?", "only the selected question");

		const auto imported = core::parse_evidence_import(import_text("[]"));
		auto       proposed = ok(core::import_evidence(draft, draft.Version, *imported, { }, 10), "proposed claim");
		expect(core::select_evidence(proposed, proposed.Version, proposed.Evidence.Claims.back().Id, true, 11).error() == core::draft_refusal::invalid,
			   "a proposed claim cannot be selected until accepted");
		expect(core::selected_evidence(proposed) == selection, "proposed records are not in the selection");
	}

	void test_codec()
	{
		auto draft         = make_draft();
		draft              = ok(core::add_claim(draft, draft.Version, core::claim_kind::compatibility, "Old configs still load.", 1), "claim");
		draft              = ok(core::add_check(draft, draft.Version, core::check_method::proof, "load.v", "", { draft.Evidence.Claims[0].Id }, { .Property = "Load total", .Assumptions = "well-formed input" }, 2), "check");
		draft              = ok(core::record_result(draft, draft.Version, { .Check = draft.Evidence.Checks[0].Id, .Outcome = core::evidence_outcome::inconclusive, .Candidate = 1U, .Inputs = { "src/engine.cpp" }, .Limitations = "Timed out at depth 9." }, 3), "result");
		const auto text    = core::encode_local_draft(draft);
		const auto decoded = core::decode_local_draft(text);
		expect(decoded && *decoded == draft, "the evidence round-trips with its draft");
		auto observed = text;
		observed.replace(observed.find(R"("class":"reported")"), 18U, R"("class":"observed")");
		expect(!core::decode_local_draft(observed), "a stored observed result is not this build's record");
		const auto plain = make_draft();
		expect(core::encode_local_draft(plain).find("evidence") == std::string::npos && core::decode_local_draft(core::encode_local_draft(plain)) == plain,
			   "a draft without evidence stores as before");
	}
}

auto main() -> int
{
	test_claims_checks_and_questions();
	test_currency();
	test_import_refusals();
	test_import_binding();
	test_agent_report();
	test_reach_and_selection();
	test_codec();
	std::cout << "local-draft-evidence:pass\n";
	return EXIT_SUCCESS;
}
