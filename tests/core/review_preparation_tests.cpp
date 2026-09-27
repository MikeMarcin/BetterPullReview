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
	std::cout << "review-preparation:pass\n";
}
