#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/sha256.hpp"
#include "cycle_course.hpp"
#include "windows_junction.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if !defined(_WIN32)
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using tests::expect;
	using tests::workspace_contract::bytes_of;
	using tests::workspace_contract::course;
	using tests::workspace_contract::digest_of;
	using tests::workspace_contract::entry;
	using tests::workspace_contract::operation;
	using tests::workspace_contract::profile;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	/// Every file beneath a root, links as their target text, by relative path.
	auto hash_tree(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto tree = std::map<std::string, std::string>{ };
		for (auto iterator = std::filesystem::recursive_directory_iterator{ aRoot }; iterator != std::filesystem::recursive_directory_iterator{ }; ++iterator) {
			const auto relative = std::filesystem::relative(iterator->path(), aRoot).generic_string();
			if (iterator->is_symlink()) {
				tree[relative] = "link:" + std::filesystem::read_symlink(iterator->path()).string();
			} else if (iterator->is_regular_file()) {
				const auto text = read_file(iterator->path());
				const auto hash = core::sha256(bytes_of(text));
				tree[relative]  = std::string{ hash.begin(), hash.end() };
			} else if (iterator->is_directory()) {
				tree[relative] = "dir";
			}
		}
		return tree;
	}

	/// A workspace port whose sessions call a hook after every file read, so
	/// a course can change the source between the passes of one capture.
	class interfering_source final : public application::workspace_source_port
	{
	public:
		using hook = std::function<void(const std::string&)>;

		explicit interfering_source(hook aHook)
			: Inner{ adapters::workspace::make_local_workspace_source() }
			, Hook{ std::move(aHook) }
		{
		}

		auto open(const std::string_view aLocation, std::stop_token aStop) -> std::expected<std::unique_ptr<application::workspace_root_session>, application::workspace_failure> override
		{
			auto session = Inner->open(aLocation, std::move(aStop));
			if (!session) {
				return std::unexpected(session.error());
			}
			return std::make_unique<interfering_session>(std::move(*session), Hook);
		}

	private:
		class interfering_session final : public application::workspace_root_session
		{
		public:
			interfering_session(std::unique_ptr<application::workspace_root_session> aInner, hook& aHook)
				: Inner{ std::move(aInner) }
				, Hook{ aHook }
			{
			}
			[[nodiscard]] auto observation() const -> const core::workspace_root_observation& override
			{
				return Inner->observation();
			}
			auto verify(std::stop_token aStop) -> std::expected<core::workspace_verification, application::workspace_failure> override
			{
				return Inner->verify(std::move(aStop));
			}
			auto list(const std::string_view aDirectory, std::stop_token aStop) -> std::expected<std::vector<application::workspace_entry_observation>, application::workspace_failure> override
			{
				return Inner->list(aDirectory, std::move(aStop));
			}
			auto stat(const std::string_view aPath, std::stop_token aStop) -> std::expected<application::workspace_entry_observation, application::workspace_failure> override
			{
				return Inner->stat(aPath, std::move(aStop));
			}
			auto read(const std::string_view aPath, const std::uint64_t aMaximumBytes, const application::workspace_chunk_sink& aSink, std::stop_token aStop)
				-> std::expected<application::workspace_read_observation, application::workspace_failure> override
			{
				auto result = Inner->read(aPath, aMaximumBytes, aSink, std::move(aStop));
				Hook(std::string{ aPath });
				return result;
			}
			auto read_link(const std::string_view aPath, std::stop_token aStop) -> std::expected<std::string, application::workspace_failure> override
			{
				return Inner->read_link(aPath, std::move(aStop));
			}
			auto read_git(const application::workspace_git_area aArea, const std::string_view aPath, const std::uint64_t aMaximumBytes, std::stop_token aStop)
				-> std::expected<std::vector<std::uint8_t>, application::workspace_failure> override
			{
				return Inner->read_git(aArea, aPath, aMaximumBytes, std::move(aStop));
			}

		private:
			std::unique_ptr<application::workspace_root_session> Inner;
			hook&                                                Hook;
		};

		std::unique_ptr<application::workspace_source_port> Inner;
		hook                                                Hook;
	};

	auto excluded(const core::cycle_manifest& aManifest, const std::string& aPath) -> std::optional<core::workspace_exclusion_reason>
	{
		const auto found = std::ranges::find(aManifest.Exclusions, aPath, &core::manifest_exclusion::Path);
		return found == aManifest.Exclusions.end() ? std::nullopt : std::optional{ found->Reason };
	}

	void write_script(const std::filesystem::path& aPath, const std::filesystem::path& aCanary)
	{
		write_file(aPath, "#!/bin/sh\necho \"$0 $*\" >> '" + aCanary.string() + "'\ncat\n");
#if !defined(_WIN32)
		std::filesystem::permissions(aPath, std::filesystem::perms::owner_all, std::filesystem::perm_options::add);
#endif
	}

	void leaves_the_checkout_untouched(const std::filesystem::path& aScratch, const tests::workspace_contract::git_runner& aGit)
	{
		const auto repo   = aScratch / "hostile";
		const auto canary = aScratch / "canary.log";
		std::filesystem::create_directories(repo);
		aGit(repo, { "init", "--quiet" });
		write_file(repo / "a.txt", "committed a\n");
		write_file(repo / "src" / "b.txt", "committed b\n");
		write_file(repo / ".gitignore", "*.secret\n");
		write_file(repo / ".gitattributes", "*.txt filter=evil diff=evil\n");
		aGit(repo, { "add", "-A" });
		aGit(repo, { "commit", "--quiet", "--no-verify", "-m", "fixture" });

		// Every executable surface Git might use now points at a canary.
		for (const auto* hook : { "pre-commit", "post-checkout", "post-index-change", "reference-transaction", "fsmonitor-watchman", "pre-auto-gc" }) {
			write_script(repo / ".git" / "hooks" / hook, canary);
		}
		write_script(aScratch / "evil.sh", canary);
		const auto evil = (aScratch / "evil.sh").string();
		aGit(repo, { "config", "core.fsmonitor", evil });
		aGit(repo, { "config", "filter.evil.clean", evil });
		aGit(repo, { "config", "filter.evil.smudge", evil });
		aGit(repo, { "config", "filter.evil.process", evil });
		aGit(repo, { "config", "diff.evil.textconv", evil });
		aGit(repo, { "config", "core.repositoryformatversion", "1" });
		aGit(repo, { "config", "extensions.partialClone", "origin" });
		aGit(repo, { "config", "remote.origin.url", "file:///nonexistent-promisor" });
		aGit(repo, { "config", "remote.origin.promisor", "true" });

		// Dirty, staged-only, untracked and ignored content.
		write_file(repo / "a.txt", "edited a, not staged\n");
		write_file(repo / "src" / "b.txt", "staged b\n");
		aGit(repo, { "add", "src/b.txt" });
		write_file(repo / "src" / "b.txt", "working b, differs from the index\n");
		write_file(repo / "c.txt", "untracked c\n");
		write_file(repo / "token.secret", "ignored secret\n");
		const auto staged = aGit.output(repo, { "rev-parse", ":src/b.txt" }, aScratch / "rev-parse.out");
		std::filesystem::remove(aScratch / "rev-parse.out");
		std::filesystem::remove(canary);

		const auto before  = hash_tree(repo);
		auto       source  = adapters::workspace::make_local_workspace_source();
		auto       cycle   = course{ *source, repo };
		const auto outcome = cycle.capture(*source);
		expect(outcome.Status == cycle_capture_status::captured, "capture succeeds");
		expect(hash_tree(repo) == before, "source, index, refs and config bytes unchanged");
		expect(!std::filesystem::exists(canary), "no hook, filter, textconv or fsmonitor ran");

		const auto manifest = cycle.manifest(outcome);
		expect(manifest.RootKind == core::workspace_root_kind::git_checkout && manifest.ObjectFormat == core::git_hash_algorithm::sha1, "Git root, SHA-1");
		expect(manifest.HeadReference == "refs/heads/main" && manifest.HeadCommit.has_value(), "HEAD recorded as provenance");
		expect(cycle.content(entry(manifest, "a.txt")) == "edited a, not staged\n", "working bytes, not HEAD");
		const auto& b = entry(manifest, "src/b.txt");
		expect(cycle.content(b) == "working b, differs from the index\n", "working bytes, not the index");
		expect(b.Staged && core::canonical_hex(*b.Staged).view() == staged, "staged object kept separately");
		expect(b.Origin == core::manifest_origin::tracked && b.FilterManaged, "tracked, filter-managed and not filtered");
		expect(entry(manifest, "c.txt").Origin == core::manifest_origin::untracked, "untracked candidate included");
		expect(excluded(manifest, "token.secret") == core::workspace_exclusion_reason::ignored, "ignored file disclosed, not read");
		expect(manifest.Quality == core::cycle_capture_quality::observed_stable && manifest.Coverage == core::cycle_capture_coverage::complete, "honest quality");
		std::cout << "NATIVE: hostile hooks, filters, textconv, fsmonitor and promisor configured; none ran; tree bytes unchanged\n";
	}

	void retries_detected_instability(const std::filesystem::path& aScratch)
	{
		const auto root = aScratch / "busy";
		write_file(root / "hot.txt", "v1");
		write_file(root / "cold.txt", "stable");

		// One change between the passes: the second attempt is stable.
		auto changes = 0;
		auto once    = interfering_source{ [&](const std::string& aPath) {
			if (aPath == "hot.txt" && changes == 0) {
				++changes;
				write_file(root / "hot.txt", "v22");
			}
		} };
		auto       cycle     = course{ once, root };
		const auto recovered = cycle.capture(once);
		expect(recovered.Status == cycle_capture_status::captured && recovered.Diagnostics.Attempts == 2, "retried once, then stable");
		expect(cycle.content(entry(cycle.manifest(recovered), "hot.txt")) == "v22", "the stable bytes are recorded");
		expect(cycle.revisions() == 1, "one revision");

		// A writer that never stops: finite retries, then unstable, nothing recorded.
		auto length     = std::size_t{ 3 };
		auto relentless = interfering_source{ [&](const std::string& aPath) {
			if (aPath == "hot.txt") {
				write_file(root / "hot.txt", std::string(++length, 'w'));
			}
		} };
		const auto unstable = cycle.capture(relentless);
		expect(unstable.Status == cycle_capture_status::unstable, "unstable after retries");
		expect(unstable.Diagnostics.Attempts == core::maximum_capture_retries + 1, "exactly three retries");
		expect(unstable.Diagnostics.Changed > 0 && std::ranges::find(unstable.Diagnostics.ChangedExamples, "hot.txt") != unstable.Diagnostics.ChangedExamples.end(), "the changing path is named");
		expect(!unstable.Receipt && !unstable.Manifest && cycle.revisions() == 1, "no partial or unstable revision");
		expect(cycle.state().Latest->Capture.Quality == core::cycle_capture_quality::observed_stable, "quality never claims atomicity");

		// A file appearing during the capture changes the enumeration.
		auto appeared = false;
		auto grows    = interfering_source{ [&](const std::string& aPath) {
			if (aPath == "cold.txt" && !appeared) {
				appeared = true;
				write_file(root / "late.txt", "late");
			}
		} };
		const auto enumerated = cycle.capture(grows);
		expect(enumerated.Status == cycle_capture_status::captured && enumerated.Diagnostics.Attempts == 2, "enumeration change retried");
		expect(core::find_entry(cycle.manifest(enumerated), "late.txt") != nullptr, "the new file is in the stable capture");

		// Cancellation: no revision, only reclaimable objects.
		auto stop   = std::stop_source{ };
		auto cancel = interfering_source{ [&](const std::string&) { stop.request_stop(); } };
		write_file(root / "hot.txt", "cancel me: new bytes");
		const auto cancelled = cycle.capture(cancel, { }, stop.get_token());
		expect(cancelled.Status == cycle_capture_status::cancelled && cycle.revisions() == 2, "cancelled capture records nothing");
		const auto inventory = cycle.Store->inventory({ });
		expect(inventory && inventory->UnreferencedObjects > 0, "installed bytes are unreferenced");
		const auto reclaimed = cycle.Store->reclaim({ });
		expect(reclaimed && reclaimed->UnreferencedObjects == 0 && reclaimed->PinnedObjects == inventory->PinnedObjects, "reclaim removes only unreferenced bytes");
		std::cout << "NATIVE: instability retried finitely; cancellation left no revision\n";
	}

	void records_scope_versions_deletions_and_exclusions(const std::filesystem::path& aScratch, const tests::workspace_contract::git_runner& aGit)
	{
		const auto repo = aScratch / "scoped";
		std::filesystem::create_directories(repo);
		aGit(repo, { "init", "--quiet" });
		write_file(repo / "src" / "main.cpp", "int main() {}\n");
		write_file(repo / "src" / "old.cpp", "old\n");
		write_file(repo / "docs" / "guide.md", "# Guide\n");
		write_file(repo / "docs" / "skip.md", "sparse\n");
		write_file(repo / ".gitignore", "build/\n");
		aGit(repo, { "add", "-A" });
		aGit(repo, { "commit", "--quiet", "--no-verify", "-m", "fixture" });
		write_file(repo / "src" / "build" / "out.o", "object");
		write_file(repo / "src" / "node_modules" / "dep.js", "dep");
		write_file(repo / "src" / "private" / "notes.txt", "private");
		write_file(repo / "src" / "scratch.txt", "untracked");
		// Sparse checkout's flag: tracked, deliberately absent from the worktree.
		aGit(repo, { "update-index", "--skip-worktree", "docs/skip.md" });
		std::filesystem::remove(repo / "docs" / "skip.md");

		auto       source = adapters::workspace::make_local_workspace_source();
		auto       cycle  = course{ *source, repo, { "src" } };
		const auto r0     = cycle.capture(*source, { .Exclusions = { "src/private" } });
		expect(r0.Status == cycle_capture_status::captured, "R0");
		const auto m0 = cycle.manifest(r0);
		expect(m0.ScopeVersion == 1 && core::find_entry(m0, "docs/guide.md") == nullptr, "outside the scope nothing is read");
		expect(excluded(m0, "src/private") == core::workspace_exclusion_reason::requested, "requested exclusion recorded");
		expect(excluded(m0, "src/node_modules") == core::workspace_exclusion_reason::default_set, "default exclusion disclosed");

		cycle.rescope({ "docs", "src" });
		const auto r1 = cycle.capture(*source, { .Exclusions = { "src/private" } });
		expect(r1.Status == cycle_capture_status::captured, "R1");
		const auto m1 = cycle.manifest(r1);
		expect(m1.ScopeVersion == 2 && core::find_entry(m1, "docs/guide.md") != nullptr, "new scope version reads docs");
		const auto& sparse = entry(m1, "docs/skip.md");
		expect(sparse.Availability == core::manifest_availability::sparse && !sparse.Content && sparse.Staged, "sparse entry visible, unread, never fetched");
		expect(std::ranges::find(m1.Deletions, "docs/skip.md") == m1.Deletions.end(), "sparse is not deletion");

		// The docs file was outside R0's scope: unknown, not added.
		const auto scope0 = core::cycle_scope_specification{ .Version = m0.ScopeVersion, .Paths = m0.Scope };
		const auto scope1 = core::cycle_scope_specification{ .Version = m1.ScopeVersion, .Paths = m1.Scope };
		const auto before = core::classify_path(scope0, m0.Coverage, "docs/guide.md", { .Listed = false });
		const auto after  = core::classify_path(scope1, m1.Coverage, "docs/guide.md", { .Listed = true });
		expect(before == core::cycle_path_state::outside_scope && core::compare_path(before, std::nullopt, after, entry(m1, "docs/guide.md").Content) == core::cycle_path_change::unknown, "scope extension is not an addition");

		// Tracked and untracked removals.
		std::filesystem::remove(repo / "src" / "old.cpp");
		std::filesystem::remove(repo / "src" / "scratch.txt");
		const auto r2 = cycle.capture(*source, { .Exclusions = { "src/private" } });
		const auto m2 = cycle.manifest(r2);
		expect(m2.Deletions == std::vector<std::string>{ "src/old.cpp" } && r2.Capture->ExplicitDeletions == 1, "tracked removal is an explicit deletion");
		const auto untracked = core::classify_path(scope1, m2.Coverage, "src/scratch.txt", { });
		expect(untracked == core::cycle_path_state::absent && core::compare_path(core::cycle_path_state::present, entry(m1, "src/scratch.txt").Content, untracked, std::nullopt) == core::cycle_path_change::deleted, "untracked removal known absent in a complete capture");
		expect(core::compare_path(core::cycle_path_state::present, entry(m1, "src/scratch.txt").Content, core::classify_path(scope1, core::cycle_capture_coverage::partial, "src/scratch.txt", { }), std::nullopt) == core::cycle_path_change::unknown, "a partial capture never deletes");
		expect(excluded(m2, "src/build") == core::workspace_exclusion_reason::ignored && core::find_entry(m2, "src/build/out.o") == nullptr, "ignored directory disclosed, not read");

		// A scope path that names nothing is missing, not deleted.
		cycle.rescope({ "docs", "nope", "src" });
		const auto r3 = cycle.capture(*source, { .Exclusions = { "src/private" } });
		const auto m3 = cycle.manifest(r3);
		expect(entry(m3, "nope").Availability == core::manifest_availability::missing_scope && m3.Deletions.size() == 1, "missing scope material");
		std::cout << "NATIVE: scope versions, exclusions, sparse, deletions and missing scope distinguished\n";
	}

	void preserves_bytes_modes_and_special_files(const std::filesystem::path& aScratch, const std::optional<tests::workspace_contract::git_runner>& aGit)
	{
		const auto root   = aScratch / "corpus";
		const auto corpus = std::map<std::string, std::string>{
			{ "lf.txt", "one\ntwo\n" },
			{ "crlf.txt", "one\r\ntwo\r\n" },
			{ "mixed.txt", "one\ntwo\r\nthree" },
			{ "bom.txt", "\xEF\xBB\xBF"
						 "bom\n" },
			{ "nofinal.txt", "no final newline" },
			{ "utf16.txt", std::string{ "\xFF\xFEh\0i\0", 6 } },
			{ "invalid.txt", "bad \xC0\xAF bytes\n" },
			{ "binary.bin", std::string{ "\x89PNG\r\n\x1A\n\0\0", 10 } },
			{ "multibyte.txt", "caf\xC3\xA9 \xF0\x9F\x98\x80\n" },
			{ "exec.sh", "#!/bin/sh\n" },
			{ "plain.sh", "#!/bin/sh\n" },
			{ "large.dat", std::string(4096, 'L') },
		};
		for (const auto& [name, text] : corpus) {
			write_file(root / name, text);
		}
#if !defined(_WIN32)
		std::filesystem::permissions(root / "exec.sh", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
		std::filesystem::create_symlink("lf.txt", root / "link");
		expect(::mkfifo((root / "pipe").c_str(), 0600) == 0, "make a FIFO");
#endif
		auto       source  = adapters::workspace::make_local_workspace_source();
		auto       cycle   = course{ *source, root };
		const auto outcome = cycle.capture(*source, { .MaximumFileBytes = 1024 });
		expect(outcome.Status == cycle_capture_status::captured, "corpus captured");
		const auto manifest = cycle.manifest(outcome);
		for (const auto& [name, text] : corpus) {
			const auto& item = entry(manifest, name);
			if (name == "large.dat") {
				expect(item.Availability == core::manifest_availability::too_large && !item.Content && item.Size == text.size(), "large file visible, not read");
				continue;
			}
			expect(cycle.content(item) == text, "retained bytes are the original bytes");
			expect(item.Content == digest_of(text) && item.Size == text.size(), "raw hash of the original bytes");
			expect(item.Profile == core::classify_material(bytes_of(text)), "byte profile");
		}
		expect(entry(manifest, "crlf.txt").Profile.LineEndings == core::material_line_endings::crlf, "CRLF kept");
		expect(entry(manifest, "bom.txt").Profile.Kind == core::material_text_kind::utf8_bom, "BOM kept");
		expect(!entry(manifest, "nofinal.txt").Profile.FinalNewline, "missing final newline kept");
		expect(entry(manifest, "utf16.txt").Profile.Kind == core::material_text_kind::unsupported_encoding, "UTF-16 is file-level only");
		expect(entry(manifest, "invalid.txt").Profile.Kind == core::material_text_kind::unsupported_encoding, "invalid UTF-8 is file-level only");
		expect(entry(manifest, "binary.bin").Profile.Kind == core::material_text_kind::binary, "binary");
#if !defined(_WIN32)
		expect(entry(manifest, "exec.sh").Mode == core::git_mode_executable && entry(manifest, "plain.sh").Mode == core::git_mode_regular, "execute bit kept");
		const auto& link = entry(manifest, "link");
		expect(link.Kind == core::manifest_entry_kind::symlink && link.Mode == core::git_mode_symlink && cycle.content(link) == "lf.txt", "link target kept as data");
		expect(entry(manifest, "pipe").Availability == core::manifest_availability::unsupported_kind, "FIFO visible, unsupported");

		// A mode change alone is a material change.
		std::filesystem::permissions(root / "exec.sh", std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
		const auto changed = cycle.capture(*source, { .MaximumFileBytes = 1024 });
		expect(changed.Status == cycle_capture_status::captured && changed.Capture->Material != outcome.Capture->Material, "mode change is new material");
		expect(entry(cycle.manifest(changed), "exec.sh").Mode == core::git_mode_regular, "new mode recorded");
#else
		expect(entry(manifest, "exec.sh").Mode == core::git_mode_regular && !entry(manifest, "exec.sh").ModeFromIndex, "no execute bit invented on Windows");
#endif
		if (!aGit) {
			return;
		}
		const auto& git  = *aGit;
		const auto  repo = aScratch / "special";
		std::filesystem::create_directories(repo / "vendor" / "lib");
		git(repo, { "init", "--quiet" });
		git(repo / "vendor" / "lib", { "init", "--quiet" });
		write_file(repo / "vendor" / "lib" / "inner.txt", "inner");
		git(repo / "vendor" / "lib", { "add", "-A" });
		git(repo / "vendor" / "lib", { "commit", "--quiet", "--no-verify", "-m", "inner" });
		const auto inner   = git.output(repo / "vendor" / "lib", { "rev-parse", "HEAD" }, aScratch / "inner.out");
		const auto pointer = std::string{ "version https://git-lfs.github.com/spec/v1\noid sha256:4d7a214614ab2935c943f9e0ff69d22eadbb8f32b1258daaa5e2ca24d17e2393\nsize 12345\n" };
		write_file(repo / ".gitattributes", "*.bin filter=lfs diff=lfs merge=lfs -text\n");
		write_file(repo / "pointer.bin", pointer);
		write_file(repo / "payload.bin", std::string{ "\x01\x02 materialized payload", 23 });
		git(repo, { "add", ".gitattributes", "pointer.bin", "payload.bin" });
		git(repo, { "update-index", "--add", "--cacheinfo", "160000," + inner + ",vendor/lib" });
		git(repo, { "commit", "--quiet", "--no-verify", "-m", "special" });
		git(repo, { "update-index", "--index-version", "4" });

		auto       special  = course{ *source, repo };
		const auto captured = special.capture(*source);
		expect(captured.Status == cycle_capture_status::captured, "special capture");
		const auto  m       = special.manifest(captured);
		const auto& gitlink = entry(m, "vendor/lib");
		expect(gitlink.Kind == core::manifest_entry_kind::gitlink && gitlink.Availability == core::manifest_availability::not_materialized, "gitlink is a boundary");
		expect(gitlink.Staged && core::canonical_hex(*gitlink.Staged).view() == inner && core::find_entry(m, "vendor/lib/inner.txt") == nullptr, "gitlink not recursed");
		expect(entry(m, "pointer.bin").Profile.LfsPointer && entry(m, "pointer.bin").FilterManaged, "LFS pointer recognized, filter recorded");
		expect(!entry(m, "payload.bin").Profile.LfsPointer && entry(m, "payload.bin").FilterManaged, "materialized LFS content distinguished");
		expect(special.content(entry(m, "payload.bin")) == std::string{ "\x01\x02 materialized payload", 23 }, "materialized bytes kept, never cleaned");
		std::cout << "NATIVE: byte corpus, modes, links, FIFO, gitlink and LFS preserved without transformation (index version 4)\n";
	}

	void checks_material_before_rebind(const std::filesystem::path& aScratch)
	{
		write_file(aScratch / "one" / "project" / "same.txt", "same");
		write_file(aScratch / "one" / "project" / "differs.txt", "one");
		write_file(aScratch / "two" / "project" / "same.txt", "same");
		write_file(aScratch / "two" / "project" / "differs.txt", "two");
		write_file(aScratch / "two" / "project" / "extra.txt", "extra");
		auto source = adapters::workspace::make_local_workspace_source();
		auto cycle  = course{ *source, aScratch / "one" / "project" };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "capture the first checkout");
		auto       host  = application::review_cycle_capture_host{ *source, *cycle.Store };
		const auto check = host.check_material(cycle.state(), (aScratch / "two" / "project").string(), { });
		expect(check.Status == cycle_capture_status::captured && check.Check, "material check");
		expect(check.Check->Compared == 2 && check.Check->Matching == 1 && check.Check->Differing == 1 && check.Check->Added == 1 && check.Check->Missing == 0, "counts");
		const auto inventory = cycle.Store->inventory({ });
		expect(inventory && inventory->UnreferencedObjects == 0, "the check installs nothing");
		// [[BPR-1279]]. The root the check read is the one a rebind confirmed against it binds.
		const auto observed = host.observe((aScratch / "two" / "project").string(), { });
		expect(observed && check.Root && check.Root->Root == observed->Root && check.Root->Kind == observed->Kind && check.Root->Location == observed->Location, "the check names the root it read");
	}

	/// [[BPR-1279]]. The runtime's rebind check: the selected cycle as it
	/// stands, the moved root's bytes against its latest capture, and any other
	/// cycle bound to the candidate. It installs and changes nothing.
	void checks_rebinds_through_the_runtime(const std::filesystem::path& aScratch)
	{
		write_file(aScratch / "rebind" / "project" / "a.txt", "alpha");
		write_file(aScratch / "rebind" / "other" / "b.txt", "beta");
		auto source = adapters::workspace::make_local_workspace_source();
		auto bound  = course{ *source, aScratch / "rebind" / "project" };
		expect(bound.capture(*source).Status == cycle_capture_status::captured, "capture the project");
		// A second cycle of the same profile, bound to the other folder.
		const auto created = bound.Store->create(operation(bound.NextOperation++), { .Authority = bound.Catalog.Authority, .Title = "Other", .Scope = { } }, { });
		expect(created.has_value(), "create the other cycle");
		const auto otherSubject = core::local_cycle_subject{ .Authority = bound.Catalog.Authority, .Cycle = created->Cycle, .Generation = 1 };
		auto       host         = application::review_cycle_capture_host{ *source, *bound.Store };
		const auto otherRoot    = host.observe((aScratch / "rebind" / "other").string(), { });
		expect(otherRoot && bound.Store->apply(operation(bound.NextOperation++), { .Subject = otherSubject, .ExpectedRevision = 1, .Change = core::bind_cycle_workspace{ .Root = *otherRoot } }, { }), "bind the other cycle");
		const auto before = bound.state();
		std::filesystem::rename(aScratch / "rebind" / "project", aScratch / "rebind" / "moved");
		const auto subject = bound.Subject;
		bound.Store.reset();

		auto lanes   = application::manual_executor{ };
		auto runtime = application::review_cycle_runtime{ adapters::sqlite::make_review_cycle_store(bound.StoreRoot.path(), profile), lanes, lanes };
		runtime.select(subject);
		auto checked = std::optional<application::cycle_store_result<application::cycle_rebind_check>>{ };
		expect(runtime.check_rebind_selected(*source, (aScratch / "rebind" / "moved").string(), [&](auto aValue) { checked = std::move(aValue); }), "admitted");
		lanes.drain();
		expect(checked && *checked, "checked");
		const auto& moved = **checked;
		expect(moved.Cycle == before && moved.Material.Status == cycle_capture_status::captured && moved.Material.Check, "the cycle as it stands, and a check");
		expect(moved.Material.Check->Compared == 1 && moved.Material.Check->Matching == 1, "the moved bytes match");
		expect(moved.Material.Root && moved.Material.Root->Root == before.Workspace->Root.Root, "the moved folder is the same root object");
		expect(moved.BoundElsewhere.empty() && core::admit_workspace_rebind(moved.Cycle, *moved.Material.Root, false) == core::workspace_rebind_problem::none, "and may be offered");

		checked.reset();
		expect(runtime.check_rebind_selected(*source, (aScratch / "rebind" / "other").string(), [&](auto aValue) { checked = std::move(aValue); }), "admitted");
		lanes.drain();
		expect(checked && *checked && (*checked)->BoundElsewhere == std::vector{ otherSubject.Cycle }, "another cycle's folder is named as that cycle's");

		checked.reset();
		expect(runtime.check_rebind_selected(*source, (aScratch / "rebind" / "nowhere").string(), [&](auto aValue) { checked = std::move(aValue); }), "admitted");
		lanes.drain();
		expect(checked && *checked && (*checked)->Material.Status == cycle_capture_status::workspace && !(*checked)->Material.Root, "a missing candidate is said as such");

		auto loaded = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		expect(runtime.load_selected([&](auto aValue) { loaded = std::move(aValue); }), "admitted");
		lanes.drain();
		expect(loaded && *loaded && **loaded && ***loaded == before, "checking changed nothing");
		runtime.shutdown();
	}

	void captures_through_the_runtime(const std::filesystem::path& aScratch)
	{
		write_file(aScratch / "runtime" / "file.txt", "runtime");
		auto       source  = adapters::workspace::make_local_workspace_source();
		auto       bound   = course{ *source, aScratch / "runtime" };
		const auto subject = bound.Subject;
		bound.Store.reset();
		auto lanes   = application::manual_executor{ };
		auto runtime = application::review_cycle_runtime{ adapters::sqlite::make_review_cycle_store(bound.StoreRoot.path(), profile), lanes, lanes };
		runtime.select(subject);
		auto result = std::optional<application::cycle_store_result<application::cycle_capture_outcome>>{ };
		expect(runtime.capture_workspace(*source, { .Operation = operation(90), .ExpectedRevision = 2 }, [&](auto aValue) { result = std::move(aValue); }), "admitted");
		lanes.drain();
		expect(result && *result && (*result)->Status == cycle_capture_status::captured, "runtime capture");
		auto pullRequest        = core::pull_request_authority{ };
		pullRequest.PullRequest = core::pull_request_id{ 4 };
		runtime.select(pullRequest);
		result.reset();
		expect(runtime.capture_workspace(*source, { .Operation = operation(91), .ExpectedRevision = 3 }, [&](auto aValue) { result = std::move(aValue); }), "admitted");
		lanes.drain();
		expect(result && !*result && result->error().Refusal == core::cycle_refusal::invalid_identity, "a pull-request selection is refused");
		runtime.shutdown();
	}
}

