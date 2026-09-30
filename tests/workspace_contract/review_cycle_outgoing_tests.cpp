#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_outgoing.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "publication_course.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Planning outgoing content through the production host, store,
/// workspace source and installed-Git publication catalog (RFC-BPR-0059,
/// BPR-1172): dirty starting work, unrelated edits, a rename, a mode change,
/// a moved target read from loose and packed references, module-only scope,
/// improvements that depend on omitted starting work and a plain folder, in
/// SHA-1 and SHA-256 repositories. The checkout and its repository are
/// byte-for-byte unchanged by planning.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_outgoing_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::publication_git;
	using tests::workspace_contract::session;
	using tests::workspace_contract::snapshot;
	using tests::workspace_contract::write_file;

	auto row(const core::outgoing_plan& aPlan, const std::string_view aPath) -> const core::outgoing_row*
	{
		const auto found = std::ranges::find(aPlan.Rows, aPath, &core::outgoing_row::Path);
		return found == aPlan.Rows.end() ? nullptr : &*found;
	}

	auto in_candidate(const core::outgoing_plan& aPlan, const std::string_view aPath) -> bool
	{
		return std::ranges::find(aPlan.Candidate, aPath, &core::git_tree_entry::Path) != aPlan.Candidate.end();
	}

	auto plan(course& aCycle, application::workspace_source_port& aSource, publication_git& aGit, application::cycle_outgoing_request aRequest) -> application::cycle_outgoing_outcome
	{
		auto host = application::review_cycle_outgoing_host{ aSource, *aCycle.Store, *aGit.Port };
		return host.plan(aCycle.state(), aRequest, { });
	}

	void write_base(const std::filesystem::path& aRoot)
	{
		write_file(aRoot / "README.md", "# Parser\n");
		write_file(aRoot / "src" / "a.cpp", "int a() { return 0; }\n");
		write_file(aRoot / "src" / "b.cpp", "int b() { return 0; }\n");
		write_file(aRoot / "src" / "old.cpp", "int moved() { return 7; }\n");
		write_file(aRoot / "tools" / "run.sh", "#!/bin/sh\nexit 0\n");
		write_file(aRoot / "docs" / "guide.md", "Read the source.\n");
	}

	void plans_over_an_explicit_base(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable, const bool aSha256)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto root   = aScratch / (aSha256 ? "sha256" : "sha1") / "project";
		std::filesystem::create_directories(root);
		if (aSha256) {
			aGit(root, { "init", "-q", "--object-format=sha256" });
		} else {
			aGit(root, { "init", "-q" });
		}
		write_base(root);
		aGit(root, { "add", "." });
		aGit(root, { "commit", "-q", "-m", "base" });
		// Upstream moved on and changed b.cpp; the checkout stays on P.
		aGit(root, { "checkout", "-q", "-b", "upstream" });
		write_file(root / "src" / "b.cpp", "int b() { return 1; }\n");
		aGit(root, { "commit", "-q", "-am", "upstream" });
		aGit(root, { "update-ref", "refs/remotes/origin/main", "HEAD" });
		aGit(root, { "checkout", "-q", "main" });
		const auto head = aGit.output(root, { "rev-parse", "HEAD" }, aScratch / "head.txt");

		// Dirty starting work, an unrelated scratch file and a draft.
		write_file(root / "src" / "a.cpp", "int a() { return 1; }\n");
		write_file(root / "notes.txt", "remember milk\n");
		write_file(root / "src" / "draft.cpp", "int draft() { return 2; }\n");
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		{
			auto notes = session{ cycle.StoreRoot.path(), cycle.Subject };
			expect(notes.begin({ .Primary = notes.anchor(1, "src/draft.cpp", "draft()"), .Body = "Keep this one." }).has_value(), "a note names the draft");
		}

		// The iteration: improvements, a rename and an executable bit.
		write_file(root / "src" / "a.cpp", "int a() { return 2; }\n");
		write_file(root / "src" / "b.cpp", "int b() { return 3; }\n");
		aGit(root, { "mv", "src/old.cpp", "src/new.cpp" });
