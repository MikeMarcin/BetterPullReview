// [[BPR-1077]]. Explicit Conventional Comments header edits bound to the exact
// body revision (RFC-BPR-0047 R05, R41), driven by the pack's
// header-edit-cases.json, composed with the real revisioned draft and tour-stop
// owners, and checked over a generated corpus.

#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback header edit failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(static_cast<bool>(stream), "pack file readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto member(const cycle_json_value::object& aObject, std::string_view aKey) -> const cycle_json_value*
	{
		return find_member(aObject, aKey);
	}

	auto text(const cycle_json_value& aValue) -> std::string
	{
		const auto* found = std::get_if<std::string>(&aValue.Value);
		expect(found != nullptr, "string field");
		return *found;
	}

	auto digest(std::string_view aBody) -> std::array<std::uint8_t, 32>
	{
		return sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() });
	}

	auto request(std::string_view aBody, feedback_header_edit aEdit, feedback_body_segment aSegment = { }) -> feedback_header_edit_request
	{
		return { .Body = aBody, .Segment = aSegment, .ExpectedDigest = digest(aBody), .Edit = aEdit };
	}

	auto set(feedback_label aLabel, std::optional<feedback_expectation> aExpectation = std::nullopt) -> feedback_header_edit
	{
		return { .Change = feedback_label_change::set, .Label = aLabel, .Expectation = aExpectation };
	}
	auto expect_only(feedback_expectation aExpectation) -> feedback_header_edit
	{
		return { .Expectation = aExpectation };
	}
	auto removal() -> feedback_header_edit
	{
		return { .Change = feedback_label_change::remove };
	}

	auto continuation(std::string_view aBody, std::size_t aAt) -> bool
	{
		return aAt < aBody.size() && (static_cast<unsigned char>(aBody[aAt]) & 0xC0U) == 0x80U;
	}

	/// What every decision holds: a change is exactly one replaced span of the
	/// old body, on code point boundaries, inside the user's text, and the
	/// recorded annotation is the new body's; anything else changes nothing.
	void expect_invariants(const feedback_header_edit_request& aRequest, const feedback_header_edit_decision& aDecision)
	{
		const auto old = aRequest.Body;
		if (!aDecision.Changed) {
			expect(aDecision.Body.empty() && aDecision.Replaced == feedback_span{ } && aDecision.Replacement.empty(), "no change carries no bytes");
			expect(aDecision.Segment == aRequest.Segment, "no change keeps the segment");
			return;
		}
		expect(aDecision.Refusal == feedback_edit_refusal::none, "a change is never a refusal");
		const auto replaced = aDecision.Replaced;
		expect(replaced.Begin <= replaced.End && replaced.End <= old.size(), "replaced span inside the old body");
		expect(!continuation(old, replaced.Begin) && !continuation(old, replaced.End), "replaced span on code point boundaries");
		expect(aDecision.Body == std::string{ old.substr(0U, replaced.Begin) } + aDecision.Replacement + std::string{ old.substr(replaced.End) }, "body is the old body with one span replaced");
		expect(aDecision.Body.size() <= maximum_shared_draft_body_bytes, "result within the draft bound");
		expect(aDecision.Annotation.BodyDigest == digest(aDecision.Body), "annotation of the new body");
		if (aRequest.Segment.Managed) {
			const auto before = *aRequest.Segment.UserBody;
			const auto after  = *aDecision.Segment.UserBody;
			expect(before.Begin <= replaced.Begin && replaced.End <= before.End, "edit inside the owner's segment");
			expect(after.Begin == before.Begin && after.End + replaced.End == before.End + replaced.Begin + aDecision.Replacement.size(), "segment follows the edit");
		}
	}

	auto decide(const feedback_header_edit_request& aRequest) -> feedback_header_edit_decision
	{
		auto decision = decide_feedback_header_edit(aRequest);
		expect_invariants(aRequest, decision);
		const auto again = decide_feedback_header_edit(aRequest);
		expect(again.Refusal == decision.Refusal && again.Body == decision.Body && again.Replaced == decision.Replaced, "deterministic");
		return decision;
	}

	auto applied(std::string_view aBody, feedback_header_edit aEdit) -> std::string
	{
		const auto decision = decide(request(aBody, aEdit));
		expect(decision.Refusal == feedback_edit_refusal::none, aBody);
		return decision.Changed ? decision.Body : std::string{ aBody };
	}

	auto refusal(std::string_view aBody, feedback_header_edit aEdit) -> feedback_edit_refusal
	{
		const auto decision = decide(request(aBody, aEdit));
		expect(!decision.Changed, "refused, changes nothing: " + std::string{ aBody.substr(0U, 40U) });
		return decision.Refusal;
	}

	// The pack's normative edit cases, through the real decision.
	auto pack_cases(const std::filesystem::path& aPack) -> std::size_t
	{
		const auto parsed = parse_cycle_json(read_file(aPack / "fixtures" / "header-edit-cases.json"), 1U << 20U);
		expect(parsed.has_value(), "header-edit-cases.json parses");
		const auto& root = std::get<cycle_json_value::object>(parsed->Value);
		expect(text(*member(root, "schema")) == "bpr.cc.header-edit-cases.v1", "case schema");
		const auto& cases = std::get<cycle_json_value::array>(member(root, "cases")->Value);
		for (const auto& entry : cases) {
			const auto&          item      = std::get<cycle_json_value::object>(entry.Value);
			const auto           id        = text(*member(item, "id"));
			const auto           before    = text(*member(item, "before"));
			const auto           after     = text(*member(item, "after"));
			const auto&          operation = std::get<cycle_json_value::object>(member(item, "operation")->Value);
			feedback_header_edit edit;
			if (const auto* label = member(operation, "label")) {
				if (std::holds_alternative<std::monostate>(label->Value)) {
					edit.Change = feedback_label_change::remove;
				} else {
					const auto found = find_feedback_label(text(*label));
					expect(found.has_value(), id + ": operation label in the catalog");
					edit = set(*found);
				}
			}
			if (const auto* expectation = member(operation, "expectation")) {
				const auto name = text(*expectation);
				expect(name == "non_blocking", id + ": expectation the fixture uses");
				edit.Expectation = feedback_expectation::non_blocking;
			}
			if (const auto* kind = member(operation, "kind")) {
				expect(text(*kind) == "parse_preview_copy_only", id + ": parse-only kind");
				const auto annotation = recognize_feedback_prefix(before);
				expect(annotation.BodyDigest == digest(after) && before == after, id + ": parse leaves the bytes");
			}
			const auto decision = decide(request(before, edit));
			expect(decision.Refusal == feedback_edit_refusal::none, id + ": accepted");
			expect((decision.Changed ? decision.Body : before) == after, id + ": exact bytes after the edit");
			if (decision.Changed) {
				expect(decision.Replaced.End <= before.size() && before.substr(decision.Replaced.End) == after.substr(after.size() - (before.size() - decision.Replaced.End)), id + ": every byte after the header kept");
			}
		}
		return cases.size();
	}

	// Explicit edits in each form, keeping custom decorations in place.
	void decoration_and_form_edits()
	{
		expect(applied("issue (security, blocking, ux): x", expect_only(feedback_expectation::non_blocking)) == "issue (security, non-blocking, ux): x", "standard token replaced in place");
		expect(applied("issue (security, blocking, ux): x", expect_only(feedback_expectation::unspecified)) == "issue (security, ux): x", "standard token removed, custom kept");
		expect(applied("issue (blocking): x", expect_only(feedback_expectation::unspecified)) == "issue: x", "empty list drops its parentheses");
		expect(applied("issue (security): x", expect_only(feedback_expectation::if_minor)) == "issue (security, if-minor): x", "new standard token appended");
		expect(applied("issue: x", expect_only(feedback_expectation::blocking)) == "issue (blocking): x", "list added");
		expect(applied("issue (blocking, non-blocking): x", expect_only(feedback_expectation::blocking)) == "issue (blocking): x", "conflict resolved by an explicit edit only");
		expect(applied("issue (blocking,security):\tx", set(feedback_label::todo)) == "todo (blocking, security): x", "explicitly edited header is canonical");
		expect(applied("**issue (blocking):** x", set(feedback_label::suggestion)) == "**suggestion (blocking):** x", "strong header form kept");
		expect(applied("**Issue** (blocking): x", expect_only(feedback_expectation::non_blocking)) == "**Issue** (non-blocking): x", "strong label form and spelling kept");
		expect(applied("ISSUE: x", set(feedback_label::question)) == "question: x", "new label written lowercase");
		expect(applied("observation (ux): x", set(feedback_label::note)) == "note (ux): x", "custom label replaced by a catalog label");
		expect(applied("observation: x", expect_only(feedback_expectation::blocking)) == "observation (blocking): x", "custom label keeps its text");
		expect(applied("\xEF\xBB\xBF\r\n\r\n  issue: x\r\n", removal()) == "\xEF\xBB\xBF\r\n\r\n  x\r\n", "leading BOM, blank lines and indent kept on removal");
		expect(applied("issue (blocking): x", removal()) == "x", "removal takes the decorations with the header");
		expect(applied("observation: x", removal()) == "x", "a custom header can be removed");

		// Requests that change nothing keep every byte, canonical or not.
		const auto unchanged = [](std::string_view aBody, feedback_header_edit aEdit) {
			const auto decision = decide(request(aBody, aEdit));
			expect(decision.Refusal == feedback_edit_refusal::none && !decision.Changed, aBody);
		};
		unchanged("Issue (blocking,security):  x", { });
		unchanged("Issue (blocking,security):  x", set(feedback_label::issue));
		unchanged("Issue (blocking,security):  x", expect_only(feedback_expectation::blocking));
		unchanged("**issue:** x", set(feedback_label::issue));
		unchanged("**issue:** x", expect_only(feedback_expectation::unspecified));
		unchanged("issue (blocking, blocking): x", expect_only(feedback_expectation::blocking));
		unchanged("Plain text.\r\n", { });
		unchanged("Plain text.\r\n", removal());
		unchanged("issue (blocking : x", { });

		expect(refusal("issue: x", expect_only(feedback_expectation::conflicting)) == feedback_edit_refusal::invalid_edit, "conflicting is not a target");
		expect(refusal("issue: x", { .Change = feedback_label_change::remove, .Expectation = feedback_expectation::blocking }) == feedback_edit_refusal::invalid_edit, "removal with decorations");
		expect(refusal("Plain text.", expect_only(feedback_expectation::blocking)) == feedback_edit_refusal::invalid_edit, "decorations need a header");
		expect(refusal("issue: note: x", removal()) == feedback_edit_refusal::restructuring_required, "removal never exposes a second label");
		expect(refusal("issue (blocking : x", set(feedback_label::todo)) == feedback_edit_refusal::unsupported_header, "malformed header left to the author");
		expect(refusal(std::string(1100U, '\n') + "issue: x", set(feedback_label::todo)) == feedback_edit_refusal::unsupported_header, "limited body left to the author");
	}

	auto decorate(std::string aToken, bool aWritten = true, feedback_label_change aChange = feedback_label_change::keep) -> feedback_header_edit
	{
		return { .Change = aChange, .Label = feedback_label::issue, .Decoration = feedback_decoration_change{ .Token = std::move(aToken), .Written = aWritten } };
	}

	// [[BPR-1083]] v004. Decorations combine, as Conventional Comments allows
	// ("issue (non-blocking, if-minor):"), and custom ones are an
	// organisation's own: one decoration goes in or out, and every other
	// token stays exactly as the author wrote it.
	void combinable_decorations()
	{
		expect(applied("issue (non-blocking): x", decorate("if-minor")) == "issue (non-blocking, if-minor): x", "if-minor combines with a weight");
		expect(applied("issue (non-blocking, if-minor): x", decorate("if-minor", false)) == "issue (non-blocking): x", "if-minor taken out alone");
		expect(applied("issue (non-blocking, if-minor): x", decorate("blocking")) == "issue (if-minor, blocking): x", "blocking takes the place of non-blocking, if-minor stays");
		expect(applied("issue (Blocking, ux): x", decorate("non-blocking")) == "issue (ux, non-blocking): x", "non-blocking takes out blocking in any case");
		expect(applied("issue (Security, if-minor): x", decorate("perf_2")) == "issue (Security, if-minor, perf_2): x", "a custom decoration is appended as typed, others kept as written");
		expect(applied("issue (Security, if-minor, security): x", decorate("SECURITY", false)) == "issue (if-minor): x", "taking out a custom one removes every copy, case-insensitively");
		expect(applied("issue (if-minor): x", decorate("If-Minor", false)) == "issue: x", "a standard one is matched by kind; an empty list drops its parentheses");
		expect(applied("**Issue** (ux): x", decorate("blocking")) == "**Issue** (ux, blocking): x", "the form and the label's spelling are kept");
		expect(applied("observation (ux): x", decorate("if-minor")) == "observation (ux, if-minor): x", "an unknown label takes decorations too");
		expect(applied("Plain text.", decorate("security", true, feedback_label_change::set)) == "issue (security): Plain text.", "a label set with a custom decoration");
		expect(applied("question: x", decorate("non-blocking", true, feedback_label_change::set)) == "issue (non-blocking): x", "a relabel with a decoration");

		const auto unchanged = [](std::string_view aBody, feedback_header_edit aEdit) {
			const auto decision = decide(request(aBody, std::move(aEdit)));
			expect(decision.Refusal == feedback_edit_refusal::none && !decision.Changed, aBody);
		};
		unchanged("issue (NON-BLOCKING, If-Minor): x", decorate("if-minor"));
		unchanged("issue (security): x", decorate("Security"));
		unchanged("issue (security): x", decorate("if-minor", false));

		auto both        = decorate("if-minor");
		both.Expectation = feedback_expectation::blocking;
		expect(refusal("issue: x", both) == feedback_edit_refusal::invalid_edit, "a decoration and an expectation are not one edit");
		expect(refusal("issue: x", decorate("if-minor", true, feedback_label_change::remove)) == feedback_edit_refusal::invalid_edit, "removal with a decoration");
		expect(refusal("Plain text.", decorate("if-minor")) == feedback_edit_refusal::invalid_edit, "a decoration needs a header");
		for (const auto* token : { "", "two words", "a,b", "(x)", "caf\xC3\xA9", "x:" }) {
			expect(refusal("issue: x", decorate(token)) == feedback_edit_refusal::invalid_edit, token);
			expect(!valid_feedback_decoration(token), token);
		}
		expect(valid_feedback_decoration(std::string(maximum_feedback_label_bytes, 'a')) && !valid_feedback_decoration(std::string(maximum_feedback_label_bytes + 1U, 'a')), "decorations are bounded like labels");
		expect(valid_feedback_decoration("non-blocking") && valid_feedback_decoration("perf_2") && valid_feedback_decoration("UX"), "the recognizer's token");

		// Several decorations read back as the recognizer reports them; the
		// grammar still calls two distinct standard ones `conflicting`, and a
		// mark reads the written kinds instead (delivery contract).
		const auto written = applied(applied(applied("issue: x", decorate("non-blocking")), decorate("if-minor")), decorate("security"));
		expect(written == "issue (non-blocking, if-minor, security): x", "three decorations written one at a time");
		const auto read = recognize_feedback_prefix(written);
		expect(read.Status == feedback_prefix_status::recognized && read.Decorations.size() == 3U && read.Decorations[0].Kind == feedback_decoration_kind::non_blocking
				   && read.Decorations[1].Kind == feedback_decoration_kind::if_minor && read.Decorations[2].Kind == feedback_decoration_kind::custom
				   && read.Expectation == feedback_expectation::conflicting,
			   "multiple and custom decorations recognized, each kept");
	}

	// Adding a label to a plain body writes before its first line, and never
	// turns another block into a paragraph to make room.
	void plain_insertion()
	{
		expect(applied("Leave this body exactly as written.\r\n", set(feedback_label::issue)) == "issue: Leave this body exactly as written.\r\n", "inserted before the first line");
		expect(applied("\xEF\xBB\xBF\n\n  Keep.\n", set(feedback_label::todo, feedback_expectation::blocking)) == "\xEF\xBB\xBF\n\n  todo (blocking): Keep.\n", "after BOM, blank lines and indent");
		expect(applied("\xC3\x89t\xC3\xA9 is wrong.", set(feedback_label::typo)) == "typo: \xC3\x89t\xC3\xA9 is wrong.", "multibyte first character");
		for (const std::string_view body : { "#123 regressed.", "**Bold** start.", "`code` here.", "1.5 is wrong.", "-1 is wrong.", "[link](https://example.com) is stale.", "Summary:\n- one", "Title\n=====\n" }) {
			const auto decision = decide(request(body, set(feedback_label::question)));
			expect(decision.Changed && decision.Body == "question: " + std::string{ body }, body);
		}
		for (const std::string_view body : { "> quoted", "- item", "* item", "+ item", "1. item", "12) item", "# Heading", "###### Heading", "```\ncode\n```", "~~~\ncode", "<div>x</div>", "| a | b |", "    indented", "\tindented", "***", "- - -", "___", "[ref]: https://example.com", "", "\n \r\n\t\n" }) {
			expect(refusal(body, set(feedback_label::question)) == feedback_edit_refusal::restructuring_required, body);
		}
		expect(refusal(std::string(1019U, '\n') + "Text.", set(feedback_label::issue)) == feedback_edit_refusal::restructuring_required, "a header past the classifier budget is refused");
		const auto large = std::string(maximum_shared_draft_body_bytes - 3U, 'x');
		expect(refusal(large, set(feedback_label::issue)) == feedback_edit_refusal::body_limit, "result over the draft bound refused");
		expect(applied(std::string(maximum_shared_draft_body_bytes - 7U, 'x'), set(feedback_label::issue)).size() == maximum_shared_draft_body_bytes, "result at the draft bound accepted");
	}

	// Every catalog label, round trip in each form and through removal.
	auto round_trips() -> std::size_t
	{
		std::size_t                           count = 0U;
		const std::array<std::string_view, 3> forms{ "%L%D: ", "**%L%D:** ", "**%L**%D: " };
		const std::array<std::string_view, 3> decorations{ "", " (blocking)", " (security, if-minor)" };
		const std::array<std::string_view, 3> tails{ "Subject caf\xC3\xA9_\xE6\x9D\xB1\xE4\xBA\xAC.\r\n\r\n```suggestion:-1+0\r\n\tcopy_n(src, n, dst);\r\n```\r\n", "Subject.\n\n```suggestion\n```\n", "Subject only" };
		const auto                            render = [](std::string_view aForm, std::string_view aLabel, std::string_view aDecorations) {
			std::string result;
			for (std::size_t at = 0U; at < aForm.size(); ++at) {
				if (aForm.substr(at, 2U) == "%L") {
					result += aLabel;
					++at;
				} else if (aForm.substr(at, 2U) == "%D") {
					result += aDecorations;
					++at;
				} else {
					result += aForm[at];
				}
			}
			return result;
		};
		for (const auto& from : feedback_label_catalog()) {
			for (const auto& to : feedback_label_catalog()) {
				for (std::size_t form = 0U; form < forms.size(); ++form) {
					for (const auto decoration : decorations) {
						for (const auto tail : tails) {
							const auto original = render(forms[form], from.Id, decoration) + std::string{ tail };
							const auto there    = applied(original, set(to.Label));
							expect(there == render(forms[form], to.Id, decoration) + std::string{ tail }, "relabel writes only the label");
							expect(applied(there, set(from.Label)) == original, "relabel round trip restores the bytes");
							++count;
						}
					}
				}
			}
			for (const auto tail : tails) {
				const auto original = render(forms[0], from.Id, "") + std::string{ tail };
				const auto bare     = applied(original, removal());
				expect(bare == tail, "removal keeps subject, discussion and patch");
				expect(applied(bare, set(from.Label)) == original, "remove and add restores a canonical header");
				++count;
			}
		}
		return count;
	}

	// A decision is bound to the body it was chosen against, and applying it
	// still goes through the draft's own expected revision.
	void revision_binding()
	{
		const std::string body  = "issue (blocking): Bound the copy.\n";
		auto              stale = request(body, set(feedback_label::suggestion));
		stale.ExpectedDigest    = digest("issue (blocking): Bound the copy.");
		const auto refused      = decide(stale);
		expect(refused.Refusal == feedback_edit_refusal::stale_revision && refused.Annotation.BodyDigest == digest(body), "older revision refused, current annotation reported");
		auto parser                   = request(body, set(feedback_label::suggestion));
		parser.ExpectedParserRevision = feedback_parser_revision + 1U;
		expect(decide(parser).Refusal == feedback_edit_refusal::stale_revision, "another parser revision refused");

		auto       draft    = revisioned_review_draft{ .Draft = { review_draft_id{ 7 }, { }, body, review_draft_state::drafting, { } }, .Revision = 4, .Scope = review_draft_scope_id{ 3 } };
		const auto decision = decide(request(draft.Draft.Body, set(feedback_label::suggestion)));
		const auto command  = revisioned_review_draft_command{ .Command = { .Id = draft.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = decision.Body }, .ExpectedRevision = draft.Revision, .Scope = draft.Scope };

		// The author typed after the edit was planned: the draft's revision
		// moved, so the old result is refused and the newer text survives.
		const auto typed = apply(draft, { .Command = { .Id = draft.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = body + "More.\n" }, .ExpectedRevision = 4, .Scope = draft.Scope });
		expect(typed.Refusal == revisioned_draft_refusal::none && typed.Accepted.Revision == 5, "typing advances the draft");
		const auto late = apply(typed.Accepted, command);
		expect(late.Refusal == revisioned_draft_refusal::conflict && late.Accepted == typed.Accepted, "stale edit refused, current draft kept");
		expect(decide(request(typed.Accepted.Draft.Body, set(feedback_label::suggestion))).Body == "suggestion (blocking): Bound the copy.\nMore.\n", "replanned against the current draft");
		auto oldPlan           = request(typed.Accepted.Draft.Body, set(feedback_label::suggestion));
		oldPlan.ExpectedDigest = digest(body);
		expect(decide(oldPlan).Refusal == feedback_edit_refusal::stale_revision, "the old plan's revision is refused against the new body");

		// A replaced scope (another window, account or pull request) refuses
		// the command without touching the draft.
		auto foreign     = command;
		foreign.Scope    = review_draft_scope_id{ 9 };
		const auto other = apply(draft, foreign);
		expect(other.Refusal == revisioned_draft_refusal::invalid_identity && other.Accepted == draft, "scope replacement refused, draft kept");

		const auto accepted = apply(draft, command);
		expect(accepted.Refusal == revisioned_draft_refusal::none && accepted.Accepted.Revision == 5 && accepted.Accepted.Draft.Body == "suggestion (blocking): Bound the copy.\n", "current edit applied through the draft");
	}

	// A managed tour stop: the edit stays inside the owner's narrative, the
	// envelope is untouched, and the owner maps the result again.
	void managed_envelope()
	{
		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       stop = render_tour_stop("Why boundary", 2, "Guard the callback.\r\n\r\nThe owner closes first.", navigation, { }, "\r\n");
		expect(stop.has_value(), "tour stop renders");
		const auto mapped = map_tour_stop(*stop);
		expect(mapped.Narrative.has_value(), "owner maps the narrative");
		const feedback_body_segment segment{ .Managed = true, .UserBody = feedback_span{ mapped.Narrative->Begin, mapped.Narrative->End } };
		const auto                  decision = decide(request(*stop, set(feedback_label::issue, feedback_expectation::blocking), segment));
		expect(decision.Changed && decision.Annotation.Label == feedback_label::issue && decision.Annotation.Expectation == feedback_expectation::blocking, "narrative labelled");
		const auto remapped = map_tour_stop(decision.Body);
		expect(remapped.Narrative && remapped.Narrative->Begin == decision.Segment.UserBody->Begin && remapped.Narrative->End == decision.Segment.UserBody->End, "owner maps the edited stop to the shifted segment");
		const auto back = decide(request(decision.Body, removal(), decision.Segment));
		expect(back.Changed && back.Body == *stop, "removal restores the published stop byte for byte");

		expect(refusal(*stop, set(feedback_label::issue)) == feedback_edit_refusal::restructuring_required, "the whole envelope is never labelled as ordinary text");
		const auto unavailable = decide(request(*stop, set(feedback_label::issue), { .Managed = true }));
		expect(unavailable.Refusal == feedback_edit_refusal::unsupported_header, "no segment, no edit");
	}

	/// splitmix64, so the generated corpus is the same on every platform.
	struct generator
	{
		std::uint64_t State;
		auto          next() -> std::uint64_t
		{
			State += 0x9e3779b97f4a7c15ULL;
			auto value = State;
			value      = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
			value      = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
			return value ^ (value >> 31U);
		}
		auto pick(std::size_t aCount) -> std::size_t { return static_cast<std::size_t>(next() % aCount); }
	};

	// Generated bodies and edits: invariants, read-back, and idempotence.
	auto generated() -> std::array<std::size_t, 7>
	{
		std::array<std::size_t, 7>            refusals{ };
		generator                             random{ 1077U };
		const std::array<std::string_view, 6> leads{ "", "\xEF\xBB\xBF", "\n", "\r\n\r\n", "   ", "    " };
		const std::array<std::string_view, 9> heads{ "issue", "Nitpick", "observation", "**todo", "praise**", "> question", "- note", "typo", "Plain words" };
		const std::array<std::string_view, 7> decorations{ "", " (blocking)", " (non-blocking, ux)", " (if-minor,blocking)", " (security)", " (", " ()" };
		const std::array<std::string_view, 5> separators{ ": ", ":** ", ":\t", ":", " " };
		const std::array<std::string_view, 6> subjects{ "Subject.", "caf\xC3\xA9 \xE6\x9D\xB1\xE4\xBA\xAC", "", "note: inner", "\xF0\x9F\x98\x80 emoji", "x" };
		const std::array<std::string_view, 5> tails{ "", "\n", "\r\n\r\nDiscussion.\r\n", "\n\n```suggestion\n\tbody();\n```\n", "\nissue: later line" };
		for (std::size_t round = 0U; round < 30000U; ++round) {
			std::string body;
			body.append(leads[random.pick(leads.size())]).append(heads[random.pick(heads.size())]).append(decorations[random.pick(decorations.size())]);
			body.append(separators[random.pick(separators.size())]).append(subjects[random.pick(subjects.size())]).append(tails[random.pick(tails.size())]);
			feedback_header_edit edit;
			edit.Change = static_cast<feedback_label_change>(random.pick(3U));
			edit.Label  = static_cast<feedback_label>(random.pick(feedback_label_count));
			if (random.pick(2U) == 0U) {
				edit.Expectation = static_cast<feedback_expectation>(random.pick(5U));
			}
			const auto before   = recognize_feedback_prefix(body);
			const auto decision = decide(request(body, edit));
			++refusals[static_cast<std::size_t>(decision.Refusal)];
			if (!decision.Changed) {
				continue;
			}
			++refusals[6];
			expect(decision.Replaced.Begin >= (before.Header ? before.Header->Begin : 0U), "nothing before the header changes");
			if (before.Subject) {
				expect(decision.Replaced.End == before.Subject->Begin, "subject and everything after it kept");
			}
			if (edit.Change != feedback_label_change::remove) {
				const auto twice = decide(request(decision.Body, edit));
				expect(twice.Refusal == feedback_edit_refusal::none && !twice.Changed, "an applied edit is idempotent");
			}
		}
		expect(refusals[6] > 0U && refusals[2] > 0U && refusals[3] > 0U && refusals[4] > 0U, "the generated corpus reaches the decided refusals");
		return refusals;
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 2, "usage: test_feedback_header_edit <pack directory>");
	const std::filesystem::path pack{ aArgv[1] };
	const auto                  cases = pack_cases(pack);
	decoration_and_form_edits();
	combinable_decorations();
	plain_insertion();
	const auto trips = round_trips();
	revision_binding();
	managed_envelope();
	const auto refusals = generated();
	std::cout << "pack header-edit cases: " << cases << '\n';
	std::cout << "label round trips: " << trips << '\n';
	std::cout << "generated: accepted " << refusals[0] << " (changed " << refusals[6] << ")" << ", stale " << refusals[1] << ", invalid " << refusals[2] << ", unsupported " << refusals[3] << ", restructuring " << refusals[4] << ", limit " << refusals[5] << '\n';
	std::cout << "FEEDBACK_HEADER_EDIT_OK\n";
	return EXIT_SUCCESS;
}