namespace {
	/// \brief [[BPR-1184]]. With a capture lane the runtime keeps serving
	/// reads while a capture runs; the first call that could move the cycle
	/// on waits for it, and so does everything queued after that call;
	/// progress arrives on the decision lane; a cancelled capture records
	/// nothing. Without one (every other test here) a capture is an ordinary
	/// call on the effect lane.
	void captures_beside_reads(const std::filesystem::path& aScratch)
	{
		for (std::size_t index = 0; index < 2100U; ++index) {
			write_file(aScratch / "beside" / ("f" + std::to_string(index) + ".txt"), "file " + std::to_string(index));
		}
		auto source = adapters::workspace::make_local_workspace_source();
		auto bound  = course{ *source, aScratch / "beside" };
		expect(bound.capture(*source).Status == cycle_capture_status::captured, "R1");
		const auto subject = bound.Subject;
		const auto first   = bound.state();
		bound.Store.reset();
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto captures  = application::manual_executor{ };
		auto runtime   = application::review_cycle_runtime{ adapters::sqlite::make_review_cycle_store(bound.StoreRoot.path(), profile), effects, decisions, &captures };
		runtime.select(subject);
		const auto settle = [&] {
			while (effects.pending() != 0U || decisions.pending() != 0U || captures.pending() != 0U) {
				effects.drain();
				decisions.drain();
				captures.drain();
			}
		};

		auto progress    = std::vector<application::cycle_capture_progress>{ };
		auto request     = application::cycle_capture_request{ .Operation = operation(92), .ExpectedRevision = first.Revision, .Deliberate = true };
		request.Progress = [&progress](const application::cycle_capture_progress& aProgress) { progress.push_back(aProgress); };
		auto captured    = std::optional<application::cycle_store_result<application::cycle_capture_outcome>>{ };
		expect(runtime.capture_workspace(*source, std::move(request), [&](auto aValue) { captured = std::move(aValue); }), "admitted");
		expect(runtime.capturing() && captures.pending() == 1U && effects.pending() == 0U, "the capture went to its own lane");

		auto loaded = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		expect(runtime.load_selected([&](auto aValue) { loaded = std::move(aValue); }), "admitted");
		expect(effects.pending() == 1U, "a read starts beside the capture");
		effects.drain();
		decisions.drain();
		expect(loaded && *loaded && **loaded && (**loaded)->Revision == first.Revision && !captured, "the read completed while the capture waits on its lane");

		auto marked = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		auto listed = std::optional<application::cycle_store_result<application::cycle_revision_page>>{ };
		expect(runtime.mark_reviewed_selected(operation(93), first.Latest->Id.Id, [&](auto aValue) { marked = std::move(aValue); }), "admitted");
		expect(runtime.revisions_selected(std::nullopt, [&](auto aValue) { listed = std::move(aValue); }), "admitted");
		expect(effects.pending() == 0U, "a write waits for the capture, and the read queued behind it");

		captures.drain();
		expect(effects.pending() == 0U && !captured, "the capture's result returns on the decision lane");
		decisions.drain();
		expect(captured && *captured && (*captured)->Status == cycle_capture_status::captured && (*captured)->Receipt && (*captured)->Receipt->Changed, "the deliberate capture recorded R2");
		expect(!runtime.capturing() && effects.pending() == 1U, "the waiting write starts once the capture completes");
		settle();
		expect(marked && *marked && listed && *listed && (*listed)->Revisions.size() == 2U, "the write and the read behind it complete, in order");
		const auto reading = std::ranges::count(progress, application::cycle_capture_progress::phase::reading, &application::cycle_capture_progress::Phase);
		expect(reading >= 3 && std::ranges::any_of(progress, [](const auto& aProgress) { return aProgress.Phase == application::cycle_capture_progress::phase::reading && aProgress.Files == 2048U; }),
			   "reading progress at the start and every 1,024 files");
		expect(std::ranges::any_of(progress, [](const auto& aProgress) { return aProgress.Phase == application::cycle_capture_progress::phase::checking; })
				   && progress.back().Phase == application::cycle_capture_progress::phase::recording && progress.back().Files == 2100U,
			   "then checking, then recording every path");

		// Cancelled while it reads: nothing recorded.
		auto again = std::optional<application::cycle_store_result<application::cycle_capture_outcome>>{ };
		auto now   = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		expect(runtime.load_selected([&](auto aValue) { now = std::move(aValue); }), "admitted");
		settle();
		expect(runtime.capture_workspace(*source, { .Operation = operation(94), .ExpectedRevision = (**now)->Revision, .Deliberate = true }, [&](auto aValue) { again = std::move(aValue); }), "admitted");
		runtime.cancel_capture();
		settle();
		// Cancelled while it loads the cycle or while it reads: either way, cancelled.
		expect(again && ((*again && (*again)->Status == cycle_capture_status::cancelled && !(*again)->Receipt) || (!*again && again->error().Failure == application::cycle_store_failure::cancelled)),
			   "a cancelled capture records nothing");
		listed.reset();
		expect(runtime.revisions_selected(std::nullopt, [&](auto aValue) { listed = std::move(aValue); }), "admitted");
		settle();
		expect(listed && *listed && (*listed)->Revisions.size() == 2U, "still two revisions");
		runtime.shutdown();
		settle();
		expect(runtime.is_quiescent(), "quiescent");
	}
}

