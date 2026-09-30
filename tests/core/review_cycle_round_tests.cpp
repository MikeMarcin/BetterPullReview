#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Frozen feedback rounds (RFC-BPR-0056, BPR-1158): the canonical
/// JSON profile, the pack's golden round, and assembly of one complete,
/// deterministic payload from exactly the selected records.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle round failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto digest(const std::string_view aText) -> cycle_material_digest
	{
		return cycle_material_digest{ sha256(bytes(aText)) };
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(stream.good(), "fixture readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto contains(const std::string_view aHaystack, const std::string_view aNeedle) -> bool
	{
		return aHaystack.find(aNeedle) != std::string_view::npos;
	}

	auto replace_once(std::string aText, const std::string_view aFrom, const std::string_view aTo) -> std::string
	{
		const auto at = aText.find(aFrom);
		expect(at != std::string::npos, "mutation target present");
		aText.replace(at, aFrom.size(), aTo);
		return aText;
	}

	// ---- canonical JSON ---------------------------------------------------

	void reads_strict_json_only()
	{
		const auto parse = [](const std::string_view aText) { return parse_cycle_json(aText, 1U << 20U); };
		expect(parse(R"({"a":1,"a":2})").error() == cycle_json_error::duplicate_key, "duplicate key refused");
		expect(parse(R"({"a":{"b":1,"b":1}})").error() == cycle_json_error::duplicate_key, "nested duplicate key refused");
		expect(parse(R"(["\ud800"])").error() == cycle_json_error::encoding, "lone high surrogate refused");
		expect(parse(R"(["\udc00x"])").error() == cycle_json_error::encoding, "lone low surrogate refused");
		expect(parse("[\"\xC0\xAF\"]").error() == cycle_json_error::encoding, "overlong UTF-8 refused");
		expect(parse("[\"\xED\xA0\x80\"]").error() == cycle_json_error::encoding, "encoded surrogate refused");
		expect(parse("[\"\xFF\"]").error() == cycle_json_error::encoding, "invalid byte refused");
		expect(parse("[1.0]").error() == cycle_json_error::number, "fraction refused");
		expect(parse("[1e3]").error() == cycle_json_error::number, "exponent refused");
		expect(parse("[9007199254740992]").error() == cycle_json_error::number, "integer beyond 2^53-1 refused");
		expect(parse("[9007199254740991]").has_value(), "largest exact integer admitted");
		expect(parse("[01]").error() == cycle_json_error::syntax, "leading zero refused");
		expect(parse("[\"a\tb\"]").error() == cycle_json_error::syntax, "raw control character refused");
		expect(parse("\xEF\xBB\xBF{}").error() == cycle_json_error::syntax, "byte-order mark refused");
		expect(parse("{} x").error() == cycle_json_error::syntax, "trailing content refused");
		expect(!parse("[NaN]").has_value() && !parse("[Infinity]").has_value(), "non-finite values refused");
		expect(parse_cycle_json("[1, 2]", 5U).error() == cycle_json_error::too_large, "size bound");
		expect(parse(std::string(32U, '[') + std::string(32U, ']')).has_value(), "32 levels admitted");
		expect(parse(std::string(33U, '[') + std::string(33U, ']')).error() == cycle_json_error::depth, "33 levels refused");
	}

	void writes_the_canonical_profile()
	{
		const auto parsed = parse_cycle_json("{ \"b\" : [ 2 , true , null ] ,\n \"a\" : \"\\u00e9\\u0001\\n\\\"\\\\\\/\\ud83d\\ude00\\u007f\" , \"\xC3\xA9\" : -3 }", 1U << 20U);
		expect(parsed.has_value(), "document parsed");
		const auto canonical = canonical_cycle_json(*parsed);
		expect(canonical.has_value(), "canonical form");
		// Keys sorted by bytes; \u0001 and \n escaped, / and U+007F literal, scalars as UTF-8.
		expect(*canonical == "{\"a\":\"\xC3\xA9\\u0001\\n\\\"\\\\/\xF0\x9F\x98\x80\x7F\",\"b\":[2,true,null],\"\xC3\xA9\":-3}", "exact canonical bytes");
		const auto reparsed = parse_cycle_json(*canonical, 1U << 20U);
		expect(reparsed && canonical_cycle_json(*reparsed) == canonical, "canonical form is a fixed point");
		expect(!canonical_cycle_json(cycle_json_value{ std::string{ "\xFF" } }), "invalid UTF-8 has no canonical form");
		const auto repeated = cycle_json_value{ cycle_json_value::object{ { .Key = "k", .Value = { } }, { .Key = "k", .Value = { } } } };
		expect(!canonical_cycle_json(repeated), "a repeated key has no canonical form");
		const auto raw = std::string{ "\r\n\t\0\xFF", 5U };
		expect(decode_cycle_base64(encode_cycle_base64(raw)) == raw, "base64 round trip");
		expect(!decode_cycle_base64("DQo=\n") && !decode_cycle_base64("DQp=") && !decode_cycle_base64("DQ o="), "noncanonical base64 refused");
	}

	// ---- the pack's golden round -----------------------------------------

	void reads_the_golden_round(const std::filesystem::path& aExamples)
	{
		const auto document = read_file(aExamples / "round.json");
		const auto decoded  = decode_round_envelope(document);
		expect(decoded.has_value(), "golden round admitted");
		expect(cycle_hex(decoded->Digest.bytes()) == "061dd80030f4082164650da96c80cc5984c2f238bf8413b5c5120beac8d86acd", "golden canonical digest");
		expect(round_digest(decoded->Canonical) == decoded->Digest, "digest covers the canonical bytes");
		const auto& payload = decoded->Payload;
		expect(payload.Items.size() == 8U && payload.Sources.size() == 4U && payload.Items[1].Suggestions.size() == 1U, "eight items, four sources");
		expect(payload.Items[1].Suggestions[0].Original == "\tentries.push_back(key);\r\n", "CRLF and tab bytes exact");
		expect(payload.Items[4].ActionOverride == cycle_round_action::discuss, "explicit item action");
		// The typed reading re-encodes to the same canonical bytes and digest.
		const auto envelope = encode_round_envelope(payload);
		expect(envelope && envelope->back() == '\n', "envelope written");
		const auto again = decode_round_envelope(*envelope);
		expect(again && again->Digest == decoded->Digest && again->Canonical == decoded->Canonical, "re-encoded envelope keeps the digest");
		expect(decode_round_payload(decoded->Canonical).has_value(), "stored canonical payload read");
		expect(decode_round_payload(document).error().Problem != cycle_round_decode_problem::json, "an envelope is not a payload");

		const auto refused = [&](const std::string& aDocument, const cycle_round_decode_problem aProblem, const char* aWhat) {
			const auto result = decode_round_envelope(aDocument);
			expect(!result.has_value(), aWhat);
			expect(result.error().Problem == aProblem, aWhat);
		};
		refused(replace_once(document, "Make insertion idempotent", "Make insertion idempotenT"), cycle_round_decode_problem::digest, "edited payload");
		refused(replace_once(document, "\"base64\": \"ICAgIHZvaWQgaW5zZXJ0KGludCBrZXkpOwo=\"", "\"base64\": \"ICAgIHZvaWQgaW5zZXJ0KGludCBrZXkpOwp=\""), cycle_round_decode_problem::bytes, "noncanonical base64");
		refused(replace_once(document, "\"length\": 26", "\"length\": 27"), cycle_round_decode_problem::bytes, "length mismatch");
		refused(replace_once(document, "\"schema\": \"bpr.cycle.round.v1\"", "\"schema\": \"bpr.cycle.round.v2\""), cycle_round_decode_problem::version, "payload version");
		refused(replace_once(document, "bpr.cycle.round-envelope.v1", "bpr.cycle.round-envelope.v9"), cycle_round_decode_problem::version, "envelope version");
		refused(replace_once(document, "\"instruction_set_complete\": true", "\"instruction_set_complete\": false"), cycle_round_decode_problem::digest, "incomplete instruction set under the old digest");
		refused(replace_once(document, "\"item_count\": 8", "\"item_count\": 7"), cycle_round_decode_problem::digest, "count edited under the old digest");
		refused(replace_once(document, "\"cycle_id\": \"RC-17\",", "\"cycle_id\": \"RC-17\", \"cycle_id\": \"RC-18\","), cycle_round_decode_problem::json, "duplicate key");
		refused(replace_once(document, "\"path\": \"tests/cache_test.cpp\",\n        \"source_revision_id\"", "\"path\": \"../cache_test.cpp\",\n        \"source_revision_id\""), cycle_round_decode_problem::path, "traversal");
		refused(replace_once(document, "\"path\": \"src/client.cpp\",\n        \"source_revision_id\"", "\"path\": \"C:/client.cpp\",\n        \"source_revision_id\""), cycle_round_decode_problem::path, "drive path");
		refused(replace_once(document, "\"id\": \"I3\"", "\"id\": \"I2\""), cycle_round_decode_problem::digest, "an edit without a new digest");
		// Self-consistent envelopes (digest recomputed) that break cross-record rules.
		const auto resealed = [&](auto aMutate, const cycle_round_decode_problem aProblem, const char* aWhat) {
			auto changed = payload;
			aMutate(changed);
			const auto sealed = encode_round_envelope(changed);
			expect(sealed.has_value(), aWhat);
			refused(*sealed, aProblem, aWhat);
		};
		resealed([](cycle_round_payload& aPayload) { aPayload.Items[2].Id = "I2"; }, cycle_round_decode_problem::duplicate, "duplicate item");
		resealed([](cycle_round_payload& aPayload) { aPayload.Items[2].Related = { "I9" }; }, cycle_round_decode_problem::relation, "dangling relation");
		resealed([](cycle_round_payload& aPayload) { aPayload.Items[1].Suggestions[0].Id = aPayload.Items[0].Suggestions[0].Id; }, cycle_round_decode_problem::duplicate, "duplicate suggestion");
		resealed([](cycle_round_payload& aPayload) { aPayload.Sources[1].Path = aPayload.Sources[0].Path; }, cycle_round_decode_problem::duplicate, "one file twice in a revision");
		resealed([](cycle_round_payload& aPayload) { aPayload.Sources[0].Blob = aPayload.Sources[1].Blob; }, cycle_round_decode_problem::range, "full file bytes that are not its blob");
		resealed([](cycle_round_payload& aPayload) { aPayload.Items[0].Suggestions[0].Original[4] = 'X'; }, cycle_round_decode_problem::range, "suggestion original not in its file");
		resealed([](cycle_round_payload& aPayload) { aPayload.Selection.Complete = false; }, cycle_round_decode_problem::completeness, "declared incomplete");
		resealed([](cycle_round_payload& aPayload) { aPayload.Selection.ItemCount = 7; }, cycle_round_decode_problem::completeness, "declared count differs");
		resealed([](cycle_round_payload& aPayload) { aPayload.Items[0].Anchors[0].Path = "a/../b"; }, cycle_round_decode_problem::path, "traversal inside an anchor");
		refused(replace_once(document, "\"payload_sha256\": \"061d", "\"payload_sha256\": \"061D"), cycle_round_decode_problem::identifier, "uppercase digest");
		refused("{\"schema\":1}", cycle_round_decode_problem::shape, "wrong envelope shape");
	}

	// ---- assembly ---------------------------------------------------------

	constexpr std::string_view header = "#pragma once\nstruct Cache {\n    void insert(int key);\n};\n";
	constexpr std::string_view body   = "#include \"cache.hpp\"\r\n\r\nvoid Cache::insert(int key) {\r\n\tentries.push_back(key);\r\n}\r\n";
	constexpr std::string_view secret = "/Users/alice/secret-client/cache";

	struct fixture
	{
		review_cycle_state                     Cycle;
		std::vector<cycle_round_item_material> Items;
		std::vector<cycle_round_file>          Files;

		auto anchor(const std::string_view aPath, const std::string_view aFile, const std::string_view aText) const -> cycle_source_anchor
		{
			const auto start = aFile.find(aText);
			expect(start != std::string_view::npos, "anchor text");
			auto made = make_anchor(cycle_source_revision_id{ 1 }, std::string{ aPath }, bytes(aFile), start, start + aText.size());
			expect(made.has_value(), "anchor");
			return *made;
		}

		auto begin(begin_cycle_feedback aCommand) -> cycle_round_item_material&
		{
			auto begun = begin_feedback(Cycle, Cycle.Revision, std::move(aCommand));
			expect(begun.has_value(), "note begun");
			Cycle = begun->Cycle;
			Items.push_back({ .Thread = begun->Thread, .History = { begun->Revision } });
			return Items.back();
		}

		static void change(cycle_round_item_material& aItem, const review_cycle_state& aCycle, const cycle_feedback_change& aChange, const cycle_feedback_revision* aNamed)
		{
			const auto outcome = apply_feedback(aCycle, aItem.Thread, aItem.Thread.Lineage.Version, aChange, aNamed);
			expect(outcome.has_value(), "note changed");
			aItem.Thread = outcome->Thread;
			if (outcome->Revision) {
				aItem.History.push_back(*outcome->Revision);
			}
		}

		static void suggest(cycle_round_item_material& aItem, const review_cycle_state& aCycle, const cycle_source_anchor& aSelection, std::string aReplacement)
		{
			const auto added = add_suggestion(aCycle, aItem.Thread, aItem.Thread.Lineage.Version, { .Selection = aSelection, .Replacement = std::move(aReplacement) }, aItem.Suggestions.size());
			expect(added.has_value(), "suggestion added");
			aItem.Thread = added->Thread;
			aItem.Suggestions.push_back(added->Suggestion);
		}
	};

	auto make_fixture() -> fixture
	{
		auto incarnation     = std::array<std::uint8_t, 32>{ };
		incarnation[0]       = 7;
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = cycle_catalog_incarnation{ incarnation } };
		auto       result    = fixture{ .Cycle = create({ .Authority = authority }, { .Authority = authority, .Title = "cache", .Scope = { } })->Cycle };
		// The root's absolute location is where to look again, never round content.
		auto bound = apply(result.Cycle, { .Subject = result.Cycle.Subject, .ExpectedRevision = result.Cycle.Revision, .Change = bind_cycle_workspace{ .Root = { .Location = std::string{ secret }, .Root = { .Volume = 1, .ObjectLow = 2 } } } });
		expect(bound.Changed, "bound");
		auto material = std::array<std::uint8_t, 32>{ };
		material[0]   = 9;
		auto captured = apply(bound.Accepted, { .Subject = bound.Accepted.Subject, .ExpectedRevision = bound.Accepted.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = cycle_material_digest{ material }, .Entries = 2, .Coverage = cycle_capture_coverage::complete, .WorkspaceVersion = 1 } } });
		expect(captured.Captured.has_value(), "R1 captured");
		result.Cycle = captured.Accepted;
		result.Files = {
			{ .Revision = cycle_source_revision_id{ 1 }, .Path = "include/cache.hpp", .Blob = digest(header), .Bytes = std::string{ header } },
			{ .Revision = cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp", .Blob = digest(body), .Bytes = std::string{ body } },
		};

		// I1: a range note with an included and a private related location, an
		// exact CRLF suggestion, an edited wording and hidden later turns.
		const auto interfaceLine = result.anchor("include/cache.hpp", header, "    void insert(int key);\n");
		auto&      first         = result.begin({ .Primary = interfaceLine,
												  .Related = { { .Target = result.anchor("src/cache.cpp", body, "\tentries.push_back(key);\r\n") },
															   { .Target = cycle_file_target{ .Revision = cycle_source_revision_id{ 1 }, .Path = "PRIVATE-NOTES.md", .Blob = digest("x") }, .Disclosure = cycle_reference_disclosure::private_only } },
												  .Body    = "suggestion: Return whether an entry was added." });
		fixture::change(first, result.Cycle, edit_feedback_turn{ .Latest = private_review_turn_revision_id{ 1 }, .Body = "issue: Return whether an entry was added; update callers." }, &first.History[0]);
		fixture::change(first, result.Cycle, append_feedback_turn{ .Author = { .Provenance = authored_provenance::adapter_observed, .Adapter = "codex-cli", .Provider = "openai", .Model = "gpt-6", .Effort = "high" }, .Body = "HIDDEN-ADAPTER-REPLY" }, nullptr);
		fixture::change(first, result.Cycle, append_feedback_turn{ .Author = { }, .Body = "HIDDEN-SCRATCH" }, nullptr);
		fixture::suggest(first, result.Cycle, result.anchor("src/cache.cpp", body, "\tentries.push_back(key);\r\n"), "\tif (contains(key)) {\r\n\t\treturn;\r\n\t}\r\n\tentries.push_back(key);\r\n");
		// I2: a whole-file note; I3 a module note with a fence; I4 the whole cycle.
		result.begin({ .Primary = cycle_file_target{ .Revision = cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp", .Blob = digest(body) }, .Body = "todo: Keep the CRLF line endings." });
		result.begin({ .Primary = cycle_module_target{ .Path = "src" }, .Body = "note: ```\ncode fence inside\n```" });
		result.begin({ .Primary = cycle_whole_target{ }, .Body = "question: Is the cache shared across threads?" });
		// I5 is never selected.
		result.begin({ .Primary = cycle_whole_target{ }, .Body = "UNSELECTED-SCRATCH" });
		return result;
	}

	auto selection_for() -> cycle_round_selection
	{
		return {
			.Source      = cycle_source_revision_id{ 1 },
			.Action      = cycle_round_action::implement,
			.Goal        = "Make insertion idempotent.",
			.Constraints = { "Preserve CRLF.", "Report tests actually run." },
			.WriteScope  = { "include/cache.hpp", "src/cache.cpp" },
			.Items       = {
				{ .Item = cycle_feedback_item_id{ 1 }, .Wording = private_review_turn_revision_id{ 2 }, .Suggestions = { { .Id = cycle_suggestion_id{ 1 }, .Revision = 1 } }, .Related = { cycle_feedback_item_id{ 2 } } },
				{ .Item = cycle_feedback_item_id{ 2 }, .Wording = private_review_turn_revision_id{ 1 } },
				{ .Item = cycle_feedback_item_id{ 3 }, .Wording = private_review_turn_revision_id{ 1 } },
				{ .Item = cycle_feedback_item_id{ 4 }, .Wording = private_review_turn_revision_id{ 1 }, .Action = cycle_round_action::discuss },
			},
			.Context = {
				{ .Revision = cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp", .Kind = cycle_context_kind::excerpt, .Start = 22, .End = 30 },
				{ .Revision = cycle_source_revision_id{ 1 }, .Path = "include/cache.hpp", .Kind = cycle_context_kind::full_file },
				{ .Revision = cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp", .Kind = cycle_context_kind::excerpt, .Start = 50, .End = 60 },
			},
		};
	}

	auto assemble(const fixture& aFixture, const cycle_round_selection& aSelection) -> std::expected<cycle_round_payload, cycle_round_refusal>
	{
		return assemble_round({ .Cycle = &aFixture.Cycle, .Round = cycle_round_id{ 1 }, .SourceCapture = aFixture.Cycle.Latest->Capture, .Selection = &aSelection, .Items = aFixture.Items, .Files = aFixture.Files });
	}

	void assembles_one_complete_payload()
	{
		const auto fixture   = make_fixture();
		const auto selection = selection_for();
		const auto payload   = assemble(fixture, selection);
		expect(payload.has_value(), "payload assembled");
		expect(payload->CycleId == "RC-1" && payload->RoundId == "F1" && payload->SourceRevision == "R1" && payload->Consistency == cycle_source_consistency::observed_stable, "round identity");
		expect(payload->Project.Id == cycle_wire_project(fixture.Cycle.Subject) && payload->Project.Label == "cache", "logical project, not a path");
		expect(payload->Items.size() == 4U && payload->Items[0].Id == "I1" && payload->Items[3].Id == "I4", "selected items in order");
		const auto& first = payload->Items[0];
		expect(first.Revision == 2U && first.Body == "issue: Return whether an entry was added; update callers.", "the selected wording, exactly");
		expect(first.Anchors.size() == 2U && first.Anchors[0].Kind == cycle_anchor_kind::range && first.Anchors[1].Path == "src/cache.cpp", "primary and included related location only");
		expect(first.Related == std::vector<std::string>{ "I2" }, "requested relationship");
		expect(first.Suggestions.size() == 1U && first.Suggestions[0].Id == "SG1.1" && first.Suggestions[0].Replacement == "\tif (contains(key)) {\r\n\t\treturn;\r\n\t}\r\n\tentries.push_back(key);\r\n",
			   "exact suggestion bytes");
		expect(payload->Items[1].Anchors[0].Kind == cycle_anchor_kind::file && payload->Items[2].Anchors[0].Kind == cycle_anchor_kind::module && payload->Items[3].Anchors[0].Kind == cycle_anchor_kind::cycle,
			   "file, module and cycle locations");
		expect(payload->Items[3].ActionOverride == cycle_round_action::discuss, "explicit item action");
		// Context: the two excerpts of one file merge into one source; the full file stays whole.
		expect(payload->Sources.size() == 2U && payload->Sources[0].Id == "S1" && payload->Sources[0].Kind == cycle_context_kind::excerpt && payload->Sources[0].Start == 22U && payload->Sources[0].End == 60U,
			   "one excerpt per file and revision");
		expect(payload->Sources[0].Bytes == body.substr(22, 38) && payload->Sources[1].Bytes == header, "source bytes exact");
		expect(payload->Selection.ItemCount == 4U && payload->Selection.SourceCount == 2U && payload->Selection.Complete, "declared complete");

		const auto canonical = encode_round_payload(*payload);
		expect(canonical.has_value(), "canonical payload");
		for (const auto hidden : { std::string_view{ "HIDDEN-ADAPTER-REPLY" }, std::string_view{ "HIDDEN-SCRATCH" }, std::string_view{ "UNSELECTED-SCRATCH" }, std::string_view{ "gpt-6" },
								   std::string_view{ "codex-cli" }, std::string_view{ "PRIVATE-NOTES" }, std::string_view{ "Return whether an entry was added.\"" }, secret, std::string_view{ "alice" } }) {
			expect(!contains(*canonical, hidden), "nothing unselected, private or absolute leaks");
		}
		// The payload reads back to itself, and its digest is pinned.
		const auto decoded = decode_round_payload(*canonical);
		expect(decoded && decoded->Payload == *payload, "payload reads back");
		expect(cycle_hex(round_digest(*canonical).bytes()) == "67b0832cd432a2f22984d1e897aa406938808f2e37c07b9427c9df1ade455e77", "golden assembled digest");
	}

	void identical_inputs_reuse_and_edits_differ()
	{
		const auto fixture   = make_fixture();
		auto       selection = selection_for();
		const auto first     = encode_round_payload(*assemble(fixture, selection));
		const auto second    = encode_round_payload(*assemble(fixture, selection));
		expect(first && first == second, "identical inputs, identical bytes");
		std::swap(selection.Items[1], selection.Items[2]);
		const auto reordered = encode_round_payload(*assemble(fixture, selection));
		expect(reordered && round_digest(*reordered) != round_digest(*first), "reordering changes the digest");
		std::swap(selection.Items[1], selection.Items[2]);
		selection.Items[3].Action = std::nullopt;
		expect(round_digest(*encode_round_payload(*assemble(fixture, selection))) != round_digest(*first), "an action change changes the digest");
	}

	void refuses_rather_than_substitutes()
	{
		auto fixture = make_fixture();
		auto refuse  = [&](const cycle_round_selection& aSelection, const cycle_round_problem aProblem, const char* aWhat) {
			const auto result = assemble(fixture, aSelection);
			expect(!result.has_value() && result.error().Problem == aProblem, aWhat);
			return result.error();
		};
		auto base = selection_for();

		auto stale             = base;
		stale.Items[0].Wording = private_review_turn_revision_id{ 1 };
		expect(refuse(stale, cycle_round_problem::stale_wording, "an older wording is not substituted").Item == 0U, "names the item");
		auto staleSuggestion                             = base;
		staleSuggestion.Items[0].Suggestions[0].Revision = 2;
		refuse(staleSuggestion, cycle_round_problem::stale_suggestion, "a suggestion revision that is not current");
		auto unknown          = base;
		unknown.Items[2].Item = cycle_feedback_item_id{ 42 };
		refuse(unknown, cycle_round_problem::unknown_item, "unknown note");
		auto dangling             = base;
		dangling.Items[1].Related = { cycle_feedback_item_id{ 5 } };
		refuse(dangling, cycle_round_problem::invalid_relation, "related note not in the round");
		auto repeated = base;
		repeated.Items.push_back(repeated.Items[0]);
		refuse(repeated, cycle_round_problem::invalid_selection, "a note selected twice");
		auto empty  = base;
		empty.Items = { };
		refuse(empty, cycle_round_problem::empty, "no note");
		auto missing       = base;
		missing.Context[0] = { .Revision = cycle_source_revision_id{ 1 }, .Path = "src/absent.cpp" };
		expect(refuse(missing, cycle_round_problem::unavailable_context, "context the revision does not hold").Context == 0U, "names the context");
		auto outside       = base;
		outside.Context[0] = { .Revision = cycle_source_revision_id{ 1 }, .Path = "/etc/passwd" };
		refuse(outside, cycle_round_problem::invalid_selection, "absolute context path");
		auto badScope       = base;
		badScope.WriteScope = { "../outside" };
		refuse(badScope, cycle_round_problem::invalid_selection, "write scope outside the project");
		auto badSource   = base;
		badSource.Source = cycle_source_revision_id{ 2 };
		refuse(badSource, cycle_round_problem::invalid_source, "an uncaptured source revision");
		auto badText = base;
		badText.Goal = "\xFF";
		refuse(badText, cycle_round_problem::invalid_text, "a goal that is not text");

		fixture.Items[1].Thread.Lineage.Lifecycle = private_thread_lifecycle::archived;
		refuse(base, cycle_round_problem::archived_item, "archived note");
		fixture.Items[1].Thread.Lineage.Lifecycle = private_thread_lifecycle::open;

		// Over the context budget: refused with the measure; admitted only when asked.
		fixture.Files.push_back({ .Revision = cycle_source_revision_id{ 1 }, .Path = "data/big.bin", .Blob = { }, .Bytes = std::string(default_round_context_bytes, 'x') });
		fixture.Files.back().Blob = digest(fixture.Files.back().Bytes);
		auto large                = base;
		large.Context.push_back({ .Revision = cycle_source_revision_id{ 1 }, .Path = "data/big.bin" });
		const auto over = refuse(large, cycle_round_problem::oversized, "context over budget");
		expect(over.Measure.ContextBytes > over.Measure.ContextBudget && over.Measure.Items == 4U, "measure disclosed");
		large.LargeContext = true;
		expect(assemble(fixture, large).has_value(), "large context admitted explicitly");
		auto many = base;
		many.Items.resize(maximum_round_items + 1U);
		expect(refuse(many, cycle_round_problem::oversized, "too many items").Measure.Items == maximum_round_items + 1U, "item count disclosed");
	}

	void freezes_only_a_flushed_current_draft()
	{
		expect(freeze_readiness({ .ShownRevision = 3 }, 3).has_value() == false, "flushed and current");
		expect(freeze_readiness({ .Composing = true, .ShownRevision = 3 }, 3) == cycle_round_problem::unflushed, "IME composition open");
		expect(freeze_readiness({ .SavePending = true, .ShownRevision = 3 }, 3) == cycle_round_problem::unflushed, "unsaved typing");
		expect(freeze_readiness({ .ShownRevision = 2 }, 3) == cycle_round_problem::stale_draft, "editor behind the stored draft");
	}

	void stores_the_selection_exactly()
	{
		const auto fixture   = make_fixture();
		auto       selection = selection_for();
		selection.Omissions  = { "Left out the thread-safety discussion." };
		const auto stored    = encode_round_selection(selection);
		expect(stored.has_value(), "draft body");
		expect(decode_round_selection(*stored) == selection, "draft round trip");
		expect(decode_round_selection("") == cycle_round_selection{ }, "empty draft");
		expect(!decode_round_selection("{\"schema\":\"bpr.cycle.round-draft.v1\"}"), "incomplete draft refused");
		auto huge          = selection;
		huge.Items[0].Item = cycle_feedback_item_id{ static_cast<std::uint64_t>(maximum_cycle_json_integer) + 1U };
		expect(!encode_round_selection(huge), "an identity beyond the exact range has no body");
	}

	void freezes_named_rounds_only()
	{
		const auto fixture = make_fixture();
		auto       begun   = begin_round_draft(fixture.Cycle, fixture.Cycle.Revision);
		expect(begun.has_value(), "draft");
		const auto& [cycle, draft] = *begun;
		auto material              = std::array<std::uint8_t, 32>{ };
		material[1]                = 3;
		const auto payload         = cycle_round_digest{ material };
		expect(freeze_named_round(cycle, draft, 1, payload, std::nullopt, cycle_round_id{ 2 }).error() == cycle_refusal::conflict, "a payload naming another round");
		const auto frozen = freeze_named_round(cycle, draft, 1, payload, std::nullopt, cycle_round_id{ 1 });
		expect(frozen && frozen->Round.Id.Id == cycle_round_id{ 1 }, "named round frozen");
		const auto again = freeze_named_round(frozen->Cycle, draft, 1, payload, frozen->Round, cycle_round_id{ 1 });
		expect(again && again->Reused && again->Round == frozen->Round, "identical freeze reuses the round");
	}
}

auto main(const int aCount, char** aArguments) -> int
{
	expect(aCount == 2, "usage: test <examples directory>");
	reads_strict_json_only();
	writes_the_canonical_profile();
	reads_the_golden_round(aArguments[1]);
	assembles_one_complete_payload();
	identical_inputs_reuse_and_edits_differ();
	refuses_rather_than_substitutes();
	freezes_only_a_flushed_current_draft();
	stores_the_selection_exactly();
	freezes_named_rounds_only();
	std::cout << "review cycle round tests passed\n";
	return EXIT_SUCCESS;
}
