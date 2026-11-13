/// \file
/// \brief RFC-BPR-0118 ([[BPR-2078]]). Suggest the change: its question rule
/// (the comment workshop's seventh operation on the draft's lines), the
/// material its one call carries, the structured reply, and every check a
/// reply passes before it is shown as a suggestion (P05). No Qt.

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/core/suggest_change.hpp"

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

	/// A JSON string literal of exact bytes.
	auto quoted(const std::string_view aText) -> std::string
	{
		auto out = std::string{ "\"" };
		for (const char ch : aText) {
			switch (ch) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:   out += ch; break;
			}
		}
		return out + "\"";
	}

	auto reply(const std::uint32_t aFirst, const std::uint32_t aLast, const std::string_view aCurrent, const std::string_view aReplacement) -> std::string
	{
		return "{\"first_line\": " + std::to_string(aFirst) + ", \"last_line\": " + std::to_string(aLast) + ", \"current\": " + quoted(aCurrent) + ", \"replacement\": " + quoted(aReplacement) + "}";
	}

	const auto head_text = std::string{ "int f(int* p)\n{\n\treturn *p;\n}\n\nint g() { return 1; }\n" };

	/// AC01. The press is the comment workshop's seventh operation, about
	/// the draft's lines, whatever else is attached.
	void the_press_is_the_seventh_operation()
	{
		expect(core::workshop_operation_count == 7U && core::stable_name(core::workshop_operation::suggest_change) == "suggest_change", "a seventh workshop operation, named");
		expect(!core::rewrites_prose(core::workshop_operation::suggest_change) && core::rewrites_prose(core::workshop_operation::check_the_premise), "it is not one of the six prose rewrites");
		expect(core::workshop_operation_for(core::agent_entry_point::workshop_comment, core::workshop_operation::suggest_change) == core::workshop_operation::suggest_change,
			   "the comment workshop entry point carries it");
		expect(!core::workshop_operation_for(core::agent_entry_point::ask_about_selection, core::workshop_operation::suggest_change), "no other entry point does");
		const auto question = core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true, .ConversationAttached = true, .Explain = { }, .SuggestChange = "src/a.cpp" });
		expect(question && question->EntryPoint == core::agent_entry_point::workshop_comment && question->Operation == core::workshop_operation::suggest_change
				   && question->Subject == core::agent_question_subject::lines,
			   "the press is a workshop_comment question about lines, with the operation");
		expect(!core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true })->Operation, "any other question carries no operation");
	}

	/// The material: exactly the lines, their words, and the lines around them.
	void the_material_is_exact_and_bounded()
	{
		const auto head     = core::split_text_lines(head_text);
		const auto material = core::bound_suggest_change_material({ .Path = "src/a.cpp", .HeadCommit = std::string(40U, 'a'), .Comment = "Check for null first.", .Head = head, .First = 2U, .Last = 2U });
		expect(material && material->FirstLine == 3U && material->LastLine == 3U && material->Lines == "\treturn *p;", "the one line, one-based, without its ending");
		expect(material->Before == "int f(int* p)\n{\n" && material->After == "\n}\n\nint g() { return 1; }\n", "the lines around it, exact");
		expect(core::valid(*material), "within its bounds");
		expect(!core::bound_suggest_change_material({ .Path = "src/a.cpp", .HeadCommit = "a", .Head = head, .First = 2U, .Last = 9U }), "a range outside the head is refused");
		expect(!core::bound_suggest_change_material({ .Path = { }, .HeadCommit = "a", .Head = head, .First = 0U, .Last = 0U }), "no path, no material");
		const auto crlf = core::split_text_lines("a\r\nb\r\nc\r\n");
		expect(core::suggest_lines_text(std::span{ crlf }.subspan(0U, 2U)) == "a\r\nb", "a CRLF range keeps the CR between its lines");
		// The material rides only with its own operation.
		auto request          = core::agent_supplement_request{ };
		request.SuggestChange = material;
		expect(!core::build_agent_supplement(request), "material without the operation is refused");
	}

	/// The reply is one JSON object, alone or fenced.
	void the_reply_is_structured()
	{
		const auto plain = core::read_suggested_replacement(reply(3U, 3U, "\treturn *p;", "\treturn p ? *p : 0;"));
		expect(plain && plain->FirstLine == 3U && plain->Current == "\treturn *p;" && plain->Replacement == "\treturn p ? *p : 0;", "a bare object reads");
		const auto fenced = core::read_suggested_replacement("```json\n" + reply(3U, 3U, "x", "y") + "\n```\n");
		expect(fenced && fenced->Replacement == "y", "an object in one fenced block reads");
		expect(!core::read_suggested_replacement("Here it is: " + reply(3U, 3U, "x", "y")), "prose around it does not");
		expect(!core::read_suggested_replacement(R"({"first_line": 3, "last_line": 3, "current": "x"})"), "a missing field does not");
		expect(!core::read_suggested_replacement(R"({"first_line": 3, "last_line": 3, "current": "x", "replacement": "y", "why": "z"})"), "an unknown field does not");
		expect(!core::read_suggested_replacement(R"({"first_line": 0, "last_line": 3, "current": "x", "replacement": "y"})"), "line zero does not");
		expect(!core::read_suggested_replacement(R"({"first_line": "3", "last_line": 3, "current": "x", "replacement": "y"})"), "a mistyped field does not");
		expect(!core::read_suggested_replacement("I cannot write that change."), "a sentence does not");
	}

	auto decide(const std::string& aAnswer, const std::uint32_t aFirst, const std::uint32_t aLast, const std::string_view aNow = head_text, const bool aSameHead = true)
		-> core::suggest_change_decision
	{
		static auto asked = std::vector<core::text_line>{ };
		static auto now   = std::vector<core::text_line>{ };
		const auto  head  = core::split_text_lines(head_text);
		asked             = std::vector<core::text_line>{ head.begin() + aFirst, head.begin() + aLast + 1U };
		now               = core::split_text_lines(aNow);
		return core::decide_suggested_change({ .Answer = aAnswer, .First = aFirst, .Last = aLast, .Asked = asked, .SameHead = aSameHead, .Head = now, .Prose = "Check for null first." });
	}

	/// AC02, AC03. Every check, and the order they are made in.
	void every_check_is_made()
	{
		const auto accepted = decide(reply(3U, 3U, "\treturn *p;", "\tif (!p) {\n\t\treturn 0;\n\t}\n\treturn *p;"), 2U, 2U);
		expect(accepted.accepted() && accepted.Block.size() == 4U && accepted.Span.size() == 4U && accepted.Span.back().Content == "\treturn *p;"
				   && accepted.Span.back().Ending == core::text_line_ending::lf,
			   "a reply that matches the head's lines is accepted, its lines ready for the edit");
		expect(decide(reply(2U, 3U, "\treturn *p;", "x"), 2U, 2U).Refusal == core::suggest_change_refusal::range_differs, "a differing range is refused");
		expect(decide(reply(3U, 3U, "\treturn *p; ", "x"), 2U, 2U).Refusal == core::suggest_change_refusal::current_differs, "one differing byte in the current text is refused");
		expect(decide(reply(3U, 3U, "\treturn *p;\n", "x"), 2U, 2U).Refusal == core::suggest_change_refusal::current_differs, "the last line's ending is not in the current text");
		expect(decide(reply(3U, 3U, "\treturn *p;", "\treturn *p;"), 2U, 2U).Refusal == core::suggest_change_refusal::identical, "an identical replacement is refused");
		expect(decide(reply(3U, 3U, "\treturn *p;", "\treturn *p;\n"), 2U, 2U).Refusal == core::suggest_change_refusal::identical, "a final line ending alone changes nothing");
		expect(decide(reply(3U, 3U, "\treturn *p;", std::string(core::maximum_suggested_replacement_bytes + 1U, 'x')), 2U, 2U).Refusal == core::suggest_change_refusal::too_large,
			   "an out-of-bounds replacement is refused");
		expect(decide(R"({"first_line": 3, "last_line": 3, "current": "\treturn *p;", "replacement": "a\u0000b"})", 2U, 2U).Refusal == core::suggest_change_refusal::invalid_text,
			   "a replacement that is not text is refused");
		expect(decide("No change is needed here.", 2U, 2U).Refusal == core::suggest_change_refusal::unreadable, "an answer with no reply is refused, to be read as text");
		// A head that moved the lines while the run was under way.
		const auto moved = std::string{ "// header\n" } + head_text;
		expect(decide(reply(3U, 3U, "\treturn *p;", "x"), 2U, 2U, moved).Refusal == core::suggest_change_refusal::head_moved, "lines that moved are refused, never re-anchored");
		expect(decide(reply(3U, 3U, "\treturn *p;", "x"), 2U, 2U, head_text, false).Refusal == core::suggest_change_refusal::head_moved, "a new head is refused");
		expect(decide(reply(2U, 3U, "x", "y"), 2U, 2U, moved).Refusal == core::suggest_change_refusal::head_moved, "the head is the first check");
		// An empty replacement deletes the lines.
		const auto deletion = decide(reply(3U, 3U, "\treturn *p;", ""), 2U, 2U);
		expect(deletion.accepted() && deletion.Block.empty(), "an empty replacement is a deletion");
	}

	/// AC03. A change GitHub could not take as a suggestion here.
	void a_change_github_cannot_take_is_refused()
	{
		const auto head  = core::split_text_lines(head_text);
		const auto asked = std::vector<core::text_line>{ head.begin() + 2, head.begin() + 3 };
		const auto answer = reply(3U, 3U, "\treturn *p;", "\treturn p ? *p : 0;");
		const auto input  = core::suggest_change_input{
			.Answer   = answer,
			.First    = 2U,
			.Last     = 2U,
			.Asked    = asked,
			.Head     = head,
			.Prose    = "Check for null first.",
			.Filtered = core::filtered_comment_target{ .Placement = core::filtered_comment_placement::refused, .Refusal = core::filtered_comment_refusal::lines_pending },
		};
		const auto refused = core::decide_suggested_change(input);
		expect(refused.Refusal == core::suggest_change_refusal::not_suggestible, "a line GitHub places nowhere takes no suggestion");
	}

	/// CRLF files: a CR before each break is the line's ending.
	void crlf_lines_keep_their_endings()
	{
		const auto head  = core::split_text_lines("a\r\nb\r\nc\r\n");
		const auto asked = std::vector<core::text_line>{ head.begin(), head.begin() + 2 };
		const auto answer = reply(1U, 2U, "a\r\nb", "x\r\ny\r\nz");
		const auto made   = core::decide_suggested_change({ .Answer = answer, .First = 0U, .Last = 1U, .Asked = asked, .Head = head, .Prose = "" });
		expect(made.accepted() && made.Block == std::vector<std::string>{ "x", "y", "z" }, "the block's lines carry no CR");
		expect(made.Span.size() == 3U && made.Span[0].Ending == core::text_line_ending::crlf && made.Span[2].Ending == core::text_line_ending::crlf, "the edit's lines keep the file's endings");
		const auto plan = core::plan_pull_request_suggestion({ .First = 0U, .Last = 1U, .Original = asked, .Edited = made.Span, .Prose = "" });
		expect(plan.Changed && plan.Block == made.Block, "and the plan RFC-BPR-0090 makes of them is the same block");
	}
} // namespace

auto main() -> int
{
	the_press_is_the_seventh_operation();
	the_material_is_exact_and_bounded();
	the_reply_is_structured();
	every_check_is_made();
	a_change_github_cannot_take_is_refused();
	crlf_lines_keep_their_endings();
	std::cout << "suggest change core tests passed\n";
	return EXIT_SUCCESS;
}
