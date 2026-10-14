#include "../support/git_metadata_writers.hpp"
#include "bpr/core/git_config.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "bpr/core/review_cycle_manifest.hpp"
#include "bpr/core/sha1.hpp"
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

		// [[BPR-1454]]. The trailer is verified; all zero is index.skipHash.
		auto hashed = build_index(2, plain);
		hashed.resize(hashed.size() - 20U);
		const auto trailer = sha1(hashed).value();
		hashed.insert(hashed.end(), trailer.begin(), trailer.end());
		expect(parse_git_index(hashed, git_hash_algorithm::sha1, 10).has_value(), "a matching SHA-1 trailer");
		hashed[20] ^= 1U;
		expect(parse_git_index(hashed, git_hash_algorithm::sha1, 10).error() == git_index_error::checksum, "a changed byte fails the checksum");
		expect(git_index_problem(git_index_error::checksum).Reason == git_repository_refusal::index_checksum, "named index checksum refusal");
		// Required (lower-case) extensions other than link and sdir are refused.
		expect(parse_git_index(build_index(2, plain, "TREE"), git_hash_algorithm::sha1, 10).has_value(), "optional extension skipped");
		expect(parse_git_index(build_index(2, plain, "abcd"), git_hash_algorithm::sha1, 10).error() == git_index_error::required_extension, "required extension refused");
		expect(parse_git_index(build_index(2, plain, "1xyz"), git_hash_algorithm::sha1, 10).error() == git_index_error::required_extension, "non-letter signature refused");
	}

	/// [[BPR-1457]]. The round trips the fuzz target asserts, on fixed inputs.
	void round_trips_written_metadata()
	{
		const auto rows = std::vector<index_row>{
			{ .Path = "a.txt" },
			{ .Path = "dir/sub/one.txt", .Seed = 2, .SkipWorktree = true },
			{ .Path = "dir/sub/two.txt", .Seed = 3, .IntentToAdd = true },
			{ .Path = "z", .Seed = 4, .Stage = 2 },
		};
		for (const auto version : { 3U, 4U }) {
			const auto parsed = parse_git_index(build_index(version, rows), git_hash_algorithm::sha1, 10);
			expect(parsed.has_value(), "fixture index parses");
			for (const auto zero : { true, false }) {
				const auto again = parse_git_index(bpr::tests::write_git_index(*parsed, git_hash_algorithm::sha1, zero), git_hash_algorithm::sha1, 10);
				expect(again && again->Entries == parsed->Entries && again->Version == version, "an index written back parses the same");
			}
		}
		const auto config = parse_git_config("top = 1\n[a.B]\nk = \" x \\\" \\n\"\n[ \"q\"]\nflag\n[s \"Sub \\\"x\"]\nv = tab\\there\n");
		expect(config && parse_git_config(bpr::tests::write_git_config(*config)) == config, "a config written back parses the same");
	}

	void parses_git_config_syntax()
	{
		const auto entries = parse_git_config("\xEF\xBB\xBF# c\n[Core]\n\tIgnoreCase = \"tr\"\\\n\"ue\" ; note\n[remote \"Origin\"] url = a  b\t c # x\n[a.B]\nbare\n");
		expect(entries && entries->size() == 3, "three entries");
		expect((*entries)[0].Section == "core" && !(*entries)[0].Subsection && (*entries)[0].Key == "ignorecase" && (*entries)[0].Value == "true", "continued quoted value, comment dropped");
		expect((*entries)[1].Subsection == "Origin" && (*entries)[1].Value == "a  b  c", "quoted subsection keeps case; white space becomes spaces");
		expect((*entries)[2].Section == "a" && (*entries)[2].Subsection == "b" && !(*entries)[2].Value, "dotted subsection folds; bare key");
		for (const auto* bad : { "[core ]\n", "[core]\nkey # c\n", "[core]\nkey = \"open\n", "[core]\nkey = \\q\n", "[]\n", "[core]\n1key = x\n", "\xEF\xBBx\n" }) {
			expect(!parse_git_config(bad), "Git refuses this config");
		}
		expect(git_config_bool(std::nullopt) == true && git_config_bool(std::string{ }) == false && git_config_bool(std::string{ "On" }) == true, "booleans");
		expect(git_config_bool(std::string{ "2k" }) == true && git_config_bool(std::string{ "0" }) == false && !git_config_bool(std::string{ "maybe" }), "numeric booleans");
		expect(git_config_int(std::string{ "1k" }) == 1024 && git_config_int(std::string{ "0x10" }) == 16 && git_config_int(std::string{ "010" }) == 8 && !git_config_int(std::string{ "1x" }), "integers");
	}

	void reads_repository_formats()
	{
		const auto format = [](const std::string_view aConfig) { return read_git_repository_format(aConfig); };
		expect(format("[core]\n\tbare = false\n")->ObjectFormat == git_hash_algorithm::sha1, "default SHA-1");
		expect(format("[core]\nrepositoryformatversion = 1\n[Extensions]\n\tObjectFormat = sha256 ; note\n")->ObjectFormat == git_hash_algorithm::sha256, "SHA-256 with an inline comment");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\nobjectformat = \"sha256\"\n")->ObjectFormat == git_hash_algorithm::sha256, "quoted value");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\nobjectformat = SHA256\n").error().Reason == git_repository_refusal::object_format, "values are case-sensitive");
		expect(format("[core]\nrepositoryformatversion = 2\n").error().Reason == git_repository_refusal::format_version, "version above 1");
		expect(format("[core]\nrepositoryformatversion = -1\n").error().Reason == git_repository_refusal::format_version, "negative version");
		const auto unknown = format("[core]\nrepositoryformatversion = 1\n[extensions]\nfuture = x\n");
		expect(unknown.error().Reason == git_repository_refusal::unknown_extension && unknown.error().Key == "extensions.future", "unknown extension named");
		expect(format("[core]\nrepositoryformatversion = 0\n[extensions]\nfuture = x\n").has_value(), "Git ignores unknown extensions in version 0");
		expect(format("[core]\nrepositoryformatversion = 0\n[extensions]\nobjectformat = sha256\n").error().Reason == git_repository_refusal::version_zero_extension, "v1 extension in v0");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\nrefstorage = reftable\n").error().Reason == git_repository_refusal::reftable, "reftable refused (Q2 a)");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\nrefstorage = files\n").has_value(), "files storage");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\nrefstorage = other\n").error().Reason == git_repository_refusal::ref_storage, "unknown storage");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\ncompatobjectformat = sha256\n").error().Reason == git_repository_refusal::unknown_extension, "compatObjectFormat declined");
		const auto allowed = format("[core]\nrepositoryformatversion = 1\n[extensions]\nnoop\nnoop-v1 = x\npartialclone = origin\npreciousobjects\nworktreeconfig = true\nrelativeworktrees = yes\n");
		expect(allowed && allowed->WorktreeConfig, "the allowlist");
		expect(format("[core]\nrepositoryformatversion = 1\n[extensions]\nworktreeconfig = maybe\n").error().Reason == git_repository_refusal::invalid_value, "bad boolean extension");
		expect(format("[core]\nignorecase = true\nignorecase = false\n")->IgnoreCase == false && format("[core]\nignorecase\n")->IgnoreCase, "ignoreCase, last wins");
		expect(format("[core]\nignorecase = maybe\n").error().Key == "core.ignorecase", "bad ignoreCase refused");
		expect(format("[core]\nkey # c\n").error().Reason == git_repository_refusal::malformed_config, "malformed config refused");
		expect(format("[include]\npath = elsewhere\n[core]\nignorecase = false\n")->IgnoreCase == false, "includes are not followed");
		auto worktree = *format("[core]\nrepositoryformatversion = 1\nignorecase = false\n[extensions]\nworktreeconfig = true\n");
		expect(!apply_git_worktree_config("[core]\nignorecase = true\n", worktree) && worktree.IgnoreCase, "config.worktree wins");
	}

	void reads_config_and_references()
	{
		const auto head = parse_git_head("ref: refs/heads/main\n", git_hash_algorithm::sha1);
		expect(head.Reference == "refs/heads/main" && !head.Commit, "symbolic HEAD");
		const auto hex      = std::string(40, 'a');
		const auto detached = parse_git_head(hex + "\n", git_hash_algorithm::sha1);
		expect(detached.Commit && !detached.Reference, "detached HEAD");
		expect(!parse_git_head(hex, git_hash_algorithm::sha256).Commit, "algorithm must match");
		expect(!parse_git_head("ref: ../../etc\n", git_hash_algorithm::sha1).Reference, "reference outside refs refused");
		expect(parse_git_reference(hex + "\n", git_hash_algorithm::sha1).has_value(), "loose reference");
		expect(parse_git_reference(hex + " trailing data\n", git_hash_algorithm::sha1).has_value() && !parse_git_reference(" " + hex, git_hash_algorithm::sha1), "Git's object-name rule");
		expect(parse_git_head("ref:refs/heads/x \n", git_hash_algorithm::sha1).Reference == "refs/heads/x", "ref: without a space");
		expect(!parse_git_head("ref: refs/heads/a\nrefs/heads/b", git_hash_algorithm::sha1).Reference, "a name across lines is not a reference");
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
		// [[BPR-1454]]. POSIX classes, and Git's abort rules for bad brackets.
		expect(wildcard_match("a[[:space:]]b", "a b") && !wildcard_match("x[![:space:]]y", "x y"), "space class and its negation");
		for (const auto* name : { "alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit" }) {
			expect(wildcard_match(std::string{ "[[:" } + name + ":]x]", "x"), "every class name is known");
		}
		expect(wildcard_match("[[:digit:][:upper:]]", "7") && !wildcard_match("[[:digit:]]", "a"), "classes combine");
		expect(!wildcard_match("[[:nonsense:]]", "a") && !wildcard_match("[[:nonsense:]]", "["), "unknown class matches nothing");
		expect(!wildcard_match("a[b", "a[b") && !wildcard_match("a\\", "a\\"), "unterminated bracket and trailing escape match nothing");
		expect(wildcard_match("[]a]", "]") && wildcard_match("[a-]", "-") && wildcard_match("[[:x]", ":"), "literal members");
		// Case folding exactly as Git's WM_CASEFOLD, including its quirks.
		expect(wildcard_match("Secret.env", "secret.env", true) && !wildcard_match("Secret.env", "secret.env"), "fold");
		expect(wildcard_match("x[a-c]y", "xBy", true) && wildcard_match("q[B-D]q", "qcq", true) && wildcard_match("[[:upper:]]up", "lup", true), "ranges and upper fold");
		expect(!wildcard_match("[A]bc", "Abc", true) && !wildcard_match("\\Kk", "Kk", true), "single bracket members and escapes keep case");
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

		auto folded = workspace_ignore_rules{ true };
		expect(folded.add("", "Secret.env\nBuild/\n") && folded.ignored("secret.env", false) && folded.ignored("build", true), "core.ignoreCase folds ignore rules");
		// An anchored pattern's literal part is compared on its own, so a `**`
		// right after it starts a segment, as in Git's match_pathname.
		auto prefixed = workspace_ignore_rules{ };
		expect(prefixed.add("", "config/local**/*.pem\nlogs/2024**/debug.log\n/out**/x\n"), "prefixed double stars");
		expect(prefixed.ignored("config/local/a/b/key.pem", false) && prefixed.ignored("logs/2024/01/debug.log", false) && prefixed.ignored("out/a/x", false), "a double star after the literal part crosses directories");
		auto trims = workspace_ignore_rules{ };
		expect(trims.add("", "back\\\\  \n") && trims.ignored("back\\", false), "an escaped backslash does not protect the trailing space");

		using kind      = git_attribute_state::kind;
		auto attributes = workspace_attribute_rules{ };
		expect(attributes.add("", "*.bin filter=lfs diff=lfs merge=lfs -text\n*.txt text\nkeep.bin -filter\nassets/ filter=lfs\n"), "attributes");
		expect(attributes.filter("a/b.bin") == git_attribute_state{ .Kind = kind::value, .Value = "lfs" } && attributes.filter("keep.bin").Kind == kind::unset, "filter attribute");
		expect(attributes.filter("a.txt").Kind == kind::unspecified, "unspecified");
		expect(attributes.filter("assets/x.png").Kind == kind::unspecified, "directory patterns never apply to contents");

		// A NUL ends its own line only; a quoted macro's name ends at a blank.
		auto nul = workspace_attribute_rules{ };
		expect(nul.add("", std::string{ "a\0b filter=x\n*.txt filter=lfs\n", 30 }) && nul.filter("foo.txt").Value == "lfs", "a NUL ends its line, not the file");
		auto macros = workspace_attribute_rules{ };
		expect(macros.add("", "\"[attr]m1 junk\" filter=mac\n*.m m1\n[attr]outer inner\n[attr]inner filter=deep\n*.d outer\n*.e filter=own outer\n*.f -outer\n"), "macros");
		expect(macros.filter("x.m").Value == "mac" && macros.filter("x.d").Value == "deep", "quoted and nested macros expand");
		expect(macros.filter("x.e").Value == "deep" && macros.filter("x.f").Kind == kind::unspecified, "the rightmost assignment wins; an unset macro does not expand");
		// A long chain of macros expands without deep recursion.
		auto chain = std::string{ };
		for (int link = 0; link < 50'000; ++link) {
			chain += "[attr]m" + std::to_string(link) + " m" + std::to_string(link + 1) + "\n";
		}
		chain += "[attr]m50000 filter=end\n*.c m0\n";
		auto chained = workspace_attribute_rules{ };
		expect(chained.add("", chain) && chained.filter("x.c").Value == "end", "a long macro chain");

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
	parses_git_config_syntax();
	round_trips_written_metadata();
	reads_repository_formats();
	reads_config_and_references();
	matches_git_wildcards();
	applies_ignore_precedence();
	encodes_manifests_canonically();
	compares_passes();
	return EXIT_SUCCESS;
}
