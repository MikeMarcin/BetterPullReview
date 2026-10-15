// [[BPR-1454]] and [[BPR-1461]]. The Git metadata readers against
// repositories that real Git made: each row is a divergence RFC-BPR-0083
// observed, and fails on the readers at its base. Git builds the fixtures and
// answers as the oracle where a row needs one; the product never runs it.

#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_outgoing.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/sha1.hpp"
#include "cycle_course.hpp"
#include "publication_course.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	auto excluded(const core::cycle_manifest& aManifest, const std::string& aPath) -> bool
	{
		return std::ranges::any_of(aManifest.Exclusions, [&aPath](const core::manifest_exclusion& aExclusion) {
			return aExclusion.Path == aPath && aExclusion.Reason == core::workspace_exclusion_reason::ignored;
		});
	}

	auto contains(const std::string_view aText, const std::string_view aPart) -> bool
	{
		return aText.find(aPart) != std::string_view::npos;
	}

	/// The installed Git's version as {major, minor}.
	auto git_version(const git_runner& aGit, const std::filesystem::path& aScratch) -> std::pair<int, int>
	{
		const auto  text  = aGit.output(aScratch, { "version" }, aScratch / "version.out");
		const auto  start = std::min(text.find_first_of("0123456789"), text.size());
		const auto* first = text.data() + start;
		const auto* last  = text.data() + text.size();
		auto        major = 0;
		auto        minor = 0;
		const auto  read  = std::from_chars(first, last, major);
		if (read.ec == std::errc{ } && read.ptr != last && *read.ptr == '.') {
			static_cast<void>(std::from_chars(read.ptr + 1, last, minor));
		}
		return { major, minor };
	}

	auto make_repository(const git_runner& aGit, const std::filesystem::path& aRepo, const std::initializer_list<std::string_view> aInit = { "init", "--quiet" })
		-> void
	{
		std::filesystem::create_directories(aRepo);
		aGit(aRepo, aInit);
		write_file(aRepo / "tracked.txt", "tracked\n");
		aGit(aRepo, { "add", "-A" });
		aGit(aRepo, { "commit", "--quiet", "--no-verify", "-m", "fixture" });
	}

	auto capture(const std::filesystem::path& aRepo) -> std::pair<application::cycle_capture_outcome, std::optional<core::cycle_manifest>>
	{
		auto       source  = adapters::workspace::make_local_workspace_source();
		auto       cycle   = course{ *source, aRepo };
		const auto outcome = cycle.capture(*source);
		if (outcome.Status != cycle_capture_status::captured) {
			return { outcome, std::nullopt };
		}
		return { outcome, cycle.manifest(outcome) };
	}

	/// Capture is refused, and its words name `aPart`.
	void expect_refused(const std::filesystem::path& aRepo, const std::string_view aPart, const char* aWhat)
	{
		const auto [outcome, manifest] = capture(aRepo);
		const auto words               = application::cycle_capture_words(outcome);
		if (outcome.Status != cycle_capture_status::workspace || !contains(words, aPart)) {
			std::cerr << "capture status " << static_cast<int>(outcome.Status) << ", words: " << words << '\n';
		}
		expect(outcome.Status == cycle_capture_status::workspace && contains(words, aPart), aWhat);
	}

	auto append_config(const std::filesystem::path& aRepo, const std::string_view aText) -> void
	{
		write_file(aRepo / ".git" / "config", read_file(aRepo / ".git" / "config") + std::string{ aText });
	}

	// Fix 1: the repository format and every extension.
	void refuses_unreadable_repository_formats(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto newer = aScratch / "format-newer";
		make_repository(aGit, newer);
		aGit(newer, { "config", "core.repositoryformatversion", "2" });
		expect_refused(newer, "core.repositoryformatversion", "a format version above 1 is refused by name");

		const auto unknown = aScratch / "format-unknown";
		make_repository(aGit, unknown);
		aGit(unknown, { "config", "core.repositoryformatversion", "1" });
		aGit(unknown, { "config", "extensions.futureFormat", "true" });
		expect_refused(unknown, "extensions.futureformat", "an unknown version 1 extension is refused by name");

		const auto zero = aScratch / "format-zero";
		make_repository(aGit, zero);
		append_config(zero, "[extensions]\n\tobjectFormat = sha1\n");
		expect_refused(zero, "extensions.objectformat", "a version 1 extension in a version 0 repository is refused, as Git refuses it");

		// The allowlist leaves a repository readable.
		const auto allowed = aScratch / "format-allowed";
		make_repository(aGit, allowed);
		aGit(allowed, { "config", "core.repositoryformatversion", "1" });
		aGit(allowed, { "config", "extensions.preciousObjects", "true" });
		aGit(allowed, { "config", "extensions.noop", "x" });
		expect(capture(allowed).first.Status == cycle_capture_status::captured, "allowed extensions are read");
		std::cout << "NATIVE: repository format version, unknown and version-0 extensions refused by name\n";
	}

	// Fix 1 and Q2 (a): a reftable checkout is refused, never an empty HEAD.
	void refuses_reftable(const std::filesystem::path& aScratch, const git_runner& aGit, const std::pair<int, int> aVersion)
	{
		if (aVersion < std::pair{ 2, 45 }) {
			std::cout << "SKIPPED: reftable needs Git 2.45; found " << aVersion.first << '.' << aVersion.second << '\n';
			return;
		}
		const auto repo = aScratch / "reftable";
		make_repository(aGit, repo, { "init", "--quiet", "--ref-format=reftable" });
		expect_refused(repo, "reftable", "a reftable checkout is refused with its own reason");

		auto       source = adapters::workspace::make_local_workspace_source();
		auto       cycle  = course{ *source, repo };
		auto       host   = application::review_cycle_capture_host{ *source, *cycle.Store };
		const auto check  = host.check_material(cycle.state(), repo.string(), { });
		expect(check.Status == cycle_capture_status::workspace && contains(application::cycle_rebind_check_words(check), "reftable"), "the rebind material check refuses it too");
		std::cout << "NATIVE: a reftable checkout refused by capture and the material check\n";
	}

	// Fix 1 for outgoing resolution: a checkout captured with files and then
	// migrated to reftable is refused rather than resolving no reference.
	void outgoing_refuses_reftable(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable, const std::pair<int, int> aVersion)
	{
		if (aVersion < std::pair{ 2, 46 }) {
			std::cout << "SKIPPED: reference migration needs Git 2.46; found " << aVersion.first << '.' << aVersion.second << '\n';
			return;
		}
		const auto repo = aScratch / "migrated";
		make_repository(aGit, repo);
		auto       source   = adapters::workspace::make_local_workspace_source();
		auto       cycle    = course{ *source, repo };
		const auto captured = cycle.capture(*source);
		expect(captured.Status == cycle_capture_status::captured, "captured with files references");
		aGit(repo, { "refs", "migrate", "--ref-format=reftable" });
		auto       git     = tests::workspace_contract::publication_git{ aExecutable };
		auto       host    = application::review_cycle_outgoing_host{ *source, *cycle.Store, *git.Port };
		const auto planned = host.plan(cycle.state(), { .Base = "HEAD" }, { });
		expect(planned.Status == application::cycle_outgoing_status::workspace, "outgoing resolution refuses the migrated checkout");
		std::cout << "NATIVE: outgoing resolution refuses a checkout migrated to reftable\n";
	}

	// Fix 2: Git's config syntax.
	void reads_config_with_git_syntax(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto repo = aScratch / "config-syntax";
		make_repository(aGit, repo, { "init", "--quiet", "--object-format=sha256" });
		auto config = read_file(repo / ".git" / "config");
		// Git keeps reading SHA-256 with an inline comment on the line.
		const auto at = config.find("objectformat = sha256");
		expect(at != std::string::npos, "Git wrote the object format");
		config.insert(at + std::string_view{ "objectformat = sha256" }.size(), " ; written by hand");
		write_file(repo / ".git" / "config", config);
		const auto oracle = aGit.output(repo, { "rev-parse", "--show-object-format" }, aScratch / "format.out");
		expect(oracle == "sha256", "Git still reads SHA-256");
		const auto [outcome, manifest] = capture(repo);
		expect(outcome.Status == cycle_capture_status::captured && manifest && manifest->ObjectFormat == core::git_hash_algorithm::sha256, "an inline comment keeps SHA-256");
		std::cout << "NATIVE: config inline comments read as Git reads them\n";
	}

	// Fix 3: core.ignoreCase folds ignore and attribute matching.
	void folds_case_under_ignore_case(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto repo = aScratch / "ignore-case";
		make_repository(aGit, repo);
		// Quoted and continued, as Git allows.
		append_config(repo, "[core]\n\tignorecase = \"tr\"\\\n\"ue\"\n");
		write_file(repo / ".gitignore", "Secret.env\n");
		write_file(repo / ".gitattributes", "*.BIN filter=lfs\n");
		aGit(repo, { "add", ".gitignore", ".gitattributes" });
		aGit(repo, { "commit", "--quiet", "--no-verify", "-m", "rules" });
		write_file(repo / "secret.env", "TOKEN=1\n");
		write_file(repo / "image.bin", "bytes\n");
		const auto oracle = aGit.output(repo, { "check-ignore", "--no-index", "secret.env" }, aScratch / "ignore.out");
		expect(oracle == "secret.env", "Git ignores secret.env under core.ignoreCase");
		const auto [outcome, manifest] = capture(repo);
		expect(outcome.Status == cycle_capture_status::captured && manifest, "captured");
		expect(excluded(*manifest, "secret.env") && core::find_entry(*manifest, "secret.env") == nullptr, "the file Git ignores stays out of the capture");
		const auto* image = core::find_entry(*manifest, "image.bin");
		expect(image != nullptr && image->FilterManaged, "the filter attribute folds case too");

		// Under extensions.worktreeConfig, config.worktree's setting wins.
		const auto worktree = aScratch / "worktree-config";
		make_repository(aGit, worktree);
		aGit(worktree, { "config", "core.repositoryformatversion", "1" });
		aGit(worktree, { "config", "extensions.worktreeConfig", "true" });
		aGit(worktree, { "config", "core.ignorecase", "false" });
		aGit(worktree, { "config", "--worktree", "core.ignorecase", "true" });
		write_file(worktree / ".gitignore", "Secret.env\n");
		write_file(worktree / "secret.env", "TOKEN=1\n");
		const auto [worktreeOutcome, worktreeManifest] = capture(worktree);
		expect(worktreeManifest && excluded(*worktreeManifest, "secret.env"), "config.worktree's ignoreCase applies");
		std::cout << "NATIVE: core.ignoreCase folds ignore and filter matching, config.worktree included\n";
	}

	// Fix 4: the POSIX bracket classes.
	void matches_posix_classes(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto repo = aScratch / "posix-classes";
		make_repository(aGit, repo);
		aGit(repo, { "config", "core.ignorecase", "false" });
		write_file(repo / ".gitignore", "a[[:space:]]b\nx[![:space:]]y\n[[:digit:]][[:upper:]]\nconfig/local**/*.pem\n");
		write_file(repo / "config" / "local" / "a" / "key.pem", "ignored by Git\n");
		write_file(repo / "a b", "ignored by Git\n");
		write_file(repo / "x y", "kept by Git\n");
		write_file(repo / "xzy", "ignored by Git\n");
		write_file(repo / "7Q", "ignored by Git\n");
		const auto [outcome, manifest] = capture(repo);
		expect(outcome.Status == cycle_capture_status::captured && manifest, "captured");
		expect(excluded(*manifest, "a b") && excluded(*manifest, "xzy") && excluded(*manifest, "7Q"), "classes match what Git ignores");
		const auto output = aScratch / "prefix.out";
		const auto status = aGit.status(repo, { "check-ignore", "--no-index", "config/local/a/key.pem" }, std::nullopt, output);
		const auto oracle = read_file(output);
		// Git 2.50.1 stripped the literal prefix before wildmatch, making this
		// `**` cross directories. Git 2.55 retains a prefix byte and keeps the
		// path. Capture preserves the previously qualified, conservative exclusion.
		const auto legacy = status == 0 && (oracle == "config/local/a/key.pem\n" || oracle == "config/local/a/key.pem\r\n");
		expect(legacy || (status == 1 && oracle.empty()), "the Git oracle independently reports one of the two known literal-prefix double-star behaviors");
		std::cout << "NATIVE: Git literal-prefix double star " << (legacy ? "ignores" : "keeps") << "; capture preserves its conservative exclusion\n";
		expect(core::find_entry(*manifest, "config/local/a/key.pem") == nullptr, "capture preserves its conservative literal-prefix exclusion");
		expect(core::find_entry(*manifest, "x y") != nullptr, "a negated class keeps what Git keeps");
		std::cout << "NATIVE: POSIX bracket classes match as Git's\n";
	}

	auto big_endian(std::vector<std::uint8_t>& aBytes, const std::uint32_t aValue) -> void
	{
		for (auto shift = 24; shift >= 0; shift -= 8) {
			aBytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
		}
	}

	auto read_bytes(const std::filesystem::path& aPath) -> std::vector<std::uint8_t>
	{
		const auto text = read_file(aPath);
		return { text.begin(), text.end() };
	}

	auto write_bytes(const std::filesystem::path& aPath, const std::vector<std::uint8_t>& aBytes) -> void
	{
		write_file(aPath, std::string{ aBytes.begin(), aBytes.end() });
	}

	// Fixes 5 and 6: the index trailer and required extensions.
	void checks_the_index(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto corrupt = aScratch / "index-checksum";
		make_repository(aGit, corrupt);
		auto bytes = read_bytes(corrupt / ".git" / "index");
		// The first entry's ctime: structure intact, checksum broken.
		bytes.at(12) ^= 0x01U;
		write_bytes(corrupt / ".git" / "index", bytes);
		expect_refused(corrupt, "checksum", "an index failing its checksum is refused");

		const auto skip = aScratch / "index-skip-hash";
		std::filesystem::create_directories(skip);
		aGit(skip, { "init", "--quiet" });
		aGit(skip, { "config", "index.skipHash", "true" });
		write_file(skip / "tracked.txt", "tracked\n");
		aGit(skip, { "add", "-A" });
		const auto zero = read_bytes(skip / ".git" / "index");
		expect(std::ranges::all_of(zero.end() - 20, zero.end(), [](const std::uint8_t aByte) { return aByte == 0; }), "Git wrote an all-zero trailer");
		expect(capture(skip).first.Status == cycle_capture_status::captured, "an index.skipHash index is read");
		std::cout << "NATIVE: index checksum verified, skipHash accepted\n";
	}

	void refuses_required_index_extensions(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto extension = aScratch / "index-extension";
		make_repository(aGit, extension);
		auto withExtension = read_bytes(extension / ".git" / "index");
		withExtension.resize(withExtension.size() - 20U);
		withExtension.insert(withExtension.end(), { 'a', 'b', 'c', 'd' });
		big_endian(withExtension, 0);
		const auto trailer = core::sha1(withExtension).value();
		withExtension.insert(withExtension.end(), trailer.begin(), trailer.end());
		write_bytes(extension / ".git" / "index", withExtension);
		expect_refused(extension, "required extension", "a required index extension is refused");
		std::cout << "NATIVE: a required index extension refused\n";
	}

	// Fix 7: HEAD never falls back to a stale packed-refs entry, and follows
	// one symbolic level.
	void resolves_head_truthfully(const std::filesystem::path& aScratch, const git_runner& aGit)
	{
		const auto alias = aScratch / "head-alias";
		make_repository(aGit, alias);
		const auto main = aGit.output(alias, { "rev-parse", "HEAD" }, aScratch / "main.out");
		aGit(alias, { "symbolic-ref", "refs/heads/alias", "refs/heads/main" });
		aGit(alias, { "symbolic-ref", "HEAD", "refs/heads/alias" });
		const auto [aliasOutcome, aliasManifest] = capture(alias);
		expect(aliasManifest && aliasManifest->HeadCommit && core::canonical_hex(*aliasManifest->HeadCommit).view() == main, "a symbolic branch resolves one level, as Git does");
		std::cout << "NATIVE: HEAD follows one symbolic level\n";
	}

	void never_falls_back_to_stale_packed_refs([[maybe_unused]] const std::filesystem::path& aScratch, [[maybe_unused]] const git_runner& aGit)
	{

#if !defined(_WIN32)
		const auto stale = aScratch / "head-stale";
		make_repository(aGit, stale);
		aGit(stale, { "pack-refs", "--all" });
		write_file(stale / "tracked.txt", "second\n");
		aGit(stale, { "commit", "--quiet", "--no-verify", "-am", "second" });
		// The loose file now holds the new commit and packed-refs the old one.
		const auto loose = stale / ".git" / "refs" / "heads" / "main";
		expect(std::filesystem::exists(loose), "a loose reference beside packed-refs");
		std::filesystem::permissions(loose, std::filesystem::perms::none);
		const auto [staleOutcome, staleManifest] = capture(stale);
		std::filesystem::permissions(loose, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
		expect(staleOutcome.Status == cycle_capture_status::workspace, "an unreadable loose reference is reported, not replaced by packed-refs");
		std::cout << "NATIVE: an unreadable loose reference never falls back to stale packed-refs\n";
#else
		std::cout << "SKIPPED: no permission-denied reference on Windows\n";
#endif
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "git-readers" };
	const auto                           git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "SKIPPED: no Git in this build; Git reader rows not run\n";
		return EXIT_SUCCESS;
	}
	const auto version    = git_version(*git, scratch.path());
	auto       executable = std::filesystem::path{ };
	// `--row NAME` runs one row, so a negative control can show each failing.
	auto row = std::string_view{ };
	for (int index = 1; index + 1 < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--git") {
			executable = aArgv[index + 1];
		}
		if (std::string_view{ aArgv[index] } == "--row") {
			row = aArgv[index + 1];
		}
	}
	const auto run = [row](const std::string_view aName) { return row.empty() || row == aName; };
	if (run("format")) {
		refuses_unreadable_repository_formats(scratch.path(), *git);
	}
	if (run("reftable")) {
		refuses_reftable(scratch.path(), *git, version);
	}
	if (run("outgoing-reftable")) {
		outgoing_refuses_reftable(scratch.path(), *git, executable, version);
	}
	if (run("config")) {
		reads_config_with_git_syntax(scratch.path(), *git);
	}
	if (run("ignore-case")) {
		folds_case_under_ignore_case(scratch.path(), *git);
	}
	if (run("posix-classes")) {
		matches_posix_classes(scratch.path(), *git);
	}
	if (run("index-checksum")) {
		checks_the_index(scratch.path(), *git);
	}
	if (run("index-extension")) {
		refuses_required_index_extensions(scratch.path(), *git);
	}
	if (run("head-symbolic")) {
		resolves_head_truthfully(scratch.path(), *git);
	}
	if (run("head-stale")) {
		never_falls_back_to_stale_packed_refs(scratch.path(), *git);
	}
	std::cout << "review-cycle-git-reader-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
