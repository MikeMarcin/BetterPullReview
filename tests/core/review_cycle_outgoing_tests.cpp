#include "bpr/core/review_cycle_outgoing.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Selected outgoing content over an explicit Git base (BPR-1172):
/// dirty starting work, unrelated edits, module-only scope, scope extension,
/// deletion, rename and mode change, an empty candidate, a moved target and
/// an improvement that depends on omitted starting work.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle outgoing failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// Content named by a short label: its capture digest and its blob.
	struct content
	{
		cycle_material_digest Digest;
		git_entry_oid         Blob;
	};

	auto make(const std::string_view aLabel, const git_hash_algorithm aAlgorithm = git_hash_algorithm::sha1) -> content
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		for (std::size_t index = 0; index < aLabel.size() && index < 31U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aLabel[index]);
		}
		bytes[31] = 0x5A;
		auto blob = std::array<std::uint8_t, 32>{ };
		for (std::size_t index = 0; index < aLabel.size() && index < 19U; ++index) {
			blob[index] = static_cast<std::uint8_t>(aLabel[index]);
		}
		blob[19] = 0xB1;
		if (aAlgorithm == git_hash_algorithm::sha256) {
			blob[31] = 0xB2;
		}
		return { .Digest = cycle_material_digest{ bytes }, .Blob = git_entry_oid{ aAlgorithm, blob } };
	}

	auto file(std::string aPath, const std::string_view aLabel, const manifest_origin aOrigin = manifest_origin::tracked, const std::uint32_t aMode = git_mode_regular) -> manifest_entry
	{
		return { .Path = std::move(aPath), .Origin = aOrigin, .Mode = aMode, .Size = 1, .Content = make(aLabel).Digest, .Profile = { .Kind = material_text_kind::utf8, .Size = 1 } };
	}

	auto capture(std::vector<manifest_entry> aEntries, std::vector<std::string> aDeletions = { }, std::vector<std::string> aScope = { }, const cycle_capture_coverage aCoverage = cycle_capture_coverage::complete)
		-> cycle_manifest
	{
		std::ranges::sort(aEntries, { }, &manifest_entry::Path);
		std::ranges::sort(aDeletions);
		return { .ScopeVersion = 1, .Scope = std::move(aScope), .RootKind = workspace_root_kind::git_checkout, .Coverage = aCoverage, .ObjectFormat = git_hash_algorithm::sha1, .Entries = std::move(aEntries), .Deletions = std::move(aDeletions) };
	}

	auto commit(const std::uint8_t aSeed) -> git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[1]   = 0xC0;
		return git_commit_oid{ git_hash_algorithm::sha1, bytes };
	}

	auto base(std::vector<std::pair<std::string, std::string>> aFiles, const std::uint8_t aSeed = 1, const std::map<std::string, std::uint32_t>& aModes = { }) -> publication_base
	{
		std::ranges::sort(aFiles);
		auto entries = std::vector<git_tree_entry>{ };
		for (const auto& [path, label] : aFiles) {
			const auto mode = aModes.contains(path) ? aModes.at(path) : git_mode_regular;
			entries.push_back({ .Path = path, .Mode = mode, .Object = make(label).Blob });
		}
		const auto tree = build_git_trees(entries, git_hash_algorithm::sha1);
		expect(tree.has_value(), "fixture base tree");
		return { .Commit = commit(aSeed), .Tree = tree->Root, .Entries = std::move(entries), .Reference = "refs/remotes/origin/main" };
	}

	auto identities(const std::span<const std::string_view> aLabels) -> std::vector<outgoing_identity>
	{
		auto out = std::vector<outgoing_identity>{ };
		for (const auto label : aLabels) {
			const auto value = make(label);
			out.push_back({ .Content = value.Digest, .Raw = value.Blob });
		}
		return out;
	}

	auto row(const outgoing_plan& aPlan, const std::string_view aPath) -> const outgoing_row*
	{
		const auto found = std::ranges::find(aPlan.Rows, aPath, &outgoing_row::Path);
		return found == aPlan.Rows.end() ? nullptr : &*found;
	}

	auto candidate_has(const outgoing_plan& aPlan, const std::string_view aPath, const std::string_view aLabel) -> bool
	{
		const auto found = std::ranges::find(aPlan.Candidate, aPath, &git_tree_entry::Path);
		return found != aPlan.Candidate.end() && found->Object == make(aLabel).Blob;
	}

	auto candidate_lacks(const outgoing_plan& aPlan, const std::string_view aPath) -> bool
	{
		return std::ranges::find(aPlan.Candidate, aPath, &git_tree_entry::Path) == aPlan.Candidate.end();
	}

	constexpr auto labels = std::array<std::string_view, 14>{ "X", "A0", "A1", "A2", "B0", "B1", "C0", "O", "R", "G", "N", "S", "LFS0", "LFS1" };

	/// P, a dirty start and an iterated final capture of the whole checkout.
	auto whole_request(const outgoing_selection_mode aMode) -> outgoing_request
	{
		auto request = outgoing_request{
			.Mode          = aMode,
			.StartRevision = cycle_source_revision_id{ 1 },
			.FinalRevision = cycle_source_revision_id{ 3 },
			.Start         = capture({ file("README.md", "X"), file("src/a.cpp", "A1"), file("src/b.cpp", "B0"), file("src/c.cpp", "C0"), file("src/old.cpp", "O"), file("tools/run.sh", "R"),
									   file("docs/guide.md", "G"), file("notes.txt", "N", manifest_origin::untracked), file("src/draft.cpp", "S", manifest_origin::untracked) }),
			.Final         = capture({ file("README.md", "X"), file("src/a.cpp", "A2"), file("src/b.cpp", "B1"), file("src/c.cpp", "C0"), file("src/new.cpp", "O", manifest_origin::untracked),
									   file("tools/run.sh", "R", manifest_origin::tracked, git_mode_executable), file("docs/guide.md", "G"), file("notes.txt", "N", manifest_origin::untracked),
									   file("src/draft.cpp", "S", manifest_origin::untracked) },
									 { "src/old.cpp" }),
			.Base          = base({ { "README.md", "X" }, { "src/a.cpp", "A0" }, { "src/b.cpp", "B0" }, { "src/c.cpp", "C0" }, { "src/old.cpp", "O" }, { "tools/run.sh", "R" }, { "docs/guide.md", "G" } }),
			.Identities    = identities(labels),
			.ReviewedPaths = { "src/draft.cpp" },
		};
		return request;
	}

	void starting_work_and_improvements()
	{
		const auto plan = plan_outgoing(whole_request(outgoing_selection_mode::starting_work_and_improvements));
		expect(plan.has_value(), "planned");
		expect(plan->buildable() && !plan->NoChange && plan->CandidateTree.valid(), "buildable");
		const auto* a = row(*plan, "src/a.cpp");
		expect(a && a->StartingWork == outgoing_change::modified && a->Improvement == outgoing_change::modified && a->Selected && a->Outgoing == outgoing_change::modified,
			   "dirty starting work with an improvement on top goes out whole");
		expect(row(*plan, "src/b.cpp")->Selected && row(*plan, "src/b.cpp")->StartingWork == outgoing_change::unchanged, "an improvement over P");
		const auto* notes = row(*plan, "notes.txt");
		expect(notes && notes->Unrelated && !notes->Proposed && !notes->Selected && candidate_lacks(*plan, "notes.txt"), "unrelated untracked dirt is listed, never proposed");
		expect(row(*plan, "src/draft.cpp")->Selected && candidate_has(*plan, "src/draft.cpp", "S"), "untracked starting work a note names is relevant");
		const auto* old = row(*plan, "src/old.cpp");
		expect(old && old->Outgoing == outgoing_change::deleted && old->RenamedTo == "src/new.cpp" && candidate_lacks(*plan, "src/old.cpp") && candidate_has(*plan, "src/new.cpp", "O"),
			   "a rename is a deletion and an addition, named as a rename");
		const auto* run = row(*plan, "tools/run.sh");
		expect(run && run->Outgoing == outgoing_change::mode_changed && run->Final.Mode == git_mode_executable, "a mode change");
		expect(row(*plan, "README.md") == nullptr && candidate_has(*plan, "README.md", "X") && candidate_has(*plan, "docs/guide.md", "G") && candidate_has(*plan, "src/c.cpp", "C0"),
			   "untouched entries are P's, and not listed");
		expect(plan->BaseCommit == commit(1) && plan->StartRevision == cycle_source_revision_id{ 1 } && plan->FinalRevision == cycle_source_revision_id{ 3 }, "three baselines kept apart");

		const auto preview = render_outgoing_preview(*plan);
		for (const auto expected : { "Publication preview: nothing has been published.", "Parent P: ", "Inspected R1 (complete); final R3 (complete).", "starting work and cycle improvement:  src/a.cpp",
									 "renamed to src/new.cpp", "mode 100644 -> 100755", "starting work nothing in the cycle touched:  notes.txt", "Target T: not observed." }) {
			expect(preview.find(expected) != std::string::npos, expected);
		}
	}

	void improvements_only()
	{
		auto       request = whole_request(outgoing_selection_mode::improvements_only);
		const auto plan    = plan_outgoing(request);
		expect(plan.has_value(), "planned");
		const auto* a = row(*plan, "src/a.cpp");
		expect(a && a->Gap == outgoing_gap::depends_on_starting_work && !a->Selected && candidate_has(*plan, "src/a.cpp", "A0"),
			   "an improvement over omitted starting work is not a clean cherry-pick");
		expect(plan->Undecided == 1U && !plan->buildable(), "it waits for a decision");
		expect(!row(*plan, "src/draft.cpp")->Selected && candidate_lacks(*plan, "src/draft.cpp"), "starting work stays out");
		expect(row(*plan, "src/b.cpp")->Selected && row(*plan, "tools/run.sh")->Selected && row(*plan, "src/old.cpp")->Selected, "improvements go out");
		expect(render_outgoing_preview(*plan).find("the improvement depends on starting work that is not included:  src/a.cpp") != std::string::npos, "the decision is shown");

		request.Overrides   = { { .Path = "src/a.cpp", .Choice = outgoing_choice::include } };
		const auto included = plan_outgoing(request);
		expect(included && included->Undecided == 0U && included->buildable() && candidate_has(*included, "src/a.cpp", "A2"), "including it takes the final bytes, starting work and all");
		request.Overrides   = { { .Path = "src/a.cpp", .Choice = outgoing_choice::exclude } };
		const auto excluded = plan_outgoing(request);
		expect(excluded && excluded->Undecided == 0U && candidate_has(*excluded, "src/a.cpp", "A0"), "leaving it out keeps P's bytes");
		request.Overrides = { { .Path = "README.md", .Choice = outgoing_choice::include } };
		expect(plan_outgoing(request).error() == outgoing_refusal::unknown_override, "a path with nothing to send takes no choice");
		request.Overrides = { { .Path = "src/b.cpp", .Choice = outgoing_choice::exclude }, { .Path = "src/b.cpp", .Choice = outgoing_choice::include } };
		expect(plan_outgoing(request).error() == outgoing_refusal::unknown_override, "one choice per path");
	}

	void module_scope_and_extension()
	{
		// A module-only capture never deletes the rest of the repository.
		auto request  = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Start = capture({ file("src/a.cpp", "A0"), file("src/b.cpp", "B0") }, { }, { "src" }, cycle_capture_coverage::partial);
		request.Final = capture({ file("src/a.cpp", "A2"), file("src/b.cpp", "B0") }, { }, { "src" }, cycle_capture_coverage::partial);
		request.ReviewedPaths.clear();
		auto plan = plan_outgoing(request);
		expect(plan.has_value(), "module plan");
		for (const auto path : { "README.md", "docs/guide.md", "tools/run.sh", "src/c.cpp", "src/old.cpp" }) {
			expect(std::ranges::find(plan->Candidate, std::string_view{ path }, &git_tree_entry::Path) != plan->Candidate.end(), path);
		}
		expect(plan->Rows.size() == 1U && row(*plan, "src/a.cpp")->Selected && !plan->StartComplete, "only the module's improvement goes out");

		// A complete capture that excluded a folder says nothing about it.
		request.Start            = capture({ file("src/a.cpp", "A0") }, { }, { "src" });
		request.Final            = capture({ file("src/a.cpp", "A2") }, { }, { "src" });
		request.Final.Exclusions = { { .Path = "src/generated", .Directory = true, .Reason = workspace_exclusion_reason::requested } };
		request.Base             = base({ { "src/a.cpp", "A0" }, { "src/generated/table.cpp", "G" } });
		request.Start.Exclusions = request.Final.Exclusions;
		plan                     = plan_outgoing(request);
		expect(plan && candidate_has(*plan, "src/generated/table.cpp", "G") && row(*plan, "src/generated/table.cpp") == nullptr, "an excluded folder keeps P's entries");

		// Scope extension: the final capture reads a file the start never saw.
		request.Start = capture({ file("src/a.cpp", "A0") }, { }, { "src/a.cpp" });
		request.Final = capture({ file("src/a.cpp", "A2"), file("src/b.cpp", "B1") }, { }, { "src" });
		request.Base  = base({ { "src/a.cpp", "A0" }, { "src/b.cpp", "B0" } });
		plan          = plan_outgoing(request);
		const auto* b = plan ? row(*plan, "src/b.cpp") : nullptr;
		expect(b && b->Gap == outgoing_gap::unknown_start && b->StartingWork == outgoing_change::unknown && !b->Proposed && candidate_has(*plan, "src/b.cpp", "B0"),
			   "a change outside the starting capture has no known origin and is not proposed");
		expect(plan->buildable(), "an origin-unknown row does not block the rest");
		request.Overrides = { { .Path = "src/b.cpp", .Choice = outgoing_choice::include } };
		plan              = plan_outgoing(request);
		expect(plan && candidate_has(*plan, "src/b.cpp", "B1"), "it can be included deliberately");

		// A final capture that narrowed its scope keeps P for what it left.
		request.Overrides.clear();
		request.Start = capture({ file("src/a.cpp", "A1"), file("src/b.cpp", "B1") }, { }, { "src" });
		request.Final = capture({ file("src/a.cpp", "A2") }, { }, { "src/a.cpp" });
		plan          = plan_outgoing(request);
		expect(plan && row(*plan, "src/b.cpp")->Gap == outgoing_gap::final_unknown && candidate_has(*plan, "src/b.cpp", "B0"), "starting work the final capture did not read stays out");
	}

	void empty_and_drift()
	{
		auto request  = whole_request(outgoing_selection_mode::improvements_only);
		request.Start = capture({ file("README.md", "X"), file("src/a.cpp", "A1") }, { }, { }, cycle_capture_coverage::partial);
		request.Final = request.Start;
		auto plan     = plan_outgoing(request);
		expect(plan && plan->NoChange && !plan->buildable() && plan->CandidateTree == plan->BaseTree, "starting work alone under improvements-only: nothing to publish");
		expect(render_outgoing_preview(*plan).find("No change: the candidate's tree is P's tree.") != std::string::npos, "said plainly");

		request        = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		auto target    = base({ { "README.md", "X" }, { "src/a.cpp", "A0" }, { "src/b.cpp", "B9" }, { "src/c.cpp", "C0" }, { "src/old.cpp", "O" }, { "tools/run.sh", "R" }, { "docs/guide.md", "G" } }, 2);
		request.Target = publication_target{ .Reference = "refs/remotes/origin/main", .Source = publication_target_source::local_reference, .Head = target.Commit, .Entries = target.Entries };
		plan           = plan_outgoing(request);
		expect(plan && plan->TargetMoved && row(*plan, "src/b.cpp")->Drifted && !row(*plan, "src/a.cpp")->Drifted, "the target moved and changed one selected path");
		const auto preview = render_outgoing_preview(*plan);
		expect(preview.find("it has moved since P: the candidate's parent is stale against it.") != std::string::npos && preview.find("the target changed this path since P") != std::string::npos
				   && preview.find("this checkout's local reference, not the remote now") != std::string::npos,
			   "base drift is visible");
		request.Target->Head = request.Base.Commit;
		plan                 = plan_outgoing(request);
		expect(plan && !plan->TargetMoved && !row(*plan, "src/b.cpp")->Drifted, "a target at P has not moved");
	}

	void limits_and_refusals()
	{
		// Bytes a filter manages are never built here.
		auto request         = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		auto large           = file("assets/model.bin", "LFS1");
		large.FilterManaged  = true;
		auto before          = file("assets/model.bin", "LFS0");
		before.FilterManaged = true;
		request.Start.Entries.push_back(before);
		request.Final.Entries.push_back(large);
		std::ranges::sort(request.Start.Entries, { }, &manifest_entry::Path);
		std::ranges::sort(request.Final.Entries, { }, &manifest_entry::Path);
		auto files        = std::vector<std::pair<std::string, std::string>>{ { "README.md", "X" }, { "src/a.cpp", "A0" }, { "src/b.cpp", "B0" }, { "src/c.cpp", "C0" }, { "src/old.cpp", "O" }, { "tools/run.sh", "R" }, { "docs/guide.md", "G" }, { "assets/model.bin", "P-pointer" } };
		request.Base      = base(files);
		auto        plan  = plan_outgoing(request);
		const auto* model = plan ? row(*plan, "assets/model.bin") : nullptr;
		expect(model && model->Gap == outgoing_gap::filter_managed && model->Selected && model->StartingWork == outgoing_change::unknown && plan->Blocked == 1U && !plan->buildable(),
			   "a changed LFS file is a capability limit, not a raw upload");
		expect(render_outgoing_preview(*plan).find("a Git filter such as LFS makes its committed object:  assets/model.bin") != std::string::npos, "the limit is shown");
		request.Overrides = { { .Path = "assets/model.bin", .Choice = outgoing_choice::exclude } };
		plan              = plan_outgoing(request);
		expect(plan && plan->Blocked == 0U && plan->buildable() && candidate_has(*plan, "assets/model.bin", "P-pointer"), "leaving it out keeps P's pointer");

		// Line endings alone are not starting work.
		request                                                                                                                                                              = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		auto crlf                                                                                                                                                            = file("src/c.cpp", "C0-crlf");
		request.Start.Entries[std::size_t(std::ranges::find(request.Start.Entries, std::string_view{ "src/c.cpp" }, &manifest_entry::Path) - request.Start.Entries.begin())] = crlf;
		request.Final.Entries[std::size_t(std::ranges::find(request.Final.Entries, std::string_view{ "src/c.cpp" }, &manifest_entry::Path) - request.Final.Entries.begin())] = crlf;
		request.Identities.push_back({ .Content = make("C0-crlf").Digest, .Raw = make("C0-crlf").Blob, .LineFeed = make("C0").Blob });
		plan = plan_outgoing(request);
		expect(plan && row(*plan, "src/c.cpp") == nullptr && candidate_has(*plan, "src/c.cpp", "C0"), "a CRLF checkout of P's LF bytes is not a change");

		// A file and a folder at one path.
		request = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Final.Entries.push_back(file("tools/run.sh/inner", "N", manifest_origin::untracked));
		std::ranges::sort(request.Final.Entries, { }, &manifest_entry::Path);
		request.Start.Entries.push_back(file("tools/run.sh/inner", "N", manifest_origin::untracked));
		std::ranges::sort(request.Start.Entries, { }, &manifest_entry::Path);
		request.ReviewedPaths = { "tools" };
		plan                  = plan_outgoing(request);
		expect(plan && row(*plan, "tools/run.sh/inner")->Gap == outgoing_gap::path_conflict && plan->Blocked >= 1U && !plan->CandidateTree.valid(), "never an invalid tree");

		request                = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Final.RootKind = workspace_root_kind::folder;
		expect(plan_outgoing(request).error() == outgoing_refusal::repository_required, "a plain folder needs a repository; no synthetic root commit");
		request                    = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Final.ObjectFormat = git_hash_algorithm::sha256;
		expect(plan_outgoing(request).error() == outgoing_refusal::object_format, "one object format throughout");
		request           = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Base.Tree = base({ { "README.md", "X" } }).Tree;
		expect(plan_outgoing(request).error() == outgoing_refusal::invalid_base, "a listing that is not P's tree");
		request = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Identities.erase(request.Identities.begin() + 3);
		expect(plan_outgoing(request).error() == outgoing_refusal::missing_identity, "every captured object needs its identity");
		request               = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.StartRevision = cycle_source_revision_id{ 4 };
		expect(plan_outgoing(request).error() == outgoing_refusal::invalid_capture, "the final capture does not precede the start");
		const auto again = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		expect(plan_outgoing(again) == plan_outgoing(again), "deterministic");
	}

	/// [[BPR-1244]]. A path the final capture saw but could not read is listed
	/// with its reason and counted; it is never silently absent from the
	/// candidate and the preview.
	void unread_final_paths()
	{
		const auto with = [](cycle_manifest aManifest, manifest_entry aEntry) {
			std::erase_if(aManifest.Entries, [&aEntry](const manifest_entry& aKnown) { return aKnown.Path == aEntry.Path; });
			aManifest.Entries.push_back(std::move(aEntry));
			std::ranges::sort(aManifest.Entries, { }, &manifest_entry::Path);
			return aManifest;
		};
		const auto unread = [](std::string aPath, const manifest_entry_kind aKind, const manifest_availability aWhy) {
			return manifest_entry{ .Path = std::move(aPath), .Kind = aKind, .Origin = manifest_origin::untracked, .Availability = aWhy };
		};

		// A link added in the cycle whose target this platform cannot read.
		auto request     = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Final    = with(request.Final, unread("docs-link", manifest_entry_kind::symlink, manifest_availability::link_target_unsupported));
		auto        plan = plan_outgoing(request);
		const auto* link = plan ? row(*plan, "docs-link") : nullptr;
		expect(link && link->Gap == outgoing_gap::final_unavailable && !link->Selected && link->Final.State == outgoing_state::unavailable
				   && link->Final.Unread == manifest_availability::link_target_unsupported && candidate_lacks(*plan, "docs-link"),
			   "a link the final capture could not read is listed, not silently absent");
		expect(plan->Unread == 1U && plan->buildable(), "it is counted, and the rest still builds");
		auto preview = render_outgoing_preview(*plan);
		expect(preview.find("1 path the final capture could not read is not in the candidate as captured.") != std::string::npos
				   && preview.find("the final capture saw it but could not read it (this platform cannot read its link target); it is not in the candidate:  docs-link")
					   != std::string::npos,
			   "the preview says so, with the reason, before anything is published");

		// An improvement the final capture could not read keeps P's entry, and says why.
		request           = whole_request(outgoing_selection_mode::starting_work_and_improvements);
		request.Final     = with(request.Final, unread("src/c.cpp", manifest_entry_kind::file, manifest_availability::too_large));
		plan              = plan_outgoing(request);
		const auto* large = plan ? row(*plan, "src/c.cpp") : nullptr;
		expect(large && large->Gap == outgoing_gap::final_unavailable && !large->Selected && candidate_has(*plan, "src/c.cpp", "C0") && plan->Unread == 1U,
			   "a file grown past the capture budget keeps P's entry and is listed");
		preview = render_outgoing_preview(*plan);
		expect(preview.find("the final capture saw it but could not read it (too large to read); P's entry stays:  src/c.cpp") != std::string::npos, "with its reason");

		// Unread by both captures and in P: nothing is known to have changed.
		request.Start = with(request.Start, unread("src/c.cpp", manifest_entry_kind::file, manifest_availability::sparse));
		request.Final = with(request.Final, unread("src/c.cpp", manifest_entry_kind::file, manifest_availability::sparse));
		plan          = plan_outgoing(request);
		expect(plan && row(*plan, "src/c.cpp") == nullptr && plan->Unread == 0U && candidate_has(*plan, "src/c.cpp", "C0"),
			   "a path neither capture read and P has is not listed");
	}
}

auto main() -> int
{
	starting_work_and_improvements();
	improvements_only();
	module_scope_and_extension();
	empty_and_drift();
	limits_and_refusals();
	unread_final_paths();
	std::cout << "review-cycle-outgoing-tests:pass\n";
	return EXIT_SUCCESS;
}
