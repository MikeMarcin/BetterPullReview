#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_outgoing.hpp"
#include "cycle_course.hpp"
#include "publication_course.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Isolated squashed candidates through the production host and the
/// installed-Git publication catalog (RFC-BPR-0059, BPR-1173): the user's
/// index, references and files hashed before and after, configuration, hook
/// and filter canaries in the checkout that never fire, a named LF
/// normalization, binary, executable and link entries, an LFS path refused
/// natively and prepared externally as a patch that ordinary Git applies,
/// and SHA-1 and SHA-256 candidates an independent repository reads back
/// exactly.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_outgoing_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::publication_git;
	using tests::workspace_contract::snapshot;
	using tests::workspace_contract::write_file;

	constexpr std::string_view pointer = "version https://git-lfs.github.com/spec/v1\n"
										 "oid sha256:4d7a214614ab2935c943f9e0ff69d22eadbb8f32b1258daaa5e2ca24d17e2393\n"
										 "size 4096\n";

	auto binary(const std::size_t aSize, const std::uint8_t aSeed) -> std::string
	{
		auto bytes = std::string(aSize, '\0');
		for (std::size_t index = 0; index < aSize; ++index) {
			bytes[index] = static_cast<char>(index % 7U == 0U ? 0U : (aSeed + index) & 0xFFU);
		}
		return bytes;
	}

	auto metadata() -> core::candidate_metadata
	{
		return { .Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000, .OffsetMinutes = 120 },
				 .Committer = { .Name = "BPR Reviewer", .Email = "reviewer@example.invalid", .Seconds = 1700000900, .OffsetMinutes = -300 },
				 .Title     = "Harden the parser",
				 .Body      = "Reviewed locally in two rounds." };
	}

	/// A script that records that it ran, for every configuration and hook
	/// slot the checkout offers.
	struct canaries
	{
		std::filesystem::path Directory;

		[[nodiscard]] auto fired() const -> bool
		{
			return std::filesystem::exists(Directory / "fired");
		}
	};

	auto plant_canaries(const std::filesystem::path& aRoot, const std::filesystem::path& aDirectory) -> canaries
	{
		std::filesystem::create_directories(aDirectory / "hooks");
		const auto fired  = (aDirectory / "fired").generic_string();
		const auto script = "#!/bin/sh\necho ran >> \"" + fired + "\"\ncat\n";
		const auto path   = aDirectory / "canary.sh";
		write_file(path, script);
		for (const auto hook : { "pre-commit", "commit-msg", "prepare-commit-msg", "post-commit", "post-checkout", "reference-transaction", "post-index-change", "pre-push" }) {
			write_file(aDirectory / "hooks" / hook, script);
			write_file(aRoot / ".git" / "hooks" / hook, script);
#if !defined(_WIN32)
			std::filesystem::permissions(aDirectory / "hooks" / hook, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
			std::filesystem::permissions(aRoot / ".git" / "hooks" / hook, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
#endif
		}
#if !defined(_WIN32)
		std::filesystem::permissions(path, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
#endif
		const auto run    = "sh \"" + path.generic_string() + "\"";
		auto       config = std::ofstream{ aRoot / ".git" / "config", std::ios::binary | std::ios::app };
		config << "[core]\n\tfsmonitor = " << path.generic_string() << "\n\thooksPath = " << (aDirectory / "hooks").generic_string() << "\n\tpager = " << run
			   << "\n\tsshCommand = " << run << "\n\talternateRefsCommand = " << run << "\n"
			   << "[filter \"evil\"]\n\tclean = " << run << "\n\tsmudge = " << run << "\n\trequired = true\n"
			   << "[diff]\n\texternal = " << run << "\n"
			   << "[commit]\n\tgpgsign = true\n"
			   << "[gpg]\n\tprogram = " << run << "\n"
			   << "[uploadpack]\n\tpackObjectsHook = " << run << "\n";
		expect(static_cast<bool>(config), "canary configuration");
		return { .Directory = aDirectory };
	}

	auto build(course& aCycle, application::workspace_source_port& aSource, publication_git& aGit, application::cycle_candidate_request aRequest)
		-> application::cycle_candidate_outcome
	{
		auto host = application::review_cycle_outgoing_host{ aSource, *aCycle.Store, *aGit.Port };
		return host.build(aCycle.state(), aRequest, { });
	}

	auto listing(const std::vector<core::git_tree_entry>& aEntries) -> std::string
	{
		auto text = std::string{ };
		for (const auto& entry : aEntries) {
			if (!text.empty()) {
				text += '\n';
			}
			auto mode = std::string{ };
			for (auto value = entry.Mode; value != 0U; value >>= 3U) {
				mode.insert(mode.begin(), static_cast<char>('0' + (value & 7U)));
			}
			text += mode + (entry.Mode == core::git_mode_gitlink ? " commit " : " blob ") + std::string{ core::canonical_hex(entry.Object).view() } + "\t" + entry.Path;
		}
		return text;
	}

	void builds_an_isolated_candidate(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable, const bool aSha256)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto base   = aScratch / (aSha256 ? "sha256" : "sha1");
		const auto root   = base / "project";
		std::filesystem::create_directories(root);
		if (aSha256) {
			aGit(root, { "init", "-q", "--object-format=sha256" });
		} else {
			aGit(root, { "init", "-q" });
		}
		write_file(root / ".gitattributes", "data/*.dat filter=evil\nmodels/*.bin filter=lfs diff=lfs merge=lfs -text\n");
		write_file(root / "README.md", "# Parser\n");
		write_file(root / "src" / "parse.cpp", "int parse() { return 0; }\n");
		write_file(root / "src" / "notes.txt", "one\ntwo\n");
		write_file(root / "tools" / "run.sh", "#!/bin/sh\nexit 0\n");
		write_file(root / "data" / "table.dat", "1,2,3\n");
		write_file(root / "models" / "model.bin", pointer);
		aGit(root, { "add", "." });
		aGit(root, { "commit", "-q", "-m", "base" });
		const auto head    = aGit.output(root, { "rev-parse", "HEAD" }, base / "head.txt");
		const auto planted = plant_canaries(root, base / "canaries");

		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R0");
		write_file(root / "src" / "parse.cpp", "int parse() { return validate(); }\n");
		write_file(root / "src" / "notes.txt", "one\r\ntwo\r\nthree\r\n");
		write_file(root / "assets" / "logo.bin", binary(2048, 3));
		write_file(root / "models" / "model.bin", binary(4096, 9));
#if !defined(_WIN32)
		std::filesystem::permissions(root / "tools" / "run.sh", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#endif
		const auto linked = tests::workspace_contract::try_symlink("README.md", root / "docs-link", false);
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "Rn");
		const auto before = snapshot(root);

		// A filter-managed path is never built natively.
		const auto outgoing = application::cycle_outgoing_request{ .Base = "HEAD" };
		const auto limited  = build(cycle, *source, git, { .Outgoing = outgoing, .Candidate = { .Metadata = metadata() } });
		expect(limited.Outcome.Status == cycle_outgoing_status::limited && limited.Limit && limited.Limit->Limit == core::candidate_limit::filter_managed
				   && limited.Limit->Paths == std::vector<std::string>{ "models/model.bin" } && !limited.Candidate,
			   "the LFS path's bytes are not uploaded where a pointer belongs");

		// External preparation: a patch ordinary Git applies at P.
		auto host     = application::review_cycle_outgoing_host{ *source, *cycle.Store, *git.Port };
		auto external = host.prepare_external(cycle.state(), outgoing, { }, { });
		expect(external.Outcome.Status == cycle_outgoing_status::planned && external.Preparation && !external.Patch.empty(), "external preparation");
		expect(external.Preparation->ToolingPaths == std::vector<std::string>{ "models/model.bin" } && external.Preparation->Instructions.find("not native publication") != std::string::npos,
			   "it names what the reviewer's tools must still do");

		// The native candidate, with the LFS path left out and one named normalization.
		auto request               = application::cycle_candidate_request{ .Outgoing = outgoing, .Candidate = { .NormalizeLineEndings = { "src/notes.txt" }, .Metadata = metadata() } };
		request.Outgoing.Overrides = { { .Path = "models/model.bin", .Choice = core::outgoing_choice::exclude } };
		const auto built           = build(cycle, *source, git, request);
		expect(built.Outcome.Status == cycle_outgoing_status::planned && built.Candidate, "built");
		const auto& candidate = *built.Candidate;
		expect(std::string{ core::canonical_hex(candidate.Parent).view() } == head && candidate.Content.Parents.size() == 1U, "one parent: P");
		expect(candidate.Transformations.size() == 1U && candidate.Transformations[0].Path == "src/notes.txt", "the one named transformation");
		expect(built.Outcome.Preview.find("CRLF -> LF  src/notes.txt") != std::string::npos && built.Outcome.Preview.find("unsigned.") != std::string::npos, "previewed");
		expect(snapshot(root) == before, "the checkout's files, index, references and configuration are byte-for-byte unchanged");
		expect(!planted.fired(), "no configured program, filter or hook of the checkout ran");

		// Required signing is a limit, and the repository is still unchanged.
		auto signing                                   = request;
		signing.Candidate.Requirements.SigningRequired = true;
		const auto refusedUnsigned                     = build(cycle, *source, git, signing);
		expect(refusedUnsigned.Outcome.Status == cycle_outgoing_status::limited && refusedUnsigned.Limit->Limit == core::candidate_limit::signing_required, "no silent unsigned commit");
		expect(snapshot(root) == before && !planted.fired(), "still unchanged; still nothing ran");

		// An independent repository reads the candidate back exactly.
		const auto verify = base / "verify";
		aGit(base, { "clone", "-q", "--no-checkout", root.generic_string(), verify.generic_string() });
		write_file(verify / ".git" / "objects" / "info" / "alternates", (git.store(aSha256) / "objects").generic_string() + "\n");
		const auto commit = std::string{ core::canonical_hex(candidate.Commit).view() };
		const auto bytes  = core::encode_git_commit(candidate.Content);
		auto       text   = std::string(bytes->begin(), bytes->end());
		while (!text.empty() && text.back() == '\n') {
			text.pop_back();
		}
		expect(aGit.output(verify, { "cat-file", "commit", commit }, base / "commit.txt") == text, "Git stored exactly the commit computed from the confirmed fields");
		expect(aGit.output(verify, { "rev-parse", commit + "^" }, base / "parent.txt") == head, "its parent is P");
		expect(aGit.output(verify, { "ls-tree", "-r", "--full-tree", commit }, base / "tree.txt") == listing(candidate.Entries), "every entry, untouched ones P's");
		const auto notes = std::ranges::find(candidate.Entries, std::string_view{ "src/notes.txt" }, &core::git_tree_entry::Path);
		expect(aGit.output(verify, { "cat-file", "blob", core::canonical_hex(notes->Object).view() }, base / "notes.txt") == "one\ntwo\nthree", "normalized to LF");
		const auto logo      = std::ranges::find(candidate.Entries, std::string_view{ "assets/logo.bin" }, &core::git_tree_entry::Path);
		const auto logoBytes = binary(2048, 3);
		expect(logo != candidate.Entries.end()
				   && logo->Object == core::retag<core::git_entry_oid_kind>(core::git_blob_identity(std::vector<std::uint8_t>(logoBytes.begin(), logoBytes.end()), candidate.Format)),
			   "binary bytes preserved exactly");
		const auto model = std::ranges::find(candidate.Entries, std::string_view{ "models/model.bin" }, &core::git_tree_entry::Path);
		expect(aGit.output(verify, { "cat-file", "blob", core::canonical_hex(model->Object).view() }, base / "model.txt") + "\n" == pointer, "the LFS pointer stays P's");
#if !defined(_WIN32)
		const auto run = std::ranges::find(candidate.Entries, std::string_view{ "tools/run.sh" }, &core::git_tree_entry::Path);
		expect(run->Mode == core::git_mode_executable, "the executable bit");
#endif
		if (linked) {
			const auto link = std::ranges::find(candidate.Entries, std::string_view{ "docs-link" }, &core::git_tree_entry::Path);
			expect(link != candidate.Entries.end() && link->Mode == core::git_mode_symlink
					   && aGit.output(verify, { "cat-file", "blob", core::canonical_hex(link->Object).view() }, base / "link.txt") == "README.md",
				   "a link is its target bytes, never followed");
		}

		// The external patch applies at P with ordinary Git and gives the prepared tree.
		const auto patch = base / "selection.patch";
		write_file(patch, std::string(external.Patch.begin(), external.Patch.end()));
		aGit(verify, { "read-tree", head });
		aGit(verify, { "apply", "--cached", "--binary", patch.generic_string() });
		expect(aGit.output(verify, { "write-tree" }, base / "applied.txt") == core::canonical_hex(external.Preparation->Tree).view(), "the patch reproduces the prepared tree");

		// The canaries are live: ordinary Git in the checkout does run them.
		write_file(root / "data" / "table.dat", "4,5,6\n");
		aGit(root, { "add", "data/table.dat" });
		expect(planted.fired(), "positive control: the checkout's own filter runs under ordinary Git");
		std::cout << "NATIVE: " << (aSha256 ? "SHA-256" : "SHA-1") << " candidate built in the private store, read back exactly, checkout untouched, no canary\n";
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	const auto executable = tests::workspace_contract::git_executable(aArgc, aArgv);
	const auto fixtures   = tests::workspace_contract::git_from(aArgc, aArgv);
	expect(executable.has_value() && fixtures.has_value(), "publication needs the build's Git (--git)");
	tests::sqlite_contract::scratch_root scratch{ "cycle-candidate" };
	builds_an_isolated_candidate(scratch.path(), *fixtures, *executable, false);
	builds_an_isolated_candidate(scratch.path(), *fixtures, *executable, true);
	std::cout << "review-cycle-candidate-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle candidate failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
