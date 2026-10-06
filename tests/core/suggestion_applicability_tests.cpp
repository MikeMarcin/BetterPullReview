// [[BPR-1084]]. RFC-BPR-0048 R13: whether a suggested change would be GitHub's
// native, applicable suggestion where it is about to be published, decided
// before Publish from the facts the publication policy already reads, and
// never by moving it to another line. The pack's three native-integration
// cases (issue-with-suggestion, suggestion-without-patch and
// unsupported-native-patch) are read from its fixture and decided here.

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_applicability.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "suggestion applicability failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto patch = std::string_view{ "issue (blocking): Clamp the width.\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n```\n" };
	constexpr auto prose = std::string_view{ "suggestion: Consider clamping the width to one pixel." };

	auto line(review_placement aPlacement = review_placement::unknown, off_hunk_creation_capability aCapability = off_hunk_creation_capability::unknown, bool aCurrent = false) -> suggestion_placement_observation
	{
		return { .Subject = suggestion_subject::current_line, .Side = diff_side::right, .Placement = aPlacement, .Capability = aCapability, .CapabilityCurrent = aCurrent };
	}

	auto native() -> suggestion_applicability_decision
	{
		return { .Applicability = suggestion_applicability::native };
	}
	auto textual(suggestion_text_reason aReason) -> suggestion_applicability_decision
	{
		return { .Applicability = suggestion_applicability::textual_only, .Reason = aReason };
	}

	void subjects()
	{
		expect(decide_suggestion_applicability(patch, line()) == native(), "a patch on a current head line is native");
		expect(decide_suggestion_applicability(patch, line(review_placement::legacy_line)) == native(), "a line inside the patch is native");
		expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::current_line, .Side = diff_side::left }) == textual(suggestion_text_reason::base_side), "the base side is text only");
		expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::file }) == textual(suggestion_text_reason::file_subject), "a file comment is text only");
		expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::outdated_line }) == textual(suggestion_text_reason::outdated_line), "an outdated thread is text only");
		expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::unavailable }) == textual(suggestion_text_reason::unavailable_anchor), "no current line is text only");
		expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::conversation }) == textual(suggestion_text_reason::conversation), "a pull request comment is text only");
		expect(decide_suggestion_applicability(patch, { }) == textual(suggestion_text_reason::unavailable_anchor), "an observation nobody filled in claims nothing native");

		// Out-of-range facts claim nothing.
		auto bad    = line();
		bad.Subject = static_cast<suggestion_subject>(9);
		expect(decide_suggestion_applicability(patch, bad) == textual(suggestion_text_reason::invalid_observation), "an unknown subject");
		bad      = line();
		bad.Side = static_cast<diff_side>(5);
		expect(decide_suggestion_applicability(patch, bad) == textual(suggestion_text_reason::invalid_observation), "an unknown side");
		bad           = line();
		bad.Placement = static_cast<review_placement>(7);
		expect(decide_suggestion_applicability(patch, bad) == textual(suggestion_text_reason::invalid_observation), "an unknown placement");
	}

	// Negative controls: bodies with no suggested change are never a patch,
	// wherever they go, and a label never makes one.
	void no_patch()
	{
		const std::array<std::string_view, 7> bodies{
			prose,
			"issue (blocking): Clamp the width.",
			"Mention `suggestion` inline.",
			"A fence in another language:\n\n```cpp\nclamp();\n```\n",
			"Two backticks are not a fence: ``suggestion``",
			"",
			"suggestion (non-blocking, if-minor): ```",
		};
		const std::array<suggestion_placement_observation, 4> placements{ line(), { .Subject = suggestion_subject::file }, { .Subject = suggestion_subject::conversation }, line(review_placement::off_hunk, off_hunk_creation_capability::unsupported, true) };
		for (const auto body : bodies) {
			for (const auto& placement : placements) {
				expect(decide_suggestion_applicability(body, placement) == suggestion_applicability_decision{ }, std::string{ "no patch: " } + std::string{ body });
			}
		}
		expect(decide_suggestion_applicability("~~~ suggestion\nx\n~~~\n", line()) == native(), "a tilde fence is a patch too");
		expect(decide_suggestion_applicability("Range.\n\n```suggestion:-1+0\nx\n```\n", line()) == native(), "a range is a patch");
		expect(decide_suggestion_applicability("Deletion.\n\n```suggestion\n```\n", line()) == native(), "an empty fence is a patch");
	}

	// The decision is the publication policy's own first step: native
	// exactly when it would attempt a native line, text only exactly when it
	// would refuse a suggestion's only placement, a file comment.
	auto agrees_with_publication_policy() -> std::size_t
	{
		std::size_t count = 0U;
		for (const auto placement : { review_placement::unknown, review_placement::legacy_line, review_placement::off_hunk }) {
			for (const auto capability : { off_hunk_creation_capability::unknown, off_hunk_creation_capability::supported, off_hunk_creation_capability::unsupported }) {
				for (const auto current : { false, true }) {
					const auto decided = decide_suggestion_applicability(patch, line(placement, capability, current));
					const auto plan    = plan_review_publication({ .Placement = placement, .Capability = capability, .Body = review_body_compatibility::requires_line_subject, .Eligible = true, .CapabilityCurrent = current });
					expect((decided == native()) == (plan.Action == review_publication_action::native_attempt), "native exactly when the policy attempts a native line");
					expect((decided == textual(suggestion_text_reason::inline_comment_unsupported)) == (plan.Diagnostic == review_publication_diagnostic::incompatible_body), "text only exactly when the policy refuses the fallback");
					const auto portable = plan_review_publication({ .Placement = placement, .Capability = capability, .Body = review_body_compatibility::portable, .Eligible = true, .CapabilityCurrent = current });
					expect(decide_suggestion_applicability(prose, line(placement, capability, current)) == suggestion_applicability_decision{ } && portable.Action != review_publication_action::refuse, "prose publishes either way and claims no patch");
					++count;
				}
			}
		}
		// The one refused line: GitHub definitively refused an inline comment
		// there, and the evidence is current. A stale cache is ignored.
		expect(decide_suggestion_applicability(patch, line(review_placement::off_hunk, off_hunk_creation_capability::unsupported, true)) == textual(suggestion_text_reason::inline_comment_unsupported), "refused inline comment: text only");
		expect(decide_suggestion_applicability(patch, line(review_placement::off_hunk, off_hunk_creation_capability::unsupported, false)) == native(), "stale evidence: GitHub is asked again");
		expect(decide_suggestion_applicability(patch, line(review_placement::off_hunk)) == native(), "an off-hunk line with no evidence is asked as an inline comment");
		return count;
	}

	void names()
	{
		expect(stable_name(suggestion_applicability::no_patch) == "no_patch" && stable_name(suggestion_applicability::native) == "native" && stable_name(suggestion_applicability::textual_only) == "textual_only", "applicability names");
		for (std::uint8_t value = 0U; value <= static_cast<std::uint8_t>(suggestion_text_reason::invalid_observation); ++value) {
			expect(stable_name(static_cast<suggestion_text_reason>(value)) != "unknown", "every reason is named");
		}
		expect(stable_name(static_cast<suggestion_text_reason>(200)) == "unknown", "an unknown reason");
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

	// The pack's three cases this card owns, decided as their stimulus says.
	auto pack_cases(const std::filesystem::path& aPack) -> std::size_t
	{
		auto       stream = std::ifstream{ aPack / "fixtures" / "native-integration-cases.json", std::ios::binary };
		const auto bytes  = std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
		const auto parsed = parse_cycle_json(bytes, 1U << 20U);
		expect(parsed.has_value(), "native-integration-cases.json parses");
		const auto& root = std::get<cycle_json_value::object>(parsed->Value);
		expect(text(*member(root, "schema")) == "bpr.cc.integration-cases.v1", "case schema");
		std::size_t found = 0U;
		for (const auto& entry : std::get<cycle_json_value::array>(member(root, "cases")->Value)) {
			const auto& item = std::get<cycle_json_value::object>(entry.Value);
			const auto  id   = text(*member(item, "id"));
			if (id == "issue-with-suggestion") {
				// "an explicitly authorized issue containing a native suggested
				// edit on a supported exact anchor"
				expect(recognize_feedback_prefix(patch).Label == feedback_label::issue && decide_suggestion_applicability(patch, line()) == native(), id);
				++found;
			} else if (id == "suggestion-without-patch") {
				// "a suggestion comment containing only prose": valid feedback,
				// no invented apply action or replacement block.
				expect(recognize_feedback_prefix(prose).Label == feedback_label::suggestion, id + ": still a suggestion");
				for (const auto subject : { suggestion_subject::current_line, suggestion_subject::file, suggestion_subject::conversation }) {
					expect(decide_suggestion_applicability(prose, { .Subject = subject }) == suggestion_applicability_decision{ }, id + ": no patch anywhere");
				}
				++found;
			} else if (id == "unsupported-native-patch") {
				// "The current placement can only publish a permalink/top-level
				// comment": honestly unavailable, no nearby-line relocation.
				expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::file }).Applicability == suggestion_applicability::textual_only, id + ": a permalink comment");
				expect(decide_suggestion_applicability(patch, { .Subject = suggestion_subject::conversation }).Applicability == suggestion_applicability::textual_only, id + ": a top-level comment");
				expect(decide_suggestion_applicability(patch, line(review_placement::off_hunk, off_hunk_creation_capability::unsupported, true)).Applicability == suggestion_applicability::textual_only, id + ": a line GitHub refused");
				++found;
			}
		}
		expect(found == 3U, "the pack's three cases for this card");
		return found;
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 2, "usage: test_suggestion_applicability <pack directory>");
	subjects();
	no_patch();
	const auto combinations = agrees_with_publication_policy();
	names();
	const auto cases = pack_cases(aArgv[1]);
	std::cout << "publication-policy combinations: " << combinations << '\n';
	std::cout << "pack native-integration cases: " << cases << '\n';
	std::cout << "SUGGESTION_APPLICABILITY_OK\n";
	return EXIT_SUCCESS;
}
