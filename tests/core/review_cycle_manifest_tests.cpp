#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "bpr/core/review_cycle_manifest.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle manifest failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto digest(const std::uint8_t aSeed) -> cycle_material_digest
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return cycle_material_digest{ bytes };
	}

	auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	// --------------------------------------------------------------- index

	void put32(std::vector<std::uint8_t>& aOut, const std::uint32_t aValue)
	{
		for (auto shift = 24; shift >= 0; shift -= 8) {
			aOut.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
		}
	}
	void put16(std::vector<std::uint8_t>& aOut, const std::uint16_t aValue)
	{
		aOut.push_back(static_cast<std::uint8_t>(aValue >> 8U));
		aOut.push_back(static_cast<std::uint8_t>(aValue & 0xFFU));
	}

	struct index_row
	{
		std::string   Path;
		std::uint32_t Mode         = 0100644U;
		std::uint8_t  Seed         = 1;
		std::uint8_t  Stage        = 0;
		bool          SkipWorktree = false;
		bool          IntentToAdd  = false;
	};

	/// Builds index bytes the way Git lays them out, with a zero checksum.
	auto build_index(const std::uint32_t aVersion, const std::vector<index_row>& aRows, const std::string_view aExtension = { }) -> std::vector<std::uint8_t>
	{
		auto out = std::vector<std::uint8_t>{ 'D', 'I', 'R', 'C' };
		put32(out, aVersion);
		put32(out, static_cast<std::uint32_t>(aRows.size()));
		auto previous = std::string{ };
		for (const auto& row : aRows) {
			const auto start = out.size();
			for (int word = 0; word < 6; ++word) {
				put32(out, 0);
			}
			put32(out, row.Mode);
			put32(out, 0);
			put32(out, 0);
			put32(out, 0);
			out.push_back(row.Seed);
			out.insert(out.end(), 19, 0);
			const auto extended = row.SkipWorktree || row.IntentToAdd;
			put16(out, static_cast<std::uint16_t>((extended ? 0x4000U : 0U) | (static_cast<unsigned>(row.Stage) << 12U) | std::min<std::size_t>(row.Path.size(), 0xFFFU)));
			if (extended) {
				put16(out, static_cast<std::uint16_t>((row.SkipWorktree ? 0x4000U : 0U) | (row.IntentToAdd ? 0x2000U : 0U)));
			}
			if (aVersion == 4) {
				auto common = std::size_t{ 0 };
				while (common < previous.size() && common < row.Path.size() && previous[common] == row.Path[common]) {
					++common;
				}
				out.push_back(static_cast<std::uint8_t>(previous.size() - common));
				out.insert(out.end(), row.Path.begin() + static_cast<std::ptrdiff_t>(common), row.Path.end());
				out.push_back(0);
			} else {
				out.insert(out.end(), row.Path.begin(), row.Path.end());
				const auto fixed  = out.size() - start - row.Path.size();
				const auto padded = (fixed + row.Path.size() + 8U) & ~std::size_t{ 7U };
				out.insert(out.end(), padded - (out.size() - start), 0);
			}
			previous = row.Path;
		}
		if (!aExtension.empty()) {
			out.insert(out.end(), aExtension.begin(), aExtension.end());
			put32(out, 0);
		}
		out.insert(out.end(), 20, 0);
		return out;
	}

	void parses_index_versions()
	{
		const auto rows = std::vector<index_row>{
			{ .Path = "README.md" },
			{ .Path = "src/a.cpp", .Mode = 0100755U, .Seed = 2 },
			{ .Path = "src/b.cpp", .Seed = 3, .IntentToAdd = true },
			{ .Path = "vendor/lib", .Mode = 0160000U, .Seed = 4 },
			{ .Path = "z/conflict.txt", .Seed = 5, .Stage = 1 },
			{ .Path = "z/conflict.txt", .Seed = 6, .Stage = 2 },
			{ .Path = "z/sparse.txt", .Seed = 7, .SkipWorktree = true },
		};
		for (const auto version : { 2U, 3U, 4U }) {
			if (version == 2U) {
				continue; // extended flags need version 3 or later
			}
			const auto parsed = parse_git_index(build_index(version, rows), git_hash_algorithm::sha1, 100);
			expect(parsed.has_value() && parsed->Version == version && parsed->Entries.size() == rows.size(), "index parsed");
			expect(parsed->Entries[1].Mode == 0100755U && parsed->Entries[1].Object == blob(2), "mode and object");
			expect(parsed->Entries[2].IntentToAdd && parsed->Entries[6].SkipWorktree, "extended flags");
			expect(parsed->Entries[3].Mode == 0160000U, "gitlink");
			expect(parsed->Unmerged && parsed->Entries[4].Stage == 1 && parsed->Entries[5].Stage == 2, "conflict stages");
			expect(parsed->Entries[6].Path == "z/sparse.txt", "prefix-compressed path");
		}
		const auto plain = std::vector<index_row>{ { .Path = "a.txt" }, { .Path = "b/c.txt", .Seed = 9 } };
		const auto v2    = parse_git_index(build_index(2, plain), git_hash_algorithm::sha1, 10);
		expect(v2 && v2->Entries.size() == 2 && v2->Entries[1].Path == "b/c.txt" && !v2->Unmerged, "version 2");
		expect(parse_git_index(build_index(2, plain, "link"), git_hash_algorithm::sha1, 10)->SplitIndex, "split index flagged");
		expect(parse_git_index(build_index(2, plain, "sdir"), git_hash_algorithm::sha1, 10)->SparseDirectories, "sparse index flagged");
		expect(parse_git_index(build_index(2, plain), git_hash_algorithm::sha1, 1).error() == git_index_error::bounds, "entry bound");

		auto bad = build_index(2, plain);
		bad[0]   = 'X';
		expect(parse_git_index(bad, git_hash_algorithm::sha1, 10).error() == git_index_error::signature, "signature");
		auto version = build_index(2, plain);
		version[7]   = 9;
		expect(parse_git_index(version, git_hash_algorithm::sha1, 10).error() == git_index_error::unsupported_version, "version");
		auto truncated = build_index(2, plain);
		truncated.resize(40);
		expect(!parse_git_index(truncated, git_hash_algorithm::sha1, 10), "truncated");
		const auto unsorted = std::vector<index_row>{ { .Path = "b" }, { .Path = "a" } };
		expect(parse_git_index(build_index(2, unsorted), git_hash_algorithm::sha1, 10).error() == git_index_error::malformed, "unsorted");
		expect(!parse_git_index(build_index(2, plain), git_hash_algorithm::sha256, 10), "hash length follows the object format");
	}

	void reads_config_and_references()
	{
		expect(git_object_format("[core]\n\tbare = false\n") == git_hash_algorithm::sha1, "default SHA-1");
		expect(git_object_format("[core]\n[Extensions]\n\tObjectFormat = sha256\n") == git_hash_algorithm::sha256, "SHA-256, keys case-insensitive");
		expect(git_object_format("[extensions]\nobjectformat = \"sha256\"\n") == git_hash_algorithm::sha256, "quoted value");
		expect(git_object_format("[extensions]\nobjectformat = md5\n") == git_hash_algorithm::invalid, "unknown format");
		expect(git_object_format("[other]\nobjectformat = sha256\n") == git_hash_algorithm::sha1, "other sections ignored");

		const auto head = parse_git_head("ref: refs/heads/main\n", git_hash_algorithm::sha1);
		expect(head.Reference == "refs/heads/main" && !head.Commit, "symbolic HEAD");
		const auto hex      = std::string(40, 'a');
		const auto detached = parse_git_head(hex + "\n", git_hash_algorithm::sha1);
		expect(detached.Commit && !detached.Reference, "detached HEAD");
		expect(!parse_git_head(hex, git_hash_algorithm::sha256).Commit, "algorithm must match");
		expect(!parse_git_head("ref: ../../etc\n", git_hash_algorithm::sha1).Reference, "reference outside refs refused");
		expect(parse_git_reference(hex + "\n", git_hash_algorithm::sha1).has_value(), "loose reference");
		const auto packed = "# pack-refs with: peeled fully-peeled sorted\n" + std::string(40, 'b') + " refs/heads/other\n" + hex + " refs/heads/main\n^" + std::string(40, 'c') + "\n";
		expect(find_packed_reference(packed, "refs/heads/main", git_hash_algorithm::sha1) == parse_git_reference(hex, git_hash_algorithm::sha1), "packed reference");
		expect(!find_packed_reference(packed, "refs/heads/none", git_hash_algorithm::sha1), "absent packed reference");
	}

	// -------------------------------------------------------------- ignore

	void matches_git_wildcards()
	{
		expect(wildcard_match("*.log", "debug.log") && !wildcard_match("*.log", "dir/debug.log"), "star stops at slash");
		expect(wildcard_match("a?c", "abc") && !wildcard_match("a?c", "a/c"), "question mark");
		expect(wildcard_match("[a-c]x", "bx") && !wildcard_match("[!a-c]x", "bx") && wildcard_match("[^a-c]x", "dx"), "brackets");
		expect(wildcard_match("**/build", "build") && wildcard_match("**/build", "a/b/build"), "leading double star");
		expect(wildcard_match("logs/**", "logs/a/b.txt") && !wildcard_match("logs/**", "logs"), "trailing double star");
		expect(wildcard_match("a/**/b", "a/b") && wildcard_match("a/**/b", "a/x/y/b"), "middle double star");
		expect(wildcard_match("\\*literal", "*literal") && !wildcard_match("\\*literal", "xliteral"), "escape");
		expect(wildcard_match("foo**bar", "fooxbar") && !wildcard_match("foo**bar", "foo/bar"), "inline double star is a star");
		expect(!wildcard_match("[[:alpha:]]", "a"), "classes unsupported and match nothing");
		// [[BPR-0970]]. Many stars against a long name that never matches used
		// to backtrack exponentially: this pair would run for years. Git's
		// early stops answer it at once.
		const auto stars = std::string{ "*a*a*a*a*a*a*a*a*a*a*a*a*b" };
		expect(!wildcard_match(stars, std::string(400U, 'a')), "many stars against a long name end without backtracking");
		expect(!wildcard_match("**/" + stars, std::string(200U, 'a') + "/" + std::string(200U, 'a')), "and so do they below a double star");
		expect(wildcard_match(stars, std::string(400U, 'a') + "b"), "the same pattern still matches what it matches");
	}

	void applies_ignore_precedence()
	{
		auto rules = workspace_ignore_rules{ };
		expect(rules.add("", "# info/exclude\nsecret.env\n"), "info/exclude");
		expect(rules.add("", "*.log\n!keep.log\nbuild/\n/root-only.txt\ndocs/*.tmp\ntrailing\\ \nspaces   \n"), "root .gitignore");
		expect(rules.add("sub", "!*.log\nlocal/\n"), "nested .gitignore");
		expect(rules.ignored("debug.log", false) && !rules.ignored("keep.log", false), "negation");
		expect(rules.ignored("a/b/debug.log", false), "unanchored matches at depth");
		expect(!rules.ignored("sub/debug.log", false), "deeper file wins");
		expect(rules.ignored("build", true) && !rules.ignored("build", false), "directory-only rule");
		expect(rules.ignored("root-only.txt", false) && !rules.ignored("x/root-only.txt", false), "anchored to its base");
		expect(rules.ignored("docs/a.tmp", false) && !rules.ignored("docs/x/a.tmp", false), "path pattern");
		expect(rules.ignored("sub/local", true) && !rules.ignored("local", true), "nested base");
		expect(rules.ignored("secret.env", false), "info/exclude applies");
		expect(rules.ignored("trailing ", false) && rules.ignored("spaces", false), "escaped and trimmed trailing spaces");

		auto attributes = workspace_attribute_rules{ };
		expect(attributes.add("", "*.bin filter=lfs diff=lfs merge=lfs -text\n*.txt text\nkeep.bin -filter\nassets/ filter=lfs\n"), "attributes");
		expect(attributes.filter("a/b.bin") == true && attributes.filter("keep.bin") == false && !attributes.filter("a.txt"), "filter attribute");
		expect(!attributes.filter("assets/x.png"), "directory patterns never apply to contents");

		expect(excluded_by_default(".git") && excluded_by_default("node_modules") && excluded_by_default(".ssh"), "default set");
		expect(!excluded_by_default("build") && !excluded_by_default("src"), "ordinary names are not excluded");
		expect(default_exclusion_names().size() == 22, "the disclosed set is fixed");
	}

	// ------------------------------------------------------------ manifest

	auto sample_manifest() -> cycle_manifest
	{
		auto manifest = cycle_manifest{
			.ScopeVersion  = 2,
			.Scope         = { "docs", "src" },
			.RootKind      = workspace_root_kind::git_checkout,
			.Coverage      = cycle_capture_coverage::complete,
			.ObjectFormat  = git_hash_algorithm::sha1,
			.HeadReference = std::string{ "refs/heads/main" },
			.HeadCommit    = parse_git_reference(std::string(40, 'd'), git_hash_algorithm::sha1),
			.RefusedPaths  = 1,
		};
		manifest.Entries = {
			{ .Path = "docs/guide.md", .Origin = manifest_origin::tracked, .Mode = git_mode_regular, .Size = 3, .Content = digest(1), .Profile = classify_material({ }), .Staged = blob(7) },
			{ .Path = "src/link", .Kind = manifest_entry_kind::symlink, .Origin = manifest_origin::untracked, .Mode = git_mode_symlink, .Size = 6, .Content = digest(2) },
			{ .Path = "src/run.sh", .Origin = manifest_origin::tracked, .Mode = git_mode_executable, .Size = 3, .Content = digest(1), .Unmerged = true },
			{ .Path = "src/sparse", .Kind = manifest_entry_kind::directory, .Origin = manifest_origin::tracked, .Availability = manifest_availability::sparse },
			{ .Path = std::string{ "src/w\xFFird" }, .Origin = manifest_origin::untracked, .Availability = manifest_availability::too_large, .Mode = git_mode_regular, .Size = 999 },
		};
		manifest.Entries[0].Profile = { .Kind = material_text_kind::utf8, .LineEndings = material_line_endings::none, .Size = 3 };
		manifest.Entries[2].Profile = { .Kind = material_text_kind::utf8, .LineEndings = material_line_endings::lf, .FinalNewline = true, .Size = 3 };
		manifest.Deletions          = { "src/removed.cpp" };
		manifest.Exclusions         = { { .Path = "src/build", .Directory = true, .Reason = workspace_exclusion_reason::ignored }, { .Path = "src/node_modules", .Directory = true, .Reason = workspace_exclusion_reason::default_set } };
		return manifest;
	}

	void encodes_manifests_canonically()
	{
		const auto manifest = sample_manifest();
		expect(!validate_manifest(manifest), "sample valid");
		const auto encoded = encode_manifest(manifest);
		const auto decoded = decode_manifest(encoded);
		expect(decoded.has_value() && *decoded == manifest, "round trip");
		expect(encode_manifest(*decoded) == encoded, "canonical bytes");

		auto trailing = encoded;
		trailing.push_back(0);
		expect(decode_manifest(trailing).error() == manifest_error::format, "trailing bytes refused");
		auto cut = encoded;
		cut.resize(cut.size() - 3);
		expect(decode_manifest(cut).error() == manifest_error::truncated, "truncation refused");
		auto magic = encoded;
		magic[0]   = 'X';
		expect(decode_manifest(magic).error() == manifest_error::format, "magic refused");

		auto unsorted = manifest;
		std::swap(unsorted.Entries[0], unsorted.Entries[1]);
		expect(validate_manifest(unsorted) == manifest_error::paths, "unsorted entries refused");
		auto duplicate = manifest;
		duplicate.Deletions.emplace_back("src/run.sh");
		std::ranges::sort(duplicate.Deletions);
		expect(validate_manifest(duplicate) == manifest_error::paths, "deleted and present refused");
		auto escaping            = manifest;
		escaping.Entries[0].Path = "../escape";
		expect(validate_manifest(escaping) == manifest_error::paths, "unadmitted path refused");
		auto hollow = manifest;
		hollow.Entries[0].Content.reset();
		expect(validate_manifest(hollow) == manifest_error::paths, "captured entry needs content");

		const auto observation = observe_manifest(manifest, digest(9), 3);
		expect(observation.Entries == 5 && observation.ExplicitDeletions == 1 && observation.Unavailable == 2 && observation.ScopeVersion == 2, "summary counts");
		expect(observation.WorkspaceVersion == 3 && observation.Quality == cycle_capture_quality::observed_stable && observation.Material == digest(9), "summary identity");
		const auto content = manifest_content(manifest);
		expect(content.size() == 2 && content[0].first == digest(1) && content[1].first == digest(2), "distinct content, deduplicated");
		expect(find_entry(manifest, "src/run.sh") != nullptr && find_entry(manifest, "src/none") == nullptr, "lookup");
	}

	void compares_passes()
	{
		const auto first = std::vector<capture_stamp>{
			{ .Path = "a", .Identity = { .Volume = 1, .ObjectLow = 1 }, .Size = 1, .Modified = 5, .Changed = 5 },
			{ .Path = "b", .Identity = { .Volume = 1, .ObjectLow = 2 }, .Size = 2, .Modified = 5, .Changed = 5 },
		};
		expect(compare_capture_passes(first, first, 4).stable(), "identical passes");
		auto touched       = first;
		touched[1].Changed = 6;
		const auto changed = compare_capture_passes(first, touched, 4);
		expect(!changed.stable() && changed.Changed == 1 && changed.Examples == std::vector<std::string>{ "b" }, "a change stamp differs");
		auto grown = first;
		grown.push_back({ .Path = "c" });
		expect(compare_capture_passes(first, grown, 4).Changed == 1, "a new path appears");
		const auto shrunk = std::vector<capture_stamp>{ first[1] };
		expect(compare_capture_passes(first, shrunk, 0).Changed == 1 && compare_capture_passes(first, shrunk, 0).Examples.empty(), "a path vanishes; examples bounded");
	}
}

int main()
{
	parses_index_versions();
	reads_config_and_references();
	matches_git_wildcards();
	applies_ignore_precedence();
	encodes_manifests_canonically();
	compares_passes();
	return EXIT_SUCCESS;
}