#if defined(_WIN32)
namespace {
	/// \brief [[BPR-1284]]. A capture lists a directory junction as itself,
	/// unsupported, and reads nothing beyond it.
	void captures_a_junction_as_itself(const std::filesystem::path& aScratch)
	{
		const auto outside = aScratch / "capture-junction-outside";
		const auto root    = aScratch / "capture-junction-root";
		write_file(outside / "secret.txt", "secret beyond the root");
		write_file(root / "inside.txt", "inside");
		if (!tests::workspace_contract::try_junction(outside, root / "escape_dir")) {
			std::cout << "NATIVE: directory junctions unavailable on this volume; junction capture row not run\n";
			return;
		}
		auto       source  = adapters::workspace::make_local_workspace_source();
		auto       cycle   = course{ *source, root };
		const auto outcome = cycle.capture(*source, { .MaximumFileBytes = 1024 });
		expect(outcome.Status == cycle_capture_status::captured, "a root holding a junction is captured");
		const auto  manifest = cycle.manifest(outcome);
		const auto& junction = entry(manifest, "escape_dir");
		expect(junction.Availability == core::manifest_availability::unsupported_kind && !junction.Content, "the junction is listed as itself, unsupported, with nothing read");
		expect(std::ranges::none_of(manifest.Entries, [](const core::manifest_entry& aEntry) { return aEntry.Path.starts_with("escape_dir/"); }), "nothing beyond the junction is listed");
		expect(cycle.content(entry(manifest, "inside.txt")) == "inside", "the root's own file is captured");
		std::cout << "NATIVE: a directory junction captured as itself, unsupported, nothing beyond it read\n";
	}
}
#endif

auto main(const int aArgc, char** aArgv) -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-capture" };
	const auto                           git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (git) {
		leaves_the_checkout_untouched(scratch.path(), *git);
		records_scope_versions_deletions_and_exclusions(scratch.path(), *git);
	} else {
		std::cout << "NATIVE: no Git in this build; Git capture rows not run\n";
	}
	retries_detected_instability(scratch.path());
	preserves_bytes_modes_and_special_files(scratch.path(), git);
#if defined(_WIN32)
	captures_a_junction_as_itself(scratch.path());
#endif
	checks_material_before_rebind(scratch.path());
	captures_through_the_runtime(scratch.path());
	checks_rebinds_through_the_runtime(scratch.path());
	captures_beside_reads(scratch.path());
	std::cout << "review-cycle-capture-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
