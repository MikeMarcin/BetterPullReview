#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_preparation.hpp"
#include <cstdlib>
#include <iostream>
namespace {
	using namespace bpr;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
	auto target() -> core::writing_target
	{
		const auto tour = tests::review_tour_fixture();
		return { .Owner = tour.Owner, .Draft = tour.Id, .Window = { tour.Owner.Profile, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } }, .Session = core::application_session_id{ 1 }, .Agent = core::agent_profile_id{ 2 }, .HookRevision = 1, .TextGeneration = 1, .EditorGeneration = 1 };
	}
}
int main()
{
	using namespace bpr;
	core::scoped_writing_state state;
	auto                       identity = target();
	const std::string          raw      = "**👩‍💻 café é**\r\n\tKeep.\n";
	for (std::size_t begin = 0; begin < raw.size(); ++begin) {
		for (std::size_t end = begin + 1; end <= raw.size(); ++end) {
			const auto request = state.begin(identity, raw, { begin, end, 0, 1, 3, 1 });
			const bool valid   = core::writing_boundary(raw, begin) && core::writing_boundary(raw, end);
			expect(request.has_value() == valid, "only valid UTF8/CRLF spans admitted");
			if (!request) {
				continue;
			}
			const auto edit = state.complete(request->Id, identity, raw, "replacement\r\n😀");
			expect(edit.has_value(), "bounded valid result applies");
			expect(edit->Text == raw.substr(0, begin) + "replacement\r\n😀" + raw.substr(end), "property: exact prefix replacement suffix");
			expect(!state.complete(request->Id, identity, raw, "again"), "duplicate cannot insert twice");
		}
	}
	auto r = state.begin(identity, " \t", { 0, 2, 2, 0, 1, 1 });
	expect(r && r->Scope == core::writing_scope::selection, "whitespace selection wins");
	r = state.begin(identity, " \t ", { });
	expect(r && r->Scope == core::writing_scope::empty_field, "Unicode whitespace empty drafting");
	r = state.begin(identity, raw, { 2, 2, 1, 1, 1, 0 });
	expect(r && r->Scope == core::writing_scope::field && r->Selection.Begin == 0 && r->Selection.End == raw.size(), "caret uses whole field");
	expect(!state.complete(r->Id, identity, raw, raw) && !state.pending(), "equal result consumes without edit");
	for (unsigned field = 0; field < 11; ++field) {
		r            = state.begin(identity, raw, { });
		auto changed = identity;
		switch (field) {
		case 0:  ++changed.TextGeneration; break;
		case 1:  ++changed.EditorGeneration; break;
		case 2:  ++changed.HookRevision; break;
		case 3:  changed.Draft = core::tour_draft_id{ 2 }; break;
		case 4:  changed.Stop = core::tour_stop_id{ 1 }; break;
		case 5:  changed.Owner.Account = core::github_account_id{ 2 }; break;
		case 6:  changed.Owner.PullRequest = core::pull_request_id{ 2 }; break;
		case 7:  changed.Window.Generation = core::bpr_window_generation{ 2 }; break;
		case 8:  changed.ContextDigest[0] = 1; break;
		case 9:  changed.Agent = core::agent_profile_id{ 3 }; break;
		default: changed.Session = core::application_session_id{ 2 };
		}
		expect(!state.complete(r->Id, changed, raw, "new"), "changed identity refuses even undo back to exact bytes");
	}
	auto older = state.begin(identity, raw, { });
	auto newer = state.begin(identity, raw, { });
	expect(!state.complete(older->Id, identity, raw, "old") && state.pending()->Id == newer->Id, "old result cannot retire new request");
	expect(state.complete(newer->Id, identity, raw, "new").has_value(), "new response wins");
	for (const auto& bad : { std::string("\xff"), std::string("\xc0\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\0", 1), std::string(65537, 'x') }) {
		r = state.begin(identity, raw, { });
		expect(!state.complete(r->Id, identity, raw, bad), "malformed and oversized output refused");
	}
	expect(!state.begin(identity, raw, { 0, 1, 0, 1, 1, 2 }), "disjoint selection refused");
	r = state.begin(identity, raw, { });
	state.cancel();
	expect(!state.complete(r->Id, identity, raw, "late"), "cancel retires authority");
	identity.Hook = core::writing_hook::outline;
	r             = state.begin(identity, { }, { });
	expect(r && !state.complete(r->Id, identity, { }, "body"), "outline capture can never return a prose replacement");
	r                   = state.begin(identity, { }, { });
	auto changedOutline = identity;
	++changedOutline.TextGeneration;
	expect(!state.complete_outline(r->Id, changedOutline), "changed draft generation refuses outline");
	r = state.begin(identity, { }, { });
	expect(state.complete_outline(r->Id, identity) && !state.complete_outline(r->Id, identity), "outline result is consumed once for its exact identity");
	// [[BPR-1064]]. Exact quoted passages and the shared splice, independent of any presentation.
	{
		const std::string text = "Alpha beta.\r\nbeta again \xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb end\n";
		const auto        once = core::writing_quote_range(text, "Alpha");
		expect(once && once->first == 0 && once->second == 5, "a unique quote resolves to its exact bytes");
		expect(core::writing_quote_range(text, "beta").error() == core::writing_refusal::ambiguous, "a repeated quote is ambiguous, never widened");
		expect(core::writing_quote_range(text, "missing").error() == core::writing_refusal::not_found, "an absent quote is not found");
		expect(core::writing_quote_range(text, "").error() == core::writing_refusal::invalid_capture && core::writing_quote_range(text, "\xff").error() == core::writing_refusal::invalid_capture, "an empty or malformed quote is refused");
		expect(core::writing_quote_range(text, "\nbeta again").error() == core::writing_refusal::invalid_capture, "a quote that starts inside a CRLF pair is refused");
		const auto emoji = core::writing_quote_range(text, "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb");
		expect(emoji.has_value() && text.substr(emoji->first, emoji->second - emoji->first) == "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb", "a multi-scalar sequence resolves on scalar boundaries");
		expect(core::splice_writing_result(text, 0, 5, "Omega").value() == "Omega beta.\r\nbeta again \xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb end\n", "only the selected bytes change");
		expect(core::splice_writing_result(text, 0, 5, "Alpha").error() == core::writing_refusal::unchanged, "an identical replacement is a no-op");
		expect(core::splice_writing_result(text, 12, 12, "x").error() == core::writing_refusal::invalid_capture, "a splice inside a CRLF pair is refused");
		expect(core::splice_writing_result(text, 3, 2, "x").error() == core::writing_refusal::invalid_capture && core::splice_writing_result(text, 0, text.size() + 1, "x").error() == core::writing_refusal::invalid_capture, "an inverted or out-of-range span is refused");
		expect(core::splice_writing_result(text, 0, 5, "\xff").error() == core::writing_refusal::invalid_result && core::splice_writing_result(text, 0, 5, std::string(core::maximum_tour_field_bytes + 1U, 'x')).error() == core::writing_refusal::invalid_result, "malformed or oversized output is refused");
		expect(core::splice_writing_result("", 0, 0, "drafted").value() == "drafted", "an empty field can be drafted");
	}
	std::cout << "review-preparation:pass\n";
}
