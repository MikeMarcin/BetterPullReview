// [[BPR-1804]]. The reviewer picker's transitions and decisions, without Qt
// or a provider.
#include "bpr/core/reviewer_request.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace {
	using namespace bpr;
	using core::reviewer_choice_refusal;
	using core::reviewer_notice;
	using core::reviewer_standing;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto person(const std::string& aLogin, const std::string& aName = { }) -> core::reviewer_candidate
	{
		return { .NodeId = "U_" + aLogin, .Login = aLogin, .Name = aName };
	}

	[[nodiscard]] auto answer() -> core::reviewer_search_answer
	{
		return {
			.Author    = "octo-author",
			.Suggested = { person("sam", "Samira Ortega"), person("Octo-Author") },
			.Matches   = { person("octo-author"), person("ana", "Ana Lima"), person("SAM"), person("rui"), person("bea") },
			.Requested = { "Rui" },
			.Reviewed  = { "bea" },
		};
	}

	void test_rows_put_suggestions_first_and_never_the_author()
	{
		const auto rows = core::reviewer_rows(answer(), "");
		expect(rows.size() == 4U, "the author and the second spelling of sam are dropped");
		expect(rows[0].Candidate.Login == "sam" && rows[0].Candidate.Suggested, "a suggestion leads");
		expect(rows[1].Candidate.Login == "ana" && !rows[1].Candidate.Suggested, "then GitHub's matches in order");
		expect(rows[2].Standing == reviewer_standing::requested, "an open request is marked, logins compared without case");
		expect(rows[3].Standing == reviewer_standing::reviewed, "a submitted review is marked for re-request");
		expect(rows[1].Standing == reviewer_standing::available, "everyone else may be asked");

		const auto named = core::reviewer_rows(answer(), "lima");
		expect(named.size() == 1U && named[0].Candidate.Login == "ana", "a query matches the profile name too");
		const auto folded = core::reviewer_rows(answer(), "SA");
		expect(folded.size() == 1U && folded[0].Candidate.Login == "sam", "and folds ASCII case");

		auto many = core::reviewer_search_answer{ };
		for (int index = 0; index < 50; ++index) {
			many.Matches.push_back(person("p" + std::to_string(index)));
		}
		many.Matches.push_back(core::reviewer_candidate{ .NodeId = "", .Login = "ghost" });
		expect(core::reviewer_rows(many, "").size() == core::maximum_reviewer_rows, "at most thirty people are listed");
		expect(core::reviewer_rows(many, "ghost").empty(), "a person without a node cannot be asked, so is not listed");
	}

	void test_queries_are_normalized()
	{
		expect(core::normalize_reviewer_query("  @Sam \n") == "Sam", "spaces and one @ go");
		expect(core::normalize_reviewer_query("   ").empty(), "blank is the empty query");
		auto       long_text = std::string(63U, 'a') + "\xC3\xA9" + "zz";
		const auto cut       = core::normalize_reviewer_query(long_text);
		expect(cut == std::string(63U, 'a'), "a two-byte character over the limit is dropped whole");
		expect(core::normalize_reviewer_query(std::string(80U, 'b')).size() == core::maximum_reviewer_query_bytes, "plain text is cut at the limit");
	}

	void test_searches_keep_only_the_latest_answer()
	{
		auto opened = core::open_reviewer_picker({ });
		expect(opened.State.Open && opened.State.Searching, "opening searches");
		const auto* const first = std::get_if<core::reviewer_search_effect>(&opened.Effect);
		expect(first != nullptr && first->Search == 1U && first->Query.empty(), "the first search is the empty query");
		expect(std::holds_alternative<std::monostate>(core::open_reviewer_picker(opened.State).Effect), "opening again asks nothing");

		auto state = core::accept_reviewer_search(opened.State, 1U, answer());
		expect(!state.Searching && state.Rows.size() == 4U, "the answer is listed");

		auto              typed  = core::type_reviewer_query(state, " an");
		const auto* const second = std::get_if<core::reviewer_search_effect>(&typed.Effect);
		expect(second != nullptr && second->Search == 2U && second->Query == "an", "a new query is a new search");
		expect(typed.State.Rows.size() == 1U && typed.State.Rows[0].Candidate.Login == "ana", "known rows narrow at once");
		expect(std::holds_alternative<std::monostate>(core::type_reviewer_query(typed.State, "an ").Effect), "the same normalized query asks nothing");

		auto later = core::type_reviewer_query(typed.State, "rui");
		auto stale = core::accept_reviewer_search(later.State, 2U, core::reviewer_search_answer{ .Matches = { person("zed") } });
		expect(stale == later.State, "an older search's answer is dropped");
		auto failed = core::fail_reviewer_search(later.State, 2U, core::reviewer_request_failure::connection);
		expect(failed == later.State, "so is its failure");
		failed = core::fail_reviewer_search(later.State, 3U, core::reviewer_request_failure::connection);
		expect(!failed.Searching && failed.SearchFailure == core::reviewer_request_failure::connection, "the latest failure is shown");

		auto closed = core::close_reviewer_picker(failed);
		expect(!closed.Open && closed.Rows.empty() && closed.Query.empty(), "closing forgets the search");
		expect(core::accept_reviewer_search(closed, 3U, answer()) == closed, "a closed picker takes no answer");
		expect(std::holds_alternative<std::monostate>(core::type_reviewer_query(closed, "x").Effect), "nor a query");
	}

	void test_choosing_asks_once_and_says_what_came_of_it()
	{
		auto state = core::accept_reviewer_search(core::open_reviewer_picker({ }).State, 1U, answer());

		auto              chose   = core::choose_reviewer(state, "ANA");
		const auto* const request = std::get_if<core::reviewer_request_effect>(&chose.Effect);
		expect(request != nullptr && request->Request == 1U && request->NodeId == "U_ana" && !request->Rerequest, "an available person is asked for once");
		expect(chose.State.Pending == std::optional<std::string>{ "ana" }, "and waited on");

		auto busy = core::choose_reviewer(chose.State, "bea");
		expect(busy.Refusal == reviewer_choice_refusal::busy && busy.State.Notice == reviewer_notice::busy, "a second choice waits for the first");

		auto done = core::finish_reviewer_request(chose.State, 1U, { .Requested = { "Rui", "ana" }, .Written = true });
		expect(!done.Pending && done.Notice == reviewer_notice::requested && done.NoticeLogin == "ana", "GitHub's list names them: requested");
		expect(done.Rows[1].Standing == reviewer_standing::requested, "and the row says so at once");
		expect(core::finish_reviewer_request(done, 1U, { }) == done, "an answer to nothing waiting is dropped");

		auto again = core::choose_reviewer(done, "ana");
		expect(again.Refusal == reviewer_choice_refusal::already_requested && again.State.Notice == reviewer_notice::already_requested, "an open request is not asked again");
		expect(core::choose_reviewer(done, "nobody").Refusal == reviewer_choice_refusal::unknown, "nor someone not listed");
		expect(core::choose_reviewer(done, "octo-author").Refusal == reviewer_choice_refusal::unknown, "nor the author");
		expect(core::choose_reviewer(core::close_reviewer_picker(done), "bea").Refusal == reviewer_choice_refusal::closed, "nor anyone once closed");

		auto              rerequest = core::choose_reviewer(done, "bea");
		const auto* const asked     = std::get_if<core::reviewer_request_effect>(&rerequest.Effect);
		expect(asked != nullptr && asked->Rerequest && asked->Request == 2U, "a reviewer is asked again");
		auto re = core::finish_reviewer_request(rerequest.State, 2U, { .Requested = { "bea" }, .Written = true });
		expect(re.Notice == reviewer_notice::rerequested, "and that is said as a re-request");

		auto dropped = core::finish_reviewer_request(core::choose_reviewer(done, "sam").State, 2U, { .Requested = { "Rui" }, .Written = true });
		expect(dropped.Notice == reviewer_notice::not_added && dropped.NoticeLogin == "sam", "GitHub answering without them is not added");

		auto known = core::finish_reviewer_request(core::choose_reviewer(done, "sam").State, 2U, { .Requested = { "sam" }, .Written = false });
		expect(known.Notice == reviewer_notice::already_requested, "nothing sent and listed: GitHub already had it");

		auto failed = core::fail_reviewer_request(core::choose_reviewer(done, "sam").State, 2U, core::reviewer_request_failure::refused);
		expect(failed.Notice == reviewer_notice::failed && failed.NoticeFailure == core::reviewer_request_failure::refused && !failed.Pending, "a refusal is kept with its reason");

		auto closing = core::close_reviewer_picker(core::choose_reviewer(done, "sam").State);
		auto after   = core::finish_reviewer_request(closing, 2U, { .Requested = { "sam" }, .Written = true });
		expect(after.Notice == reviewer_notice::requested && !after.Pending, "a request finishes after the picker closed");
		auto reopened = core::open_reviewer_picker(after);
		expect(reopened.State.Notice == reviewer_notice::none, "the next opening starts quiet");
	}

	void test_the_provider_decides_before_writing()
	{
		const auto observed = answer();
		expect(core::decide_reviewer_write(observed, "ana") == core::reviewer_write_decision::write, "an available person is written");
		expect(core::decide_reviewer_write(observed, "rui") == core::reviewer_write_decision::already_requested, "an open request is not written again");
		expect(core::decide_reviewer_write(observed, "OCTO-AUTHOR") == core::reviewer_write_decision::author, "the author is never asked");
		expect(core::decide_reviewer_write(observed, "bea") == core::reviewer_write_decision::write, "a reviewer is asked again");
		expect(core::name(core::reviewer_notice::not_added) == "not_added" && core::name(reviewer_standing::reviewed) == "reviewed", "names are stable");
	}
} // namespace

auto main() -> int
{
	test_rows_put_suggestions_first_and_never_the_author();
	test_queries_are_normalized();
	test_searches_keep_only_the_latest_answer();
	test_choosing_asks_once_and_says_what_came_of_it();
	test_the_provider_decides_before_writing();
	std::cout << "reviewer request core tests passed\n";
	return EXIT_SUCCESS;
}
