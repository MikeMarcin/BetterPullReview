// [[BPR-1084]]. RFC-BPR-0048 R12: every label and prefix transformation the
// core offers leaves a native suggested change byte for byte, and leaves the
// draft's or tour stop's anchor where it was. The matrix runs each header
// edit (relabel, Plain, each standard decoration, a custom decoration in and
// out) through all four entry points: the header decision, the composition
// decision, the comment draft plan applied by the draft's own reducer, and
// the tour-stop plan applied by the tour's. The bodies carry a multiline
// replacement, an empty (deletion) fence, a `suggestion:-1+0` range, CRLF and
// tab content, a longer and a tilde fence, Unicode, and two fences in one
// body; the pack's header-edit cases run through the same four.

#include "../support/review_tour_fixture.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/suggestion_applicability.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback suggestion preservation failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto digest(std::string_view aBody) -> std::array<std::uint8_t, 32>
	{
		return sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() });
	}

	/// Every fenced block of a body, opener to closer, as exact bytes. A
	/// fence opens on a line of three or more backticks or tildes, indented
	/// at most three spaces, and closes on a line of at least as many of the
	/// same character; an unclosed fence runs to the end.
	auto fences(std::string_view aBody) -> std::vector<std::string>
	{
		std::vector<std::string>   found;
		std::size_t                at = 0U;
		std::optional<std::size_t> open;
		char                       marker = '\0';
		std::size_t                width  = 0U;
		while (at < aBody.size()) {
			const auto  end    = aBody.find('\n', at);
			const auto  stop   = end == std::string_view::npos ? aBody.size() : end + 1U;
			auto        line   = aBody.substr(at, stop - at);
			std::size_t indent = 0U;
			while (indent < line.size() && indent < 4U && line[indent] == ' ') {
				++indent;
			}
			std::size_t run = 0U;
			if (indent < 4U && indent < line.size() && (line[indent] == '`' || line[indent] == '~')) {
				const auto character = line[indent];
				while (indent + run < line.size() && line[indent + run] == character) {
					++run;
				}
				if (run >= 3U) {
					if (!open) {
						open   = at;
						marker = character;
						width  = run;
					} else if (character == marker && run >= width) {
						found.emplace_back(aBody.substr(*open, stop - *open));
						open.reset();
					}
				}
			}
			at = stop;
		}
		if (open) {
			found.emplace_back(aBody.substr(*open));
		}
		return found;
	}

	auto before_first_fence(std::string_view aBody) -> std::size_t
	{
		const auto found = fences(aBody);
		return found.empty() ? aBody.size() : aBody.find(found.front());
	}

	/// What a suggested change must keep: its fences, what the publication
	/// policy reads, and what the applicability decision says anywhere.
	void expect_patch_kept(std::string_view aBefore, std::string_view aAfter, std::string_view aWhy)
	{
		const auto before = fences(aBefore);
		expect(fences(aAfter) == before, std::string{ aWhy } + ": every fence kept byte for byte");
		expect(classify_review_body(aAfter) == classify_review_body(aBefore), std::string{ aWhy } + ": the publication policy reads the same body kind");
		for (const auto subject : { suggestion_subject::current_line, suggestion_subject::file, suggestion_subject::outdated_line }) {
			const auto placement = suggestion_placement_observation{ .Subject = subject };
			expect(decide_suggestion_applicability(aAfter, placement) == decide_suggestion_applicability(aBefore, placement), std::string{ aWhy } + ": the same applicability");
		}
	}

	/// A changed body is the old one with one span replaced, the span ends
	/// before the first fence, and everything after it is the old bytes.
	void expect_only_header(std::string_view aBefore, const feedback_header_edit_decision& aDecision, std::string_view aWhy)
	{
		if (!aDecision.Changed) {
			expect(aDecision.Body.empty(), std::string{ aWhy } + ": no change carries no body");
			return;
		}
		const auto& replaced = aDecision.Replaced;
		const auto  after    = std::string_view{ aDecision.Body };
		expect(replaced.Begin <= replaced.End && replaced.End <= aBefore.size(), std::string{ aWhy } + ": span inside the body");
		expect(after.substr(0U, replaced.Begin) == aBefore.substr(0U, replaced.Begin), std::string{ aWhy } + ": bytes before the header kept");
		const auto tail = aBefore.substr(replaced.End);
		expect(after.size() >= tail.size() && after.substr(after.size() - tail.size()) == tail, std::string{ aWhy } + ": every byte after the header kept");
		const auto firstFence = before_first_fence(aBefore);
		expect(replaced.End <= firstFence, std::string{ aWhy } + ": the header ends before the first fence");
		expect_patch_kept(aBefore, after, aWhy);
	}

	struct operation
	{
		std::string          Name;
		feedback_header_edit Edit;
	};

	auto operations() -> std::vector<operation>
	{
		std::vector<operation> result;
		for (const auto& entry : feedback_label_catalog()) {
			result.push_back({ "set " + std::string{ entry.Id }, { .Change = feedback_label_change::set, .Label = entry.Label } });
		}
		result.push_back({ "plain", { .Change = feedback_label_change::remove } });
		for (const auto expectation : { feedback_expectation::unspecified, feedback_expectation::blocking, feedback_expectation::non_blocking, feedback_expectation::if_minor }) {
			result.push_back({ "expectation " + std::to_string(static_cast<int>(expectation)), { .Expectation = expectation } });
		}
		result.push_back({ "set issue (blocking)", { .Change = feedback_label_change::set, .Label = feedback_label::issue, .Expectation = feedback_expectation::blocking } });
		for (const auto* token : { "if-minor", "security", "blocking" }) {
			result.push_back({ std::string{ "decorate " } + token, { .Decoration = feedback_decoration_change{ .Token = token } } });
			result.push_back({ std::string{ "undecorate " } + token, { .Decoration = feedback_decoration_change{ .Token = token, .Written = false } } });
		}
		result.push_back({ "set suggestion with if-minor", { .Change = feedback_label_change::set, .Label = feedback_label::suggestion, .Decoration = feedback_decoration_change{ .Token = "if-minor" } } });
		return result;
	}

	// Subjects and suggested changes, each the first block after a header.
	constexpr std::array<std::string_view, 9> patches{
		// A multiline replacement.
		"Clamp the width.\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n\treturn width;\n```\n",
		// An empty fence deletes the line.
		"Remove the duplicate.\n\n```suggestion\n```\n",
		// A range, CRLF and a tab (the pack's own case).
		"Bound the copy.\r\n\r\n```suggestion:-1+0\r\n\tcopy_n(src, checked_count, dst);\r\n```\r\n\r\nKeep this explanation.\r\n",
		// A longer fence, a blank and trailing spaces inside, CRLF throughout.
		"Tabs and spaces.\r\n\r\n````suggestion\r\n\t\tx = 1;  \r\n\r\n\t```\r\n````\r\n",
		// A tilde fence and Markdown after it, with no final newline.
		"Use the guard.\n\n~~~suggestion\nstd::scoped_lock lock{ Mutex };\n~~~\n> Quoted after the change.",
		// Unicode in the subject and the replacement.
		"Rename caf\xC3\xA9_\xE6\x9D\xB1\xE4\xBA\xAC.\n\n```suggestion\nconst auto \xE6\x9D\xB1\xE4\xBA\xAC = \"caf\xC3\xA9 \xF0\x9F\x98\x80\";\n```\n",
		// Two distinct fences, a range on the second.
		"Two changes.\n\n```suggestion\nfirst();\n```\n\nAnd this one:\n\n```suggestion:-2+1\nsecond();\nthird();\n```\n",
		// An info string with spaces before it, as the classifier reads it.
		"Spaced info.\n\n```   suggestion\nvalue = 0;\n```\n",
		// Prose only: a suggestion label with no patch stays prose.
		"Consider clamping the width to one pixel.",
	};
	constexpr std::array<std::string_view, 6> headers{ "", "issue: ", "issue (blocking): ", "**suggestion (non-blocking, if-minor):** ", "question (security): ", "Nitpick (if-minor): " };

	auto line_anchor() -> source_line_ref
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[3]   = 7U;
		return { .Comparison = file_comparison_digest{ bytes }, .Side = diff_side::right, .Line = 41U };
	}

	auto comment(std::string aBody, bool aReply) -> revisioned_review_draft
	{
		auto value                = revisioned_review_draft{ .Draft = { .Id = review_draft_id{ 7 }, .Anchor = line_anchor(), .Body = std::move(aBody) }, .Revision = 4, .Scope = review_draft_scope_id{ 3 } };
		value.Draft.ReplyToThread = aReply ? std::optional<std::string>{ "PRRT_reply_thread" } : std::nullopt;
		return value;
	}

	struct tally
	{
		std::size_t Bodies  = 0U;
		std::size_t Changed = 0U;
		std::size_t Refused = 0U;
		std::size_t Paths   = 0U;
	};

	/// One body and one edit through all four entry points.
	void one(std::string_view aBody, const operation& aOperation, tally& aTally)
	{
		const auto why     = std::string{ aOperation.Name } + " on " + std::string{ aBody.substr(0U, 48U) };
		const auto request = feedback_header_edit_request{ .Body = aBody, .ExpectedDigest = digest(aBody), .Edit = aOperation.Edit };

		const auto header = decide_feedback_header_edit(request);
		expect_only_header(aBody, header, why + " [header]");
		const auto composed = decide_feedback_composition_edit(request);
		expect_only_header(aBody, composed, why + " [composition]");
		expect(composed.Refusal == header.Refusal && composed.Body == header.Body, why + ": a written body composes as the header decision decides");
		++aTally.Paths;
		++aTally.Paths;
		if (header.Changed) {
			++aTally.Changed;
		} else if (header.Refusal != feedback_edit_refusal::none) {
			++aTally.Refused;
		}

		// A root comment and a reply: the plan's command carries the body
		// only, and the reducer keeps the line, the thread and the identity.
		for (const auto reply : { false, true }) {
			const auto current = comment(std::string{ aBody }, reply);
			const auto plan    = plan_feedback_draft_edit(current, feedback_draft_basis_of(current), aOperation.Edit);
			expect(plan.Decision.Refusal == header.Refusal && plan.Decision.Body == header.Body, why + ": the draft plan decides as the header decision");
			expect(plan.Command.has_value() == header.Changed, why + ": a command only for a change");
			if (plan.Command) {
				expect(!plan.Command->ChangeReplyTarget && !plan.Command->ReplyToThread && !plan.Command->Composition, why + ": the command moves no thread and carries no link");
				const auto outcome = apply(current, *plan.Command);
				expect(outcome.Refusal == revisioned_draft_refusal::none && outcome.Accepted.Draft.Body == header.Body, why + ": the draft owner applies the decided body");
				expect(outcome.Accepted.Draft.Anchor == current.Draft.Anchor && outcome.Accepted.Draft.ReplyToThread == current.Draft.ReplyToThread
						   && outcome.Accepted.Draft.Id == current.Draft.Id && outcome.Accepted.Scope == current.Scope,
					   why + ": line, thread, identity and scope unchanged");
				expect_patch_kept(aBody, outcome.Accepted.Draft.Body, why + " [draft]");
			}
			++aTally.Paths;
		}

		// A local tour stop: only its narrative's header changes.
		auto tour               = bpr::tests::review_tour_fixture();
		tour.Stops.front().Body = std::string{ aBody };
		const auto stop         = tour.Stops.front().Id;
		const auto tourPlan     = plan_feedback_tour_stop_edit(tour, stop, *feedback_tour_stop_basis(tour, stop), aOperation.Edit);
		expect(tourPlan.Decision.Refusal == header.Refusal && tourPlan.Decision.Body == header.Body, why + ": the tour plan decides as the header decision");
		expect(tourPlan.Command.has_value() == header.Changed, why + ": a tour command only for a change");
		if (tourPlan.Command) {
			const auto outcome = apply(tour, *tourPlan.Command);
			expect(outcome.Refusal == tour_refusal::none, why + ": the tour owner applies it");
			const auto& before = tour.Stops.front();
			const auto& edited = outcome.Accepted.Stops.front();
			expect(edited.Body == header.Body && edited.Title == before.Title && edited.Original == before.Original && edited.Mapped == before.Mapped
					   && edited.Mapping == before.Mapping && edited.Related == before.Related && edited.Destination == before.Destination,
				   why + ": the stop's ranges and destination unchanged");
			expect_patch_kept(aBody, edited.Body, why + " [tour]");
		}
		++aTally.Paths;
		++aTally.Bodies;
	}

	auto matrix() -> tally
	{
		auto       counts = tally{ };
		const auto edits  = operations();
		for (const auto header : headers) {
			for (const auto patch : patches) {
				const auto body = std::string{ header } + std::string{ patch };
				for (const auto& edit : edits) {
					one(body, edit, counts);
				}
			}
		}
		return counts;
	}

	/// Labels and fences are independent: a relabel never adds, removes or
	/// infers a patch, and a fence never implies a label.
	void independence()
	{
		const auto prose = std::string{ "suggestion: Consider clamping the width to one pixel." };
		const auto issue = decide_feedback_header_edit({ .Body = prose, .ExpectedDigest = digest(prose), .Edit = { .Change = feedback_label_change::set, .Label = feedback_label::issue } });
		expect(issue.Changed && issue.Body == "issue: Consider clamping the width to one pixel." && fences(issue.Body).empty(), "a prose suggestion relabelled gains no patch");
		const auto patched = std::string{ "issue (blocking): Clamp it.\n\n```suggestion\nclamp();\n```\n" };
		expect(recognize_feedback_prefix(patched).Label == feedback_label::issue, "an issue with a patch is still an issue");
		const auto plain = std::string{ "```suggestion\nclamp();\n```\n" };
		expect(recognize_feedback_prefix(plain).Status != feedback_prefix_status::recognized, "a fence alone implies no label");
		const auto refused = decide_feedback_header_edit({ .Body = plain, .ExpectedDigest = digest(plain), .Edit = { .Change = feedback_label_change::set, .Label = feedback_label::suggestion } });
		expect(refused.Refusal == feedback_edit_refusal::restructuring_required && !refused.Changed, "a body that opens with a fence takes no header; nothing is moved to make room");
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

	/// The pack's header-edit cases through all four entry points.
	auto pack_cases(const std::filesystem::path& aPack) -> std::size_t
	{
		auto       stream = std::ifstream{ aPack / "fixtures" / "header-edit-cases.json", std::ios::binary };
		const auto bytes  = std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
		const auto parsed = parse_cycle_json(bytes, 1U << 20U);
		expect(parsed.has_value(), "header-edit-cases.json parses");
		const auto& root    = std::get<cycle_json_value::object>(parsed->Value);
		const auto& cases   = std::get<cycle_json_value::array>(member(root, "cases")->Value);
		std::size_t count   = 0U;
		auto        tallied = tally{ };
		for (const auto& entry : cases) {
			const auto& item      = std::get<cycle_json_value::object>(entry.Value);
			const auto  id        = text(*member(item, "id"));
			const auto  before    = text(*member(item, "before"));
			const auto  after     = text(*member(item, "after"));
			const auto& operation = std::get<cycle_json_value::object>(member(item, "operation")->Value);
			auto        edit      = feedback_header_edit{ };
			if (const auto* label = member(operation, "label")) {
				if (std::holds_alternative<std::monostate>(label->Value)) {
					edit.Change = feedback_label_change::remove;
				} else {
					const auto found = find_feedback_label(text(*label));
					expect(found.has_value(), id + ": label in the catalog");
					edit = { .Change = feedback_label_change::set, .Label = *found };
				}
			}
			if (const auto* expectation = member(operation, "expectation")) {
				expect(text(*expectation) == "non_blocking", id + ": expectation the fixture uses");
				edit.Expectation = feedback_expectation::non_blocking;
			}
			one(before, { id, edit }, tallied);
			const auto decided = decide_feedback_header_edit({ .Body = before, .ExpectedDigest = digest(before), .Edit = edit });
			expect((decided.Changed ? decided.Body : before) == after, id + ": the pack's exact bytes");
			if (!fences(before).empty()) {
				++count;
			}
		}
		expect(count == 3U, "the pack's three cases with a native patch were read");
		return cases.size();
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 2, "usage: test_feedback_suggestion_preservation <pack directory>");
	// The scanner itself, so the matrix's fence comparison means something.
	expect(fences("a\n```suggestion\nx\n```\nb\n~~~suggestion\n~~~\n").size() == 2U, "two fences found");
	expect(fences("```suggestion\r\n\r\n````\r\n") == std::vector<std::string>{ "```suggestion\r\n\r\n````\r\n" }, "a longer closer closes, CRLF kept");
	expect(fences("````suggestion\n```\n````").size() == 1U && fences("````suggestion\n```\n````").front() == "````suggestion\n```\n````", "a shorter run inside a longer fence is content");
	const auto cases = pack_cases(aArgv[1]);
	independence();
	const auto counts = matrix();
	expect(counts.Changed > 0U && counts.Refused > 0U, "the matrix reaches both changes and refusals");
	std::cout << "pack header-edit cases through four entry points: " << cases << '\n';
	std::cout << "matrix bodies: " << counts.Bodies << ", changed " << counts.Changed << ", refused " << counts.Refused << ", entry-point runs " << counts.Paths << '\n';
	std::cout << "FEEDBACK_SUGGESTION_PRESERVATION_OK\n";
	return EXIT_SUCCESS;
}
