#include "bpr/core/review_cycle_candidate.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The squashed candidate and its transformation limits (BPR-1173):
/// one parent, exact metadata and message, a named LF normalization with its
/// record, and refusals that name the paths for filters, submodules,
/// signing, undecided improvements and an unchanged tree.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle candidate failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto digest(const std::uint8_t aSeed) -> cycle_material_digest
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[31]  = 0xD1;
		return cycle_material_digest{ bytes };
	}

	auto blob(const std::uint8_t aSeed) -> git_entry_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[19]  = 0xB1;
		return git_entry_oid{ git_hash_algorithm::sha1, bytes };
	}

	auto file(std::string aPath, const std::uint8_t aSeed, const bool aFilter = false) -> manifest_entry
	{
		return { .Path          = std::move(aPath),
				 .Origin        = manifest_origin::tracked,
				 .Mode          = git_mode_regular,
				 .Size          = 1,
				 .Content       = digest(aSeed),
				 .Profile       = { .Kind = material_text_kind::utf8, .Size = 1 },
				 .FilterManaged = aFilter };
	}

	auto capture(std::vector<manifest_entry> aEntries) -> cycle_manifest
	{
		std::ranges::sort(aEntries, { }, &manifest_entry::Path);
		return { .ScopeVersion = 1, .RootKind = workspace_root_kind::git_checkout, .Coverage = cycle_capture_coverage::complete, .ObjectFormat = git_hash_algorithm::sha1, .Entries = std::move(aEntries) };
	}

	auto parent() -> git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0xC0;
		return git_commit_oid{ git_hash_algorithm::sha1, bytes };
	}

	/// P holds a.cpp(1), crlf.txt(2), gone.txt(3); R0 is P; Rn edits a.cpp,
	/// rewrites crlf.txt with CRLF endings, deletes gone.txt and adds new.txt.
	auto request() -> outgoing_request
	{
		auto       entries = std::vector<git_tree_entry>{ { .Path = "a.cpp", .Mode = git_mode_regular, .Object = blob(1) },
														  { .Path = "crlf.txt", .Mode = git_mode_regular, .Object = blob(2) },
														  { .Path = "gone.txt", .Mode = git_mode_regular, .Object = blob(3) } };
		const auto tree    = build_git_trees(entries, git_hash_algorithm::sha1);
		auto       out     = outgoing_request{
			.StartRevision = cycle_source_revision_id{ 1 },
			.FinalRevision = cycle_source_revision_id{ 2 },
			.Start         = capture({ file("a.cpp", 1), file("crlf.txt", 2), file("gone.txt", 3) }),
			.Final         = capture({ file("a.cpp", 11), file("crlf.txt", 12), file("new.txt", 14) }),
			.Base          = { .Commit = parent(), .Tree = tree->Root, .Entries = std::move(entries) },
		};
		for (const auto seed : std::array<std::uint8_t, 5>{ 1, 2, 3, 11, 14 }) {
			out.Identities.push_back({ .Content = digest(seed), .Raw = blob(seed) });
		}
		out.Identities.push_back({ .Content = digest(12), .Raw = blob(12), .LineFeed = blob(22) });
		return out;
	}

	auto metadata() -> candidate_metadata
	{
		return { .Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000, .OffsetMinutes = 60 },
				 .Committer = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000500, .OffsetMinutes = 60 },
				 .Title     = "Harden parsing",
				 .Body      = "Validate input before use.\n\n" };
	}

	void builds_one_exact_commit()
	{
		const auto input = request();
		const auto plan  = plan_outgoing(input);
		expect(plan && plan->buildable(), "planned");
		const auto candidate = prepare_candidate(*plan, input.Identities, { .Metadata = metadata() });
		expect(candidate.has_value(), "candidate");
		expect(candidate->Parent == parent() && candidate->Content.Parents == std::vector{ parent() }, "one parent: P");
		expect(candidate->Content.Message == "Harden parsing\n\nValidate input before use.\n", "the confirmed message, no trailer added");
		expect(candidate->Content.Tree == candidate->Tree && git_commit_identity(candidate->Content) == candidate->Commit, "the commit identity is its content's");
		expect(build_git_trees(candidate->Entries, git_hash_algorithm::sha1)->Root == candidate->Tree && candidate->Entries == plan->Candidate, "the plan's tree");
		expect(candidate->Blobs.size() == 3U && candidate->Deleted == std::vector<std::string>{ "gone.txt" } && candidate->Transformations.empty(), "changes listed, nothing transformed");
		const auto preview = render_candidate_preview(*candidate);
		for (const auto text : { "Candidate commit ", "one parent (P ", "unsigned.", "Author: Ada Lovelace <ada@example.invalid> at 1700000000 UTC+1:00", "  Harden parsing",
								 "Transformations: none; every byte is as captured." }) {
			expect(preview.find(text) != std::string::npos, text);
		}

		// A named normalization changes bytes, so it changes the tree and commit.
		const auto normalized = prepare_candidate(*plan, input.Identities, { .NormalizeLineEndings = { "crlf.txt" }, .Metadata = metadata() });
		expect(normalized && normalized->Transformations.size() == 1U && normalized->Transformations[0].BeforeObject == blob(12) && normalized->Transformations[0].After == blob(22),
			   "the transformation is recorded");
		const auto entry = std::ranges::find(normalized->Entries, std::string_view{ "crlf.txt" }, &git_tree_entry::Path);
		expect(entry->Object == blob(22) && normalized->Tree != candidate->Tree && normalized->Commit != candidate->Commit, "and it is a different candidate");
		expect(std::ranges::find(normalized->Blobs, std::string_view{ "crlf.txt" }, &candidate_blob::Path)->LineFeed, "its bytes are normalized when stored");
		expect(render_candidate_preview(*normalized).find("CRLF -> LF  crlf.txt") != std::string::npos, "and previewed");

		expect(prepare_candidate(*plan, input.Identities, { .NormalizeLineEndings = { "a.cpp" }, .Metadata = metadata() }).error().Limit == candidate_limit::normalization,
			   "no normalization without CRLF text");
		expect(prepare_candidate(*plan, input.Identities, { .NormalizeLineEndings = { "gone.txt" }, .Metadata = metadata() }).error().Limit == candidate_limit::normalization,
			   "nor for a deletion");
		expect(prepare_candidate(*plan, input.Identities, { .NormalizeLineEndings = { "crlf.txt", "crlf.txt" }, .Metadata = metadata() }).error().Limit == candidate_limit::normalization,
			   "nor twice");
		auto badTitle  = metadata();
		badTitle.Title = "Two\nlines";
		expect(prepare_candidate(*plan, input.Identities, { .Metadata = badTitle }).error().Limit == candidate_limit::metadata, "a one-line title");
		auto badAuthor        = metadata();
		badAuthor.Author.Name = "";
		expect(prepare_candidate(*plan, input.Identities, { .Metadata = badAuthor }).error().Limit == candidate_limit::metadata, "a named author");
		expect(prepare_candidate(*plan, input.Identities, { .Metadata = metadata(), .Requirements = { .SigningRequired = true } }).error().Limit == candidate_limit::signing_required,
			   "required signing is not silently skipped");
		expect(candidate_message("Title", "") == "Title\n" && !candidate_message(" Title", "") && !candidate_message("", "body"), "messages");
	}

	void refuses_what_it_cannot_build()
	{
		auto input = request();
		input.Final.Entries.push_back(file("model.bin", 15, true));
		std::ranges::sort(input.Final.Entries, { }, &manifest_entry::Path);
		input.Identities.push_back({ .Content = digest(15), .Raw = blob(15) });
		const auto plan = plan_outgoing(input);
		expect(plan && plan->Blocked == 1U, "an LFS path is selected but blocked");
		const auto refused = prepare_candidate(*plan, input.Identities, { .Metadata = metadata() });
		expect(!refused && refused.error().Limit == candidate_limit::filter_managed && refused.error().Paths == std::vector<std::string>{ "model.bin" }, "named, not uploaded raw");
		const auto external = prepare_external(*plan, { .SigningRequired = true });
		expect(external && external->ToolingPaths == std::vector<std::string>{ "model.bin" } && external->Parent == parent(), "external preparation keeps it for the reviewer's tools");
		expect(std::ranges::find(external->Entries, std::string_view{ "model.bin" }, &git_tree_entry::Path) != external->Entries.end() && external->Tree == build_git_trees(external->Entries, git_hash_algorithm::sha1)->Root,
			   "with its bytes in the prepared tree");
		for (const auto text : { "this is not native publication", "stage them through your filter:\n  model.bin", "requires signed commits", "externally published" }) {
			expect(external->Instructions.find(text) != std::string::npos, text);
		}

		auto choose  = request();
		choose.Mode  = outgoing_selection_mode::improvements_only;
		choose.Start = capture({ file("a.cpp", 9), file("crlf.txt", 2), file("gone.txt", 3) });
		choose.Identities.push_back({ .Content = digest(9), .Raw = blob(9) });
		const auto undecided = plan_outgoing(choose);
		expect(undecided && prepare_candidate(*undecided, choose.Identities, { .Metadata = metadata() }).error().Limit == candidate_limit::undecided, "an undecided improvement");
		expect(prepare_external(*undecided, { }).error().Limit == candidate_limit::undecided, "for external preparation too");

		auto same            = request();
		same.Final           = same.Start;
		const auto unchanged = plan_outgoing(same);
		expect(unchanged && prepare_candidate(*unchanged, same.Identities, { .Metadata = metadata() }).error().Limit == candidate_limit::no_change, "no empty commit");
		expect(prepare_external(*unchanged, { }).error().Limit == candidate_limit::no_change, "nor an empty patch");
	}
}

auto main() -> int
{
	builds_one_exact_commit();
	refuses_what_it_cannot_build();
	std::cout << "review-cycle-candidate-tests:pass\n";
	return EXIT_SUCCESS;
}