#if !defined(_WIN32)
		std::filesystem::permissions(root / "tools" / "run.sh", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#endif
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2");
		const auto before = snapshot(root);

		const auto outcome = plan(cycle, *source, git, { .Base = "HEAD", .Target = "refs/remotes/origin/main" });
		expect(outcome.Status == cycle_outgoing_status::planned && outcome.Plan, "planned");
		const auto& planned = *outcome.Plan;
		expect(std::string{ core::canonical_hex(planned.BaseCommit).view() } == head && planned.BaseReference == "HEAD", "P is the exact commit HEAD named");
		expect(planned.BaseCommit.algorithm() == (aSha256 ? core::git_hash_algorithm::sha256 : core::git_hash_algorithm::sha1), "the repository's own object format");
		expect(planned.StartRevision == core::cycle_source_revision_id{ 1 } && planned.FinalRevision == core::cycle_source_revision_id{ 2 }, "R0 and Rn");
		expect(planned.TargetMoved && planned.TargetSource == core::publication_target_source::local_reference, "T moved past P, as the checkout knows it");
		const auto* a = row(planned, "src/a.cpp");
		expect(a && a->StartingWork == core::outgoing_change::modified && a->Improvement == core::outgoing_change::modified && a->Selected, "dirty starting work under an improvement");
		const auto* b = row(planned, "src/b.cpp");
		expect(b && b->Selected && b->StartingWork == core::outgoing_change::unchanged && b->Drifted, "an improvement to a path the target has since changed");
		const auto* old = row(planned, "src/old.cpp");
		expect(old && old->Outgoing == core::outgoing_change::deleted && old->RenamedTo == "src/new.cpp" && in_candidate(planned, "src/new.cpp") && !in_candidate(planned, "src/old.cpp"),
			   "the rename");
#if !defined(_WIN32)
		const auto* run = row(planned, "tools/run.sh");
		expect(run && run->Outgoing == core::outgoing_change::mode_changed && run->Final.Mode == core::git_mode_executable, "the executable bit");
#endif
		expect(row(planned, "notes.txt") && row(planned, "notes.txt")->Unrelated && !in_candidate(planned, "notes.txt"), "the scratch file stays out");
		expect(row(planned, "src/draft.cpp") && row(planned, "src/draft.cpp")->Selected, "the draft a note names goes out");
		expect(in_candidate(planned, "README.md") && in_candidate(planned, "docs/guide.md") && row(planned, "README.md") == nullptr, "P's untouched entries");
		expect(planned.buildable() && planned.CandidateTree.valid(), "a buildable candidate");
		expect(outcome.Preview.find("Target T: refs/remotes/origin/main at ") != std::string::npos && outcome.Preview.find("the target changed this path since P") != std::string::npos,
			   "the preview names the drift");
		expect(snapshot(root) == before, "planning changed no byte of the checkout or its repository");

		// Improvements only: the improvement over omitted starting work waits.
		auto onlyImprovements = plan(cycle, *source, git, { .Base = "HEAD", .Mode = core::outgoing_selection_mode::improvements_only });
		expect(onlyImprovements.Plan && row(*onlyImprovements.Plan, "src/a.cpp")->Gap == core::outgoing_gap::depends_on_starting_work && onlyImprovements.Plan->Undecided == 1U,
			   "a decision, not a clean cherry-pick");
		expect(!row(*onlyImprovements.Plan, "src/draft.cpp")->Selected && !onlyImprovements.Plan->TargetHead, "starting work out; no target named");

		// References are read as data, loose or packed; exact commits too.
		aGit(root, { "pack-refs", "--all" });
		const auto packed = plan(cycle, *source, git, { .Base = "refs/heads/main", .Target = "refs/remotes/origin/main" });
		expect(packed.Plan && packed.Plan->BaseCommit == planned.BaseCommit && packed.Plan->TargetHead == planned.TargetHead, "packed references resolve the same");
		const auto exact = plan(cycle, *source, git, { .Base = head });
		expect(exact.Plan && exact.Plan->BaseCommit == planned.BaseCommit && exact.Plan->BaseReference.empty(), "an exact commit");
		expect(plan(cycle, *source, git, { .Base = "refs/heads/nowhere" }).Status == cycle_outgoing_status::reference_unavailable, "a missing reference");
		expect(plan(cycle, *source, git, { .Base = "refs/heads/../main" }).Status == cycle_outgoing_status::reference_unavailable, "an unsafe reference name");
		expect(plan(cycle, *source, git, { .Base = "main" }).Status == cycle_outgoing_status::reference_unavailable, "a short name is not guessed");
		auto missing = std::string(head.size(), 'e');
		expect(plan(cycle, *source, git, { .Base = missing }).Status == cycle_outgoing_status::repository, "a commit the repository does not hold");

		// A module-only cycle never deletes the rest of the repository.
		auto module = course{ *source, root, { "src" } };
		expect(module.capture(*source).Status == cycle_capture_status::captured, "module R1");
		write_file(root / "src" / "a.cpp", "int a() { return 4; }\n");
		expect(module.capture(*source).Status == cycle_capture_status::captured, "module R2");
		const auto scoped = plan(module, *source, git, { .Base = "HEAD" });
		expect(scoped.Plan && in_candidate(*scoped.Plan, "README.md") && in_candidate(*scoped.Plan, "docs/guide.md") && in_candidate(*scoped.Plan, "tools/run.sh"), "outside the module, P stays");
		expect(row(*scoped.Plan, "src/a.cpp")->Selected && row(*scoped.Plan, "notes.txt") == nullptr, "inside it, only what the module saw");
		std::cout << "NATIVE: " << (aSha256 ? "SHA-256" : "SHA-1") << " outgoing plan keeps R0, Rn, P and T apart and changes nothing on disk\n";
	}

	void refuses_a_plain_folder(const std::filesystem::path& aScratch, const std::filesystem::path& aExecutable)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto root   = aScratch / "folder";
		write_file(root / "a.txt", "plain\n");
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		const auto outcome = plan(cycle, *source, git, { .Base = "HEAD" });
		expect(outcome.Status == cycle_outgoing_status::refused && outcome.Refusal == core::outgoing_refusal::repository_required, "no synthetic root commit for a plain folder");
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	const auto executable = tests::workspace_contract::git_executable(aArgc, aArgv);
	const auto fixtures   = tests::workspace_contract::git_from(aArgc, aArgv);
	expect(executable.has_value() && fixtures.has_value(), "publication needs the build's Git (--git)");
	tests::sqlite_contract::scratch_root scratch{ "cycle-outgoing" };
	plans_over_an_explicit_base(scratch.path(), *fixtures, *executable, false);
	plans_over_an_explicit_base(scratch.path(), *fixtures, *executable, true);
	refuses_a_plain_folder(scratch.path(), *executable);
	std::cout << "review-cycle-outgoing-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle outgoing failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
