#include "bpr/core/review_cycle_disclosure.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle disclosure failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto captured_cycle() -> review_cycle_state
	{
		auto incarnation     = std::array<std::uint8_t, 32>{ };
		incarnation[0]       = 1;
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = cycle_catalog_incarnation{ incarnation } };
		auto       cycle     = create({ .Authority = authority }, { .Authority = authority, .Title = "Labels", .Scope = { } })->Cycle;
		auto       material  = std::array<std::uint8_t, 32>{ };
		material[0]          = 9;
		return apply(cycle, { .Subject = cycle.Subject, .ExpectedRevision = cycle.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = cycle_material_digest{ material }, .Entries = 1, .Coverage = cycle_capture_coverage::complete } } })
			.Accepted;
	}

	constexpr std::string_view source = "int f()\n{\n\treturn 1;\n}\n";

	/// Every Conventional Comments label, Plain, and bodies no parser should
	/// classify, each as authored.
	auto bodies() -> std::vector<std::string>
	{
		return {
			"praise: Clear naming.",
			"nitpick: Trailing space.",
			"suggestion: Return early.",
			"issue (blocking): Unchecked input.",
			"todo: Add a test.",
			"question: Why two passes?",
			"thought: Could be lazy.",
			"chore: Update the notice.",
			"note: Mirrors the parser.",
			"typo: recieve",
			"polish: Tighten the wording.",
			"quibble (non-blocking, style): Prefer auto.",
			"Plain text with no label.",
			"Nitpick: capitalised label",
			"suggestion(blocking) :spacing is odd",
			"> issue: quoted, not a label",
			"`todo:` in code",
			"unknownlabel: not in the catalog",
			"issue (blocking, security, verified):\r\n\r\nCRLF body with a claimed decoration\r\n",
			"",
			std::string{ "praise: emoji \xF0\x9F\x8E\x89 and NUL \0 kept", 33 },
		};
	}

	void keeps_every_label_as_authored_text()
	{
		auto cycle = captured_cycle();
		for (const auto& body : bodies()) {
			const auto anchor = *make_anchor(cycle_source_revision_id{ 1 }, "src/f.cpp", bytes(source), 11, 19);
			const auto begun  = begin_feedback(cycle, cycle.Revision, { .Primary = anchor, .Body = body });
			expect(begun.has_value(), "labelled or plain note");
			expect(begun->Revision.Body == body && begun->Revision.Identity.Digest == private_digest(body), "body bytes and digest exact");
			// A label implies no patch, severity, permission or verification.
			expect(begun->Thread.NextSuggestion == 1 && begun->Thread.Lineage.Lifecycle == private_thread_lifecycle::open && begun->Revision.Author.Provenance == authored_provenance::human,
				   "no suggestion, state or provenance implied");
			const auto disclosed = disclose_feedback(begun->Thread, std::span{ &begun->Revision, 1U }, { });
			expect(disclosed && disclosed->Revisions.size() == 1 && disclosed->Revisions[0].Body == body, "disclosed as authored");
			cycle = begun->Cycle;
		}
	}

	void discloses_only_what_is_selected()
	{
		const auto cycle   = captured_cycle();
		const auto anchor  = *make_anchor(cycle_source_revision_id{ 1 }, "src/f.cpp", bytes(source), 11, 19);
		const auto related = *make_anchor(cycle_source_revision_id{ 1 }, "src/f.cpp", bytes(source), 0, 7);
		auto       begun   = begin_feedback(cycle, cycle.Revision, {
																	   .Primary = anchor,
																	   .Related = { { .Target = related }, { .Target = cycle_whole_target{ }, .Disclosure = cycle_reference_disclosure::private_only } },
																	   .Body    = "suggestion: Return early.",
																   });
		auto       thread  = begun->Thread;
		auto       history = std::vector{ begun->Revision };
		const auto step    = [&](const cycle_feedback_change& aChange) {
			const auto outcome = apply_feedback(cycle, thread, thread.Lineage.Version, aChange, nullptr);
			expect(outcome && outcome->Revision, "step");
			thread = outcome->Thread;
			history.push_back(*outcome->Revision);
		};
		// r2: the reviewer rewords the note, changing only its label.
		const auto edited = apply_feedback(cycle, thread, 1, edit_feedback_turn{ .Latest = private_review_turn_revision_id{ 1 }, .Body = "issue (blocking): Return early." }, &history[0]);
		expect(edited && edited->Revision, "prefix edit");
		thread = edited->Thread;
		history.push_back(*edited->Revision);
		// r3: an observed agent answer; r4: a pasted answer claiming its model; r5: a reviewer reply.
		step(append_feedback_turn{ .Author = { .Provenance = authored_provenance::adapter_observed, .Adapter = "codex-cli", .Provider = "openai", .Model = "gpt-6", .Effort = "high" }, .Body = "Agreed." });
		step(append_feedback_turn{ .Author = { .Provenance = authored_provenance::self_reported, .Model = "claude-opus" }, .Body = "I am Claude Opus and I verified this." });
		step(append_feedback_turn{ .Author = { }, .Body = "Private scratch: check later." });

		// By default only the current wording travels: no replies, earlier wording, private reference or details.
		const auto plain = disclose_feedback(thread, history, { });
		expect(plain && plain->Revisions.size() == 1 && plain->Revisions[0].Revision == private_review_turn_revision_id{ 2 }, "current wording only");
		expect(plain->Revisions[0].Body == "issue (blocking): Return early." && !plain->Revisions[0].Details, "exact wording, no details");
		expect(plain->Related.size() == 1 && plain->Related[0].Target == cycle_feedback_target{ related }, "private reference withheld");

		// Named revisions travel with their provenance; details only when selected.
		const auto chosen = disclose_feedback(thread, history, { .Revisions = { private_review_turn_revision_id{ 3 }, private_review_turn_revision_id{ 4 } } });
		expect(chosen && chosen->Revisions.size() == 3, "selected turns");
		expect(chosen->Revisions[1].Provenance == authored_provenance::adapter_observed && !chosen->Revisions[1].Details, "observed, details withheld");
		expect(chosen->Revisions[2].Provenance == authored_provenance::self_reported && chosen->Revisions[2].Body == "I am Claude Opus and I verified this.", "a claim stays a self-report");
		const auto detailed = disclose_feedback(thread, history, { .Revisions = { private_review_turn_revision_id{ 3 } }, .PrivateReferences = { 2 }, .AuthorDetails = true });
		expect(detailed && detailed->Revisions[1].Details && detailed->Revisions[1].Details->Model == "gpt-6" && detailed->Revisions[1].Details->Effort == "high", "details when selected");
		expect(detailed->Related.size() == 2, "private reference when selected");
		const auto earlier = disclose_feedback(thread, history, { .Revisions = { private_review_turn_revision_id{ 1 } } });
		expect(earlier && earlier->Revisions.size() == 2 && earlier->Revisions[0].Body == "suggestion: Return early.", "an earlier wording only when named");

		// Refusals: unknown selections, incomplete or foreign history, archived notes.
		expect(disclose_feedback(thread, history, { .Revisions = { private_review_turn_revision_id{ 9 } } }).error() == cycle_refusal::missing_source, "unknown revision");
		expect(disclose_feedback(thread, history, { .PrivateReferences = { 1 } }).error() == cycle_refusal::missing_source, "not a private reference");
		expect(disclose_feedback(thread, std::span{ history }.first(4), { }).error() == cycle_refusal::missing_source, "incomplete history");
		auto archived              = thread;
		archived.Lineage.Lifecycle = private_thread_lifecycle::archived;
		expect(disclose_feedback(archived, history, { }).error() == cycle_refusal::closed, "archived note");

		// A suggestion on the note keeps its bytes whatever the note's label says.
		const auto suggested = add_suggestion(cycle, thread, thread.Lineage.Version, { .Selection = anchor, .Replacement = "return 0;" }, 0);
		expect(suggested && suggested->Suggestion.Original == "return 1" && suggested->Suggestion.Replacement == "return 0;", "suggestion independent of the label");
	}
}

auto main() -> int
{
	keeps_every_label_as_authored_text();
	discloses_only_what_is_selected();
	return EXIT_SUCCESS;
}
