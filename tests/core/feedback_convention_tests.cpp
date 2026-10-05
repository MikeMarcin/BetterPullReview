// [[BPR-1076]]. The Conventional Comments catalog and the bounded prefix
// recognizer (RFC-BPR-0047 R01-R04, R06, R40), driven by the pack's own
// LABEL-CATALOG.json and parser-cases.json plus generated bound cases.

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback convention failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(static_cast<bool>(stream), "pack file readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto read_json(const std::filesystem::path& aPath) -> cycle_json_value::object
	{
		const auto parsed = parse_cycle_json(read_file(aPath), 1U << 20U);
		expect(parsed.has_value(), "pack JSON parses");
		const auto* object = std::get_if<cycle_json_value::object>(&parsed->Value);
		expect(object != nullptr, "pack JSON is an object");
		return *object;
	}

	auto member(const cycle_json_value::object& aObject, std::string_view aKey) -> const cycle_json_value&
	{
		const auto* found = find_member(aObject, aKey);
		expect(found != nullptr, aKey);
		return *found;
	}

	auto text(const cycle_json_value& aValue) -> std::string
	{
		const auto* found = std::get_if<std::string>(&aValue.Value);
		expect(found != nullptr, "string field");
		return *found;
	}

	auto has(const feedback_annotation& aResult, feedback_diagnostic aDiagnostic) -> bool
	{
		return std::ranges::find(aResult.Diagnostics, aDiagnostic) != aResult.Diagnostics.end();
	}

	auto slice(std::string_view aBody, const std::optional<feedback_span>& aSpan) -> std::string_view
	{
		expect(aSpan.has_value(), "span present");
		return feedback_text(aBody, *aSpan);
	}

	auto upper(std::string_view aText) -> std::string
	{
		std::string result{ aText };
		std::ranges::transform(result, result.begin(), [](char aChar) { return aChar >= 'a' && aChar <= 'z' ? static_cast<char>(aChar - 'a' + 'A') : aChar; });
		return result;
	}

	auto joined(std::initializer_list<std::string_view> aParts) -> std::string
	{
		std::string result;
		for (const auto part : aParts) {
			result += part;
		}
		return result;
	}

	/// Properties every result holds, whatever the input: spans ordered and
	/// inside the user body, no fact without its status, digest of the exact
	/// bytes, and a window no wider than the budget.
	void expect_invariants(std::string_view aBody, const feedback_annotation& aResult)
	{
		expect(aResult.BodyDigest == sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() }), "digest names the exact body");
		expect(aResult.ParserRevision == feedback_parser_revision, "parser revision recorded");
		expect(aResult.InspectedBytes <= maximum_feedback_prefix_bytes, "classifier window bounded");
		const bool recognized = aResult.Status == feedback_prefix_status::recognized;
		const bool structured = recognized || aResult.Status == feedback_prefix_status::unknown_label;
		expect(aResult.Label.has_value() == recognized && aResult.Guidance.has_value() == recognized, "label and guidance only when recognized");
		expect(aResult.Header.has_value() == structured && aResult.Subject.has_value() == structured, "spans only for a well-formed header");
		expect(structured || aResult.Decorations.empty(), "a broken list is never an empty one");
		expect(recognized || aResult.Expectation == feedback_expectation::unspecified, "no expectation without a recognized label");
		expect((aResult.Status == feedback_prefix_status::limited) == has(aResult, feedback_diagnostic::classifier_budget_exceeded), "limited is always explained");
		if (aResult.Status == feedback_prefix_status::plain) {
			expect(!aResult.RawLabel.has_value(), "plain keeps no token");
		}
		if (!aResult.UserBody) {
			return;
		}
		const auto user = *aResult.UserBody;
		expect(user.Begin <= user.End && user.End <= aBody.size(), "user body inside the body");
		if (aResult.RawLabel) {
			expect(user.Begin <= aResult.RawLabel->Begin && aResult.RawLabel->Begin < aResult.RawLabel->End && aResult.RawLabel->End <= user.End, "raw label inside the user body");
			const auto raw = slice(aBody, aResult.RawLabel);
			expect(find_feedback_label(raw) == aResult.Label || !recognized, "raw token is the recognized label");
		}
		if (!structured) {
			return;
		}
		const auto header  = *aResult.Header;
		const auto subject = *aResult.Subject;
		expect(user.Begin <= header.Begin && header.Begin < header.End && header.End == subject.Begin && subject.Begin < subject.End && subject.End <= user.End, "header then subject, contiguous");
		expect(std::ranges::all_of(slice(aBody, aResult.Header), [](char aByte) { return static_cast<unsigned char>(aByte) < 128U; }), "header bytes are ASCII, so its edges are code point boundaries");
		expect(header.Begin <= aResult.RawLabel->Begin && aResult.RawLabel->End <= header.End, "label inside header");
		for (const auto& decoration : aResult.Decorations) {
			expect(aResult.RawLabel->End < decoration.Raw.Begin && decoration.Raw.End < header.End, "decoration inside header");
		}
		const auto subjectText = slice(aBody, aResult.Subject);
		expect(subjectText.find('\n') == std::string_view::npos && subjectText.back() != '\r' && subjectText.back() != ' ', "subject is one trimmed line");
		if (aResult.Discussion) {
			expect(aResult.Discussion->Begin > subject.End && aResult.Discussion->End == user.End && aBody[aResult.Discussion->Begin - 1U] == '\n', "discussion follows the subject line");
		}
	}

	auto recognize_checked(std::string_view aBody) -> feedback_annotation
	{
		const std::string copy{ aBody };
		auto              result = recognize_feedback_prefix(aBody);
		expect(copy == aBody, "recognition never changes the body");
		expect_invariants(aBody, result);
		return result;
	}

	// R01: the catalog is the pack's catalog, field for field and in order.
	void catalog_matches_pack(const std::filesystem::path& aPack)
	{
		const auto document = read_json(aPack / "specs" / "LABEL-CATALOG.json");
		expect(text(member(document, "schema")) == "bpr.feedback-label-catalog.v1", "catalog schema");
		expect(std::get<std::int64_t>(member(document, "catalog_revision").Value) == feedback_catalog_revision, "catalog revision");
		const auto& labels = std::get<cycle_json_value::array>(member(document, "labels").Value);
		expect(labels.size() == feedback_label_count && feedback_label_catalog().size() == feedback_label_count, "twelve labels");
		for (std::size_t at = 0U; at < labels.size(); ++at) {
			const auto& entry      = std::get<cycle_json_value::object>(labels[at].Value);
			const auto& definition = feedback_label_catalog()[at];
			expect(static_cast<std::size_t>(definition.Label) == at && &definition_of(definition.Label) == &definition, "stable order");
			expect(definition.Id == text(member(entry, "id")) && stable_name(definition.Label) == definition.Id, "id");
			expect(definition.Description == text(member(entry, "description")), "description");
			expect(stable_name(definition.Guidance) == text(member(entry, "guidance")), "guidance");
			expect(definition.IconRole == text(member(entry, "icon_role")) && definition.IconRole == "feedback.label." + std::string{ definition.Id }, "own icon role");
			expect(std::get<bool>(member(entry, "first_class").Value), "first class");
			expect(find_feedback_label(definition.Id) == definition.Label && find_feedback_label(upper(definition.Id)) == definition.Label, "lookup, ASCII case-insensitive");
		}
		// No alias, abbreviation or plural stands in for a label, in
		// particular for the three labels added beyond the original nine.
		for (const std::string_view alias : { "nit", "nits", "typos", "spelling", "polishing", "polishes", "quibbles", "todos", "praises", "issues", "suggest", "q", "" }) {
			expect(!find_feedback_label(alias), alias);
			const auto result = recognize_checked(std::string{ alias } + (alias.empty() ? "" : ": Some subject."));
			expect(!result.Label, "an alias never classifies");
		}
	}

	// R02-R04: the pack's normative corpus, against the real recognizer.
	void parser_corpus(const std::filesystem::path& aPack)
	{
		const auto document = read_json(aPack / "fixtures" / "parser-cases.json");
		expect(text(member(document, "schema")) == "bpr.cc.parser-cases.v1", "corpus schema");
		const auto& cases = std::get<cycle_json_value::array>(member(document, "cases").Value);
		expect(cases.size() == 48U, "all 48 corpus cases present");
		for (const auto& item : cases) {
			const auto& object = std::get<cycle_json_value::object>(item.Value);
			const auto  id     = text(member(object, "id"));
			const auto  body   = text(member(object, "body"));
			const auto  result = recognize_checked(body);
			expect(stable_name(result.Status) == text(member(object, "expected_status")), id + ": status");
			const auto& label = member(object, "expected_label");
			if (std::holds_alternative<std::monostate>(label.Value)) {
				expect(!result.Label, id + ": no label");
			} else {
				expect(result.Label && stable_name(*result.Label) == text(label), id + ": label");
				expect(result.Guidance == definition_of(*result.Label).Guidance, id + ": guidance from the catalog");
			}
			expect(stable_name(result.Expectation) == text(member(object, "expected_expectation")), id + ": expectation");
			if (const auto* required = find_member(object, "required_diagnostics")) {
				for (const auto& diagnostic : std::get<cycle_json_value::array>(required->Value)) {
					expect(std::ranges::any_of(result.Diagnostics, [&](feedback_diagnostic aFound) { return stable_name(aFound) == text(diagnostic); }), id + ": diagnostic " + text(diagnostic));
				}
			}
		}
		std::cout << "corpus cases: " << cases.size() << '\n';
	}

	// R01: every label has the same capability in every form, decoration and case.
	void every_label_every_form()
	{
		struct decoration_case
		{
			std::string_view     Source;
			feedback_expectation Expectation;
			std::size_t          Count;
		};
		const std::array decorations{
			decoration_case{ "", feedback_expectation::unspecified, 0U },
			decoration_case{ " (blocking)", feedback_expectation::blocking, 1U },
			decoration_case{ " (non-blocking)", feedback_expectation::non_blocking, 1U },
			decoration_case{ " (if-minor)", feedback_expectation::if_minor, 1U },
			decoration_case{ " (security, ux)", feedback_expectation::unspecified, 2U },
			decoration_case{ "(Blocking,security)", feedback_expectation::blocking, 2U },
		};
		std::size_t checked = 0U;
		for (const auto& definition : feedback_label_catalog()) {
			const std::string lower{ definition.Id };
			for (const auto& spelling : { lower, upper(lower), upper(lower.substr(0U, 1U)) + lower.substr(1U) }) {
				for (const auto& decoration : decorations) {
					const std::string tail{ decoration.Source };
					const std::array  forms{
						std::pair{ joined({ spelling, tail, ": Subject text.\n\nDiscussion." }), feedback_header_form::plain },
						std::pair{ joined({ "**", spelling, tail, ":** Subject text.\n\nDiscussion." }), feedback_header_form::strong_header },
						std::pair{ joined({ "**", spelling, "**", tail, ": Subject text.\n\nDiscussion." }), feedback_header_form::strong_label },
					};
					for (const auto& [body, form] : forms) {
						const auto result = recognize_checked(body);
						expect(result.Status == feedback_prefix_status::recognized && result.Label == definition.Label && result.Form == form, body);
						expect(result.Guidance == definition.Guidance, "guidance is the catalog's, whatever was written");
						expect(slice(body, result.RawLabel) == spelling, "raw spelling retained");
						expect(slice(body, result.Subject) == "Subject text." && slice(body, result.Discussion) == "\nDiscussion.", "subject and discussion");
						expect(result.Decorations.size() == decoration.Count, "decoration count");
						expect(result.Expectation == decoration.Expectation, "explicit expectation only from decorations");
						++checked;
					}
				}
			}
		}
		std::cout << "label/form/decoration/case combinations: " << checked << '\n';
	}

	template <typename TAnnotation>
	concept carries_resolution = requires(TAnnotation aValue) { aValue.Resolved; } || requires(TAnnotation aValue) { aValue.Resolution; };
	template <typename TAnnotation>
	concept carries_defect_state = requires(TAnnotation aValue) { aValue.Severity; } || requires(TAnnotation aValue) { aValue.Verified; };
	template <typename TAnnotation>
	concept carries_verdict = requires(TAnnotation aValue) { aValue.Verdict; } || requires(TAnnotation aValue) { aValue.Blocking; };

	// R06: guidance, explicit expectation and diagnostics stay separate facts.
	void separate_facts()
	{
		const auto note = recognize_checked("note: The snapshot is immutable.");
		expect(note.Guidance == feedback_label_guidance::always_non_blocking && note.Expectation == feedback_expectation::unspecified && note.Diagnostics.empty(), "guidance never becomes an explicit decoration");
		const auto chore = recognize_checked("chore: Run the release script.");
		expect(chore.Guidance == feedback_label_guidance::required_before_acceptance && chore.Expectation == feedback_expectation::unspecified, "required-before-acceptance guidance is not blocking");
		const auto contradiction = recognize_checked("note (blocking): Clarify.");
		expect(contradiction.Status == feedback_prefix_status::recognized && contradiction.Expectation == feedback_expectation::blocking && contradiction.Guidance == feedback_label_guidance::always_non_blocking, "contradiction keeps both facts");
		expect(has(recognize_checked("chore (non-blocking): Later.\n"), feedback_diagnostic::label_expectation_conflict), "required work declared non-blocking is advisory");
		expect(has(recognize_checked("nitpick (blocking): Rename.\n"), feedback_diagnostic::label_expectation_conflict), "blocking nitpick is advisory");
		expect(!has(recognize_checked("issue (blocking): Guard.\n"), feedback_diagnostic::label_expectation_conflict), "unspecified guidance never conflicts");
		const auto conditional = recognize_checked("polish (if-minor): Simplify.");
		expect(conditional.Expectation == feedback_expectation::if_minor, "if-minor is conditional, not blocking or non-blocking");
		const auto conflict = recognize_checked("issue (non-blocking, BLOCKING, non-blocking): Decide.");
		expect(conflict.Expectation == feedback_expectation::conflicting && has(conflict, feedback_diagnostic::conflicting_decorations) && has(conflict, feedback_diagnostic::duplicate_decoration), "conflict is diagnosed, not last-token-wins");
		// The annotation has no field for shared resolution, a private defect
		// state or a review verdict: those facts belong to their own owners.
		static_assert(!carries_resolution<feedback_annotation> && !carries_defect_state<feedback_annotation> && !carries_verdict<feedback_annotation>);
	}

	// R04: decorations survive exactly as written, in order.
	void decorations_lossless()
	{
		const std::string body   = "issue ( Security,ux , perf_2,non-blocking, Security ): Check the length.";
		const auto        result = recognize_checked(body);
		expect(result.Status == feedback_prefix_status::recognized && result.Decorations.size() == 5U, "custom decorations recognized");
		const std::array expected{ "Security", "ux", "perf_2", "non-blocking", "Security" };
		for (std::size_t at = 0U; at < expected.size(); ++at) {
			expect(feedback_text(body, result.Decorations[at].Raw) == expected[at], "raw decoration text and order");
		}
		expect(result.Decorations[3].Kind == feedback_decoration_kind::non_blocking && result.Decorations[0].Kind == feedback_decoration_kind::custom, "standard token typed, custom kept as text");
		expect(result.Expectation == feedback_expectation::non_blocking && has(result, feedback_diagnostic::duplicate_decoration), "custom tokens never change expectation");
		expect(slice(body, result.Header) == "issue ( Security,ux , perf_2,non-blocking, Security ): ", "header span is the written header");
		const auto unknown = recognize_checked("observation (blocking, security): Keep this ordinary.");
		expect(unknown.Status == feedback_prefix_status::unknown_label && unknown.Decorations.size() == 2U && unknown.Expectation == feedback_expectation::unspecified, "unknown label borrows no blocking meaning");
		const auto underscored = recognize_checked("issue (non_blocking): Not the standard token.");
		expect(underscored.Expectation == feedback_expectation::unspecified && underscored.Decorations[0].Kind == feedback_decoration_kind::custom, "only the exact standard spelling is standard");
	}

	struct refusal_case
	{
		std::string_view       Body;
		feedback_prefix_status Status;
		std::string_view       RawLabel;
		feedback_diagnostic    Diagnostic;
	};

	// R02, R03: broken syntax after a label stays readable, never repaired.
	void malformed_and_unknown()
	{
		using enum feedback_prefix_status;
		using enum feedback_diagnostic;
		const std::array cases{
			refusal_case{ "issue (): x", malformed, "issue", empty_decoration_list },
			refusal_case{ "issue (a,,b): x", malformed, "issue", empty_decoration },
			refusal_case{ "issue (,a): x", malformed, "issue", empty_decoration },
			refusal_case{ "issue (a(b)): x", malformed, "issue", nested_decoration },
			refusal_case{ "issue (a: x", malformed, "issue", unclosed_decoration },
			refusal_case{ "issue (a\n): x", malformed, "issue", unclosed_decoration },
			refusal_case{ "issue (blocking", malformed, "issue", unclosed_decoration },
			refusal_case{ "issue (see above): x", malformed, "issue", invalid_decoration },
			refusal_case{ "issue (a.b): x", malformed, "issue", invalid_decoration },
			refusal_case{ "issue (blocking) : x", malformed, "issue", missing_colon },
			refusal_case{ "issue (blocking)", malformed, "issue", missing_colon },
			refusal_case{ "issue:x", malformed, "issue", malformed_header },
			refusal_case{ "Issue : x", malformed, "Issue", malformed_header },
			refusal_case{ "issue:\xC2\xA0x", malformed, "issue", malformed_header },
			refusal_case{ "issue:", malformed, "issue", empty_subject },
			refusal_case{ "issue:\r\nThe subject is on the next line.", malformed, "issue", empty_subject },
			refusal_case{ "**issue:**x", malformed, "issue", malformed_header },
			refusal_case{ "**issue:* x", malformed, "issue", malformed_header },
			refusal_case{ "**issue (blocking)**: x", malformed, "issue", missing_colon },
			refusal_case{ "**issue**: ", malformed, "issue", empty_subject },
		};
		for (const auto& item : cases) {
			const auto result = recognize_checked(item.Body);
			expect(result.Status == item.Status && result.Diagnostics.size() == 1U && result.Diagnostics[0] == item.Diagnostic, item.Body);
			expect(slice(item.Body, result.RawLabel) == item.RawLabel && !result.Label && result.Decorations.empty(), "raw token kept for explanation, nothing classified");
		}
		// The same breakage after a word outside the catalog is ordinary prose.
		for (const std::string_view prose : { "Hmm (see above): x", "https://example.com/a", "Summary:\n- one", "Note that this is prose.", "Issue tracker links are fine.", "**Note** this", "**Bold**text", "*issue:* single star", "issue", "issue is broken", "e.g.: x", "issué: x" }) {
			const auto result = recognize_checked(prose);
			expect(result.Status == feedback_prefix_status::plain && result.Diagnostics.empty(), prose);
		}
		const auto warning = recognize_checked("**Warning:** Unknown strong header.");
		expect(warning.Status == feedback_prefix_status::unknown_label && slice("**Warning:** Unknown strong header.", warning.RawLabel) == "Warning", "unknown strong header readable");
	}

	// R03: a label anywhere but the start of the first ordinary block never
	// classifies the outer body, for every label.
	void false_positives()
	{
		std::size_t checked = 0U;
		for (const auto& definition : feedback_label_catalog()) {
			const std::string header = std::string{ definition.Id } + " (blocking): Example only.";
			const std::array  wrapped{
				"> " + header,
				">" + header,
				"```\n" + header + "\n```",
				"~~~text\n" + header + "\n~~~",
				"```suggestion\n" + header + "\n```",
				"    " + header,
				"\t" + header,
				"   \t" + header,
				"<!-- x -->\n" + header,
				"<div>" + header + "</div>",
				"- " + header,
				"* " + header,
				"+ " + header,
				"1. " + header,
				"# " + header,
				"### " + header,
				"[" + header + "](https://example.invalid/)",
				"`" + header + "`",
				"Context first.\n\n" + header,
				"Context first.\n" + header,
				"_" + header + "_",
				"__" + std::string{ definition.Id } + ":__ subject",
				"\xC2\xA0" + header,
				"\xE2\x80\x8B" + header,
			};
			for (const auto& body : wrapped) {
				const auto result = recognize_checked(body);
				expect(result.Status == feedback_prefix_status::plain && !result.Label && !result.RawLabel, body);
				++checked;
			}
		}
		std::cout << "false-positive bodies: " << checked << '\n';
	}

	// Byte spans, UTF-8 and line endings.
	void spans_and_line_endings()
	{
		const std::string crlf   = "\xEF\xBB\xBF\r\n  \t\r\n   issue (blocking):  Preserve the bytes. \r\n\r\nDiscussion.\r\n";
		const auto        result = recognize_checked(crlf);
		expect(result.Status == feedback_prefix_status::recognized && result.Header->Begin == 13U, "BOM, blank lines and three spaces are skipped, not stripped");
		expect(slice(crlf, result.Header) == "issue (blocking):  " && slice(crlf, result.Subject) == "Preserve the bytes.", "CRLF subject excludes its terminator");
		expect(slice(crlf, result.Discussion) == "\r\nDiscussion.\r\n", "discussion keeps its CRLF bytes");
		const std::string unicode = "question: Does café_東京 retain its identity?\n";
		const auto        wide    = recognize_checked(unicode);
		expect(slice(unicode, wide.Subject) == "Does café_東京 retain its identity?" && wide.Subject->End == unicode.size() - 1U, "UTF-8 subject spans whole bytes");
		const std::string bare = "todo: Only a subject";
		expect(!recognize_checked(bare).Discussion && recognize_checked(bare).Subject->End == bare.size(), "no discussion without a line terminator");
	}

	auto padded_header(std::size_t aSubjectBegin) -> std::string
	{
		const std::string_view header = "issue (blocking): ";
		return std::string(aSubjectBegin - header.size(), '\n') + std::string{ header } + "Subject.";
	}

	// R40: the classifier budget, at the limit and one past it, never truncates.
	void budgets()
	{
		const auto atLimit = padded_header(maximum_feedback_prefix_bytes - 1U);
		const auto inside  = recognize_checked(atLimit);
		expect(inside.Status == feedback_prefix_status::recognized && inside.Subject->Begin == maximum_feedback_prefix_bytes - 1U, "subject at the last budget byte is recognized");
		const auto past    = padded_header(maximum_feedback_prefix_bytes);
		const auto limited = recognize_checked(past);
		expect(limited.Status == feedback_prefix_status::limited && !limited.Label && limited.Expectation == feedback_expectation::unspecified, "one byte past is limited, not non-blocking");
		expect(limited.InspectedBytes == maximum_feedback_prefix_bytes && limited.UserBody == feedback_span{ 0U, past.size() }, "the whole body stays the readable text");
		// A short body that ends inside the window is decided, not limited.
		const auto shortBody = std::string(maximum_feedback_prefix_bytes - 6U, '\n') + "issue:";
		expect(recognize_checked(shortBody).Status == feedback_prefix_status::malformed, "end of a short body is not a budget refusal");

		std::string list = "issue (";
		for (std::size_t at = 0U; at < maximum_feedback_decorations; ++at) {
			list += (at == 0U ? "d" : ",d") + std::to_string(at);
		}
		const auto full = recognize_checked(list + "): Many.");
		expect(full.Status == feedback_prefix_status::recognized && full.Decorations.size() == maximum_feedback_decorations, "decoration limit inclusive");
		const auto over = recognize_checked(list + ",extra): Many.");
		expect(over.Status == feedback_prefix_status::limited && over.Decorations.empty() && slice(list, over.RawLabel) == "issue", "one decoration past the limit is limited");

		const std::string longest(maximum_feedback_label_bytes, 'a');
		expect(recognize_checked(longest + ": x").Status == feedback_prefix_status::unknown_label, "longest word that can be a label");
		expect(recognize_checked(longest + "a: x").Status == feedback_prefix_status::plain, "a longer word is prose");

		const std::string longSubject = "issue: " + std::string(60U * 1024U, 'x') + "\nmore";
		const auto        wide        = recognize_checked(longSubject);
		expect(wide.Status == feedback_prefix_status::recognized && wide.Subject->End == longSubject.size() - 5U && wide.InspectedBytes < 16U, "a long subject is neither limited nor truncated");
	}

	// R40: pathological input does bounded work and never backtracks.
	void pathological()
	{
		constexpr std::size_t big = 1U << 20U;
		struct shaped
		{
			std::string            Body;
			feedback_prefix_status Status;
		};
		const std::array cases{
			shaped{ "issue (" + std::string(big, '('), feedback_prefix_status::malformed },
			shaped{ "issue " + std::string(big, ' ') + "(blocking): x", feedback_prefix_status::limited },
			shaped{ std::string(big, '\n') + "issue: x", feedback_prefix_status::limited },
			shaped{ std::string(big, '*'), feedback_prefix_status::plain },
			shaped{ "**" + std::string(big, 'a'), feedback_prefix_status::plain },
			shaped{ "issue (a" + std::string(big, ' '), feedback_prefix_status::limited },
			shaped{ "issue (a b" + std::string(big, ' '), feedback_prefix_status::limited },
			shaped{ "issue (" + std::string(big, 'a'), feedback_prefix_status::limited },
			shaped{ "issue:" + std::string(big, ' '), feedback_prefix_status::limited },
			shaped{ "**issue:" + std::string(big, '*'), feedback_prefix_status::malformed },
		};
		for (const auto& item : cases) {
			const auto result = recognize_checked(item.Body);
			expect(result.Status == item.Status, stable_name(result.Status));
		}
		std::string many = "issue (";
		for (std::size_t at = 0U; at < big / 2U; ++at) {
			many += "a,";
		}
		const auto result = recognize_checked(many);
		expect(result.Status == feedback_prefix_status::limited && result.InspectedBytes <= maximum_feedback_prefix_bytes, "many decorations stop at the count limit");
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
	};

	// R40: generated bodies over the grammar's own alphabet.
	void generated_properties()
	{
		const auto                 pieces = std::to_array<std::string_view>({
			"issue",
			"note",
			"Quibble",
			"word",
			"**",
			"*",
			" ",
			"  ",
			"\t",
			"(",
			")",
			",",
			", ",
			":",
			": ",
			"\n",
			"\r\n",
			"\r",
			"blocking",
			"non-blocking",
			"if-minor",
			"x",
			"\xC3\xA9",
			"\xEF\xBB\xBF",
			">",
			"`",
			"#",
			"-",
			"1.",
			"<",
			"[",
			"_",
		});
		generator                  random{ 1076U };
		std::array<std::size_t, 5> counts{ };
		for (std::size_t round = 0U; round < 40000U; ++round) {
			std::string body;
			const auto  length = random.next() % 24U;
			for (std::size_t at = 0U; at < length; ++at) {
				body += pieces[random.next() % pieces.size()];
			}
			const auto first = recognize_checked(body);
			const auto again = recognize_feedback_prefix(body);
			expect(first.Status == again.Status && first.Header == again.Header && first.Subject == again.Subject && first.Diagnostics == again.Diagnostics, "deterministic");
			++counts[static_cast<std::size_t>(first.Status)];
		}
		std::cout << "generated: recognized " << counts[0] << ", plain " << counts[1] << ", unknown_label " << counts[2] << ", malformed " << counts[3] << ", limited " << counts[4] << '\n';
		expect(counts[0] > 0U && counts[1] > 0U && counts[2] > 0U && counts[3] > 0U, "the generated corpus reaches every decided status");
	}

	// Managed envelopes: only the owner's validated span is the user body.
	void managed_segments()
	{
		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       stop = render_tour_stop("Why boundary", 2, "issue (blocking): Guard the callback.\n\nThe owner closes first.", navigation, "https://github.com/o/r/blob/1111111111111111111111111111111111111111/a.md?plain=1#L2-L4");
		expect(stop.has_value(), "tour stop renders");
		const auto mapped = map_tour_stop(*stop);
		expect(mapped.Narrative.has_value(), "tour owner maps the narrative");
		const feedback_span narrative{ mapped.Narrative->Begin, mapped.Narrative->End };
		const auto          labelled = recognize_feedback_segment(*stop, narrative);
		expect_invariants(*stop, labelled);
		expect(labelled.Status == feedback_prefix_status::recognized && labelled.Label == feedback_label::issue && labelled.Expectation == feedback_expectation::blocking, "labelled tour narrative recognized");
		expect(labelled.Header->Begin > narrative.Begin && labelled.Discussion->End == narrative.End && labelled.UserBody == narrative, "spans stay inside the narrative");
		expect(slice(*stop, labelled.Subject) == "Guard the callback.", "subject bytes from the published body");
		expect(recognize_checked(*stop).Status == feedback_prefix_status::plain, "the whole envelope is never classified by stripping");

		const auto plainStop = render_tour_stop("Why", 1, "Ordinary narrative.", navigation);
		const auto plainMap  = map_tour_stop(*plainStop);
		expect(recognize_feedback_segment(*plainStop, feedback_span{ plainMap.Narrative->Begin, plainMap.Narrative->End }).Status == feedback_prefix_status::plain, "plain tour root stays plain");

		const auto edited = *stop + "\nA website paragraph after the footer.\n";
		expect(!map_tour_stop(edited).Narrative, "edited envelope is not mapped");
		const auto unavailable = recognize_feedback_segment(edited, std::nullopt);
		expect_invariants(edited, unavailable);
		expect(unavailable.Status == feedback_prefix_status::plain && unavailable.Diagnostics == std::vector{ feedback_diagnostic::classification_unavailable } && !unavailable.UserBody, "missing segment is unavailable, not guessed");
		expect(has(recognize_feedback_segment("abc", feedback_span{ 2U, 9U }), feedback_diagnostic::classification_unavailable), "span outside the body refused");
		expect(has(recognize_feedback_segment("abc", feedback_span{ 2U, 1U }), feedback_diagnostic::classification_unavailable), "inverted span refused");

		// An off-hunk permalink body: the owner returns the prose suffix, so
		// its offset is the body size less the prose size.
		const std::string prose     = "question (non-blocking): Is this retained?";
		const std::string permalink = "https://github.com/o/r/blob/1111111111111111111111111111111111111111/a.md#L3\n\n" + prose;
		const auto        suffix    = recognize_feedback_segment(permalink, feedback_span{ permalink.size() - prose.size(), permalink.size() });
		expect(suffix.Status == feedback_prefix_status::recognized && suffix.Label == feedback_label::question && suffix.Header->Begin == permalink.size() - prose.size(), "permalink prose recognized at its own offset");
		expect(recognize_checked(permalink).Status == feedback_prefix_status::plain, "a URL scheme is not a label");
		const auto budgeted = std::string(2000U, 'x') + "\n\n" + std::string(maximum_feedback_prefix_bytes, '\n') + "issue: x";
		const auto window   = recognize_feedback_segment(budgeted, feedback_span{ 2002U, budgeted.size() });
		expect(window.Status == feedback_prefix_status::limited && window.InspectedBytes == maximum_feedback_prefix_bytes, "the budget counts from the segment start");
	}
}

int main(int aArgc, char** aArgv)
{
	expect(aArgc == 2, "usage: test_feedback_convention <conventional-comments pack directory>");
	const std::filesystem::path pack{ aArgv[1] };
	catalog_matches_pack(pack);
	parser_corpus(pack);
	every_label_every_form();
	separate_facts();
	decorations_lossless();
	malformed_and_unknown();
	false_positives();
	spans_and_line_endings();
	budgets();
	pathological();
	generated_properties();
	managed_segments();
	std::cout << "FEEDBACK_CONVENTION_OK\n";
	return EXIT_SUCCESS;
}
