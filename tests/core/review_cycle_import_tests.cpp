#include "bpr/core/review_cycle_import.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Attaching a round received from elsewhere (RFC-BPR-0056,
/// BPR-1162): only explicit project and revision mappings associate it with
/// a local cycle, the bytes it names must be exactly the mapped bytes, a
/// label or path never stands in for a mapping, and a fork starts local notes
/// of unknown provenance without touching the sender's round.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle import failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(stream.good(), "fixture readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto digest(const std::string_view aText) -> cycle_material_digest
	{
		return cycle_material_digest{ sha256({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() }) };
	}

	/// A local cycle with `aCaptures` captured revisions.
	auto cycle(const std::uint8_t aSeed, const std::string& aTitle, const int aCaptures) -> review_cycle_state
	{
		auto incarnation     = std::array<std::uint8_t, 32>{ };
		incarnation[0]       = 5;
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 2 }, .Incarnation = cycle_catalog_incarnation{ incarnation } };
		auto       state     = create({ .Authority = authority, .NextCycle = aSeed }, { .Authority = authority, .Title = aTitle, .Scope = { } })->Cycle;
		for (int index = 0; index < aCaptures; ++index) {
			auto material = std::array<std::uint8_t, 32>{ };
			material[0]   = static_cast<std::uint8_t>(index + 1);
			state         = apply(state, { .Subject = state.Subject, .ExpectedRevision = state.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = cycle_material_digest{ material }, .Entries = 4, .Coverage = cycle_capture_coverage::complete } } })
								.Accepted;
		}
		return state;
	}

	/// What a local revision retains: exactly the round's source files.
	auto observed_as_sent(const cycle_round_payload& aPayload, const cycle_source_revision_id aLocal) -> std::vector<cycle_mapped_file>
	{
		auto files = std::vector<cycle_mapped_file>{ };
		for (const auto& source : aPayload.Sources) {
			files.push_back({ .Local = aLocal, .Path = source.Path, .Blob = digest(source.Bytes) });
		}
		return files;
	}

	void associates_only_through_explicit_mappings(const std::filesystem::path& aExamples)
	{
		const auto round = decode_round_envelope(read_file(aExamples / "round.json"));
		expect(round.has_value(), "golden round");
		const auto& payload = round->Payload;
		const auto  summary = summarize_import(*round);
		expect(summary.ForeignCycle == "RC-17" && summary.ForeignRound == "F1" && summary.ForeignProject == "example-cache", "sender identities as sent");
		expect(foreign_revisions(payload) == std::vector<std::string>{ "R0" } && foreign_files(payload).size() == 4U, "one sender revision, four named files");
		expect(imported_project_key("example-cache") == imported_project_key("example-cache") && imported_project_key("example-cache") != imported_project_key("example-cache2"), "project key");

		// Two local cycles with the same title: the same alias in two projects.
		const auto first  = cycle(1, "cache", 1);
		const auto second = cycle(2, "cache", 1);
		const auto map    = std::vector{ workspace_import_mapping{ .Imported = imported_project_key("example-cache"), .Local = derive_workspace_project(first.Subject) } };
		const auto r0     = std::vector{ cycle_revision_mapping{ .Foreign = "R0", .Local = cycle_source_revision_id{ 1 } } };
		const auto seen   = observed_as_sent(payload, cycle_source_revision_id{ 1 });

		const auto attached = associate_import(summary, payload, first, map, r0, seen, std::nullopt);
		expect(attached && attached->Cycle == first.Subject && attached->Revisions == r0, "associated through the explicit mapping");
		expect(associate_import(summary, payload, second, map, r0, seen, std::nullopt).error().Problem == cycle_import_problem::unmapped_project, "the same title is not a mapping");
		expect(associate_import(summary, payload, first, { }, r0, seen, std::nullopt).error().Problem == cycle_import_problem::unmapped_project, "no mapping, no association");
		// A label that looks like a root is only a label.
		auto hinted          = payload;
		hinted.Project.Label = "/Users/alice/secret/cache";
		expect(associate_import(summary, hinted, second, { }, r0, seen, std::nullopt).error().Problem == cycle_import_problem::unmapped_project, "a path-like label resolves nothing");

		// Revisions: all named, each once, each captured locally.
		expect(associate_import(summary, payload, first, map, { }, seen, std::nullopt).error() == cycle_import_refusal{ .Problem = cycle_import_problem::unmapped_revision, .Detail = "R0" },
			   "an unmapped sender revision is named");
		expect(associate_import(summary, payload, first, map, { r0[0], r0[0] }, seen, std::nullopt).error().Problem == cycle_import_problem::invalid_mapping, "a revision mapped twice");
		expect(associate_import(summary, payload, first, map, { r0[0], { .Foreign = "R9", .Local = cycle_source_revision_id{ 1 } } }, seen, std::nullopt).error().Detail == "R9",
			   "a mapping for a revision the round does not name");
		expect(associate_import(summary, payload, first, map, { { .Foreign = "R0", .Local = cycle_source_revision_id{ 2 } } }, seen, std::nullopt).error().Problem == cycle_import_problem::invalid_mapping,
			   "a local revision never captured");
		const auto uncaptured = cycle(3, "cache", 0);
		const auto toEmpty    = std::vector{ workspace_import_mapping{ .Imported = imported_project_key("example-cache"), .Local = derive_workspace_project(uncaptured.Subject) } };
		expect(associate_import(summary, payload, uncaptured, toEmpty, r0, seen, std::nullopt).error().Problem == cycle_import_problem::invalid_mapping, "a cycle with nothing captured");

		// Mapping to mismatched source: one file's bytes differ, or it is absent.
		auto changed          = seen;
		changed[1].Blob       = digest("different bytes");
		const auto mismatched = associate_import(summary, payload, first, map, r0, changed, std::nullopt);
		expect(!mismatched && mismatched.error() == cycle_import_refusal{ .Problem = cycle_import_problem::mismatched_source, .Detail = seen[1].Path }, "mismatched bytes name the path");
		changed[1].Blob = std::nullopt;
		expect(associate_import(summary, payload, first, map, r0, changed, std::nullopt).error().Problem == cycle_import_problem::mismatched_source, "an absent file is not a match");

		// Once associated, the same association is accepted and another refused.
		const auto existing = std::optional{ *attached };
		expect(associate_import(summary, payload, first, map, r0, seen, existing) == attached, "repeating the association");
		const auto other = std::vector{ workspace_import_mapping{ .Imported = imported_project_key("example-cache"), .Local = derive_workspace_project(second.Subject) } };
		expect(associate_import(summary, payload, second, other, r0, seen, existing).error().Problem == cycle_import_problem::already_associated, "a second association refused");
		auto archived      = first;
		archived.Lifecycle = cycle_lifecycle::archived;
		expect(associate_import(summary, payload, archived, map, r0, seen, std::nullopt).error().Problem == cycle_import_problem::invalid_cycle, "an archived cycle");

		const auto stored = encode_import_association(*attached);
		expect(decode_import_association(stored, first.Subject.Authority) == existing && !decode_import_association("{}", first.Subject.Authority), "association stored exactly");
	}

	void forks_into_local_notes(const std::filesystem::path& aExamples)
	{
		const auto  round   = decode_round_envelope(read_file(aExamples / "round.json"));
		const auto& payload = round->Payload;
		const auto  local   = cycle(1, "cache", 1);
		const auto  mapping = cycle_import_association{ .Cycle = local.Subject, .Revisions = { { .Foreign = "R0", .Local = cycle_source_revision_id{ 1 } } } };
		auto        files   = std::vector<cycle_round_file>{ };
		for (const auto& source : payload.Sources) {
			files.push_back({ .Revision = cycle_source_revision_id{ 1 }, .Path = source.Path, .Blob = digest(source.Bytes), .Bytes = source.Bytes });
		}
		const auto plan = plan_import_fork(payload, mapping, files);
		expect(plan && plan->size() == 8U && plan->front().first == "I1" && plan->back().first == "I8", "one note per item, keyed by the sender's names");
		for (std::size_t index = 0; index < plan->size(); ++index) {
			const auto& [foreign, note] = (*plan)[index];
			expect(note.Body == payload.Items[index].Body && note.Provenance == authored_provenance::unknown, "exact body, unknown provenance");
		}
		const auto* range = std::get_if<cycle_source_anchor>(&plan->front().second.Primary);
		expect(range && range->Revision == cycle_source_revision_id{ 1 } && range->Start == 81U && range->End == 107U && range->Original == "    void insert(int key);\n", "range re-anchored on the mapped bytes");
		const auto* file = std::get_if<cycle_file_target>(&(*plan)[2].second.Primary);
		expect(file && file->Path == "src/client.cpp" && file->Revision == cycle_source_revision_id{ 1 }, "file location mapped");
		expect(valid_target(local, plan->front().second.Primary), "a valid local target");
		files[0].Bytes = "changed";
		files[0].Blob  = digest("changed");
		expect(plan_import_fork(payload, mapping, files).error().Problem == cycle_import_problem::mismatched_source, "a range over other bytes is refused");
	}
}

auto main(const int aCount, char** aArguments) -> int
{
	expect(aCount == 2, "usage: test <examples directory>");
	associates_only_through_explicit_mappings(aArguments[1]);
	forks_into_local_notes(aArguments[1]);
	std::cout << "review cycle import tests passed\n";
	return EXIT_SUCCESS;
}
