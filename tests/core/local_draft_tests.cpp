/// \file
/// \brief RFC-BPR-0108 ([[BPR-2019]], [[BPR-2020]]). The local PR draft's
/// decisions without any file: the remote chain from an isolated local clone
/// to its hosted repository, the target and comparison an observation
/// supports, the candidate's changes with only selected working files, which
/// draft an opened worktree resumes, bounded admission and the stored form.

#include "bpr/core/local_draft.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto commit(const char aDigit) -> core::git_commit_oid
	{
		return *core::parse_git_commit_oid(std::string(40U, aDigit));
	}

	auto entry_oid(const char aDigit) -> core::git_entry_oid
	{
		return *core::parse_git_entry_oid(std::string(40U, aDigit));
	}

	auto github(std::string aOwner, std::string aName) -> core::remote_location
	{
		return { .Host = "github.com", .Owner = std::move(aOwner), .Name = std::move(aName) };
	}

	void test_remote_chain()
	{
		// The isolated worktree: a local clone whose origin is the primary
		// clone, which names GitHub.
		auto       walk  = core::remote_chain_walk{ };
		const auto first = core::step_remote_chain(walk, "/work/feature", std::vector<core::git_remote>{ { .Name = "origin", .Url = "/src/primary" } });
		const auto* follow = std::get_if<core::remote_chain_follow>(&first);
		expect(follow != nullptr && follow->Path == "/src/primary" && follow->Remote == "origin", "a local origin is followed");
		walk.Hops.push_back({ .Remote = follow->Remote, .Location = "/src/primary" });
		const auto second   = core::step_remote_chain(walk, "/src/primary", std::vector<core::git_remote>{ { .Name = "origin", .Url = "git@github.com:acme/engine.git" } });
		const auto* resolved = std::get_if<core::remote_chain_resolved>(&second);
		expect(resolved != nullptr && resolved->Hops.size() == 1U && resolved->Destinations.size() == 1U && resolved->Destinations[0].Repository == github("acme", "engine"),
			   "the chain resolves to the hosted remote, with its hop");

		auto       cyclic = core::remote_chain_walk{ };
		const auto a      = core::step_remote_chain(cyclic, "/a", std::vector<core::git_remote>{ { .Name = "origin", .Url = "file:///b" } });
		expect(std::holds_alternative<core::remote_chain_follow>(a) && std::get<core::remote_chain_follow>(a).Path == "/b", "a file URL is a local remote");
		cyclic.Hops.push_back({ .Remote = "origin", .Location = "/b" });
		(void)core::step_remote_chain(cyclic, "/b", std::vector<core::git_remote>{ { .Name = "origin", .Url = "/a" } });
		const auto back = core::step_remote_chain(cyclic, "/a", std::vector<core::git_remote>{ });
		expect(std::holds_alternative<core::remote_chain_refused>(back) && std::get<core::remote_chain_refused>(back).Reason == core::remote_chain_refusal::cycle, "a cycle is refused");

		auto deep = core::remote_chain_walk{ };
		auto step = core::remote_chain_step{ };
		for (auto hop = 0; hop <= 5; ++hop) {
			const auto location = "/c" + std::to_string(hop);
			step                = core::step_remote_chain(deep, location, std::vector<core::git_remote>{ { .Name = "origin", .Url = "/c" + std::to_string(hop + 1) } });
			if (!std::holds_alternative<core::remote_chain_follow>(step)) {
				break;
			}
			deep.Hops.push_back({ .Remote = "origin", .Location = "/c" + std::to_string(hop + 1) });
		}
		expect(std::holds_alternative<core::remote_chain_refused>(step) && std::get<core::remote_chain_refused>(step).Reason == core::remote_chain_refusal::too_many_hops
				   && deep.Hops.size() == core::maximum_local_remote_hops,
			   "a fifth hop is refused");

		auto none = core::remote_chain_walk{ };
		expect(std::get<core::remote_chain_refused>(core::step_remote_chain(none, "/x", std::nullopt)).Reason == core::remote_chain_refusal::unreadable, "an unreadable clone");
		auto bare = core::remote_chain_walk{ };
		expect(std::get<core::remote_chain_refused>(core::step_remote_chain(bare, "/y", std::vector<core::git_remote>{ })).Reason == core::remote_chain_refusal::no_hosted_remote,
			   "a clone naming nothing");
		auto two = core::remote_chain_walk{ };
		expect(std::get<core::remote_chain_refused>(
				   core::step_remote_chain(two, "/z", std::vector<core::git_remote>{ { .Name = "a", .Url = "/p" }, { .Name = "b", .Url = "../q" } }))
					   .Reason
				   == core::remote_chain_refusal::several_local_remotes,
			   "two local remotes are not guessed between");
		expect(!core::local_remote_path("git@github.com:acme/engine.git") && !core::local_remote_path("host:path") && core::local_remote_path("../primary"),
			   "hosted and scp-like URLs are not local paths");
	}

	void test_target_choice()
	{
		const auto origin   = core::destination_facts{ .Destination = { .Remote = "origin", .Repository = github("me", "engine") }, .DefaultBranch = "main" };
		const auto upstream = core::destination_facts{ .Destination = { .Remote = "upstream", .Repository = github("acme", "engine") }, .DefaultBranch = "trunk" };
		const auto one      = std::vector{ origin };
		const auto chosen   = core::choose_target(one, std::nullopt);
		expect(chosen && chosen->Target.Branch == "main" && chosen->Origin == core::target_origin::remote_default, "one repository's default branch");

		const auto fork      = std::vector{ origin, upstream };
		const auto ambiguous = core::choose_target(fork, std::nullopt);
		expect(!ambiguous && ambiguous.error().Question == core::target_question::several_repositories && ambiguous.error().Choices.size() == 2U,
			   "a fork and its parent: origin is not assumed");
		const auto explicitChoice = core::choose_target_explicitly(fork, { .Remote = "upstream", .Repository = github("acme", "engine"), .Branch = "trunk" });
		expect(explicitChoice && explicitChoice->Origin == core::target_origin::chosen, "the author chooses the parent");
		expect(core::choose_target(fork, explicitChoice).value() == *explicitChoice, "a kept choice stays");
		expect(!core::choose_target_explicitly(fork, { .Remote = "elsewhere", .Repository = github("x", "y"), .Branch = "main" }), "an unknown repository is refused");
		expect(core::choose_target(one, explicitChoice).error().Question == core::target_question::chosen_repository_missing, "a chosen repository that left is asked again");

		const auto noHead = std::vector{ core::destination_facts{ .Destination = origin.Destination, .Branches = { "main", "develop" } } };
		expect(core::choose_target(noHead, std::nullopt).error().Question == core::target_question::no_default_branch, "main is not assumed");
	}

	void test_comparison()
	{
		expect(core::decide_comparison(std::vector<core::git_commit_oid>{ }).State == core::comparison_state::no_common_base, "no merge base");
		const auto one = core::decide_comparison(std::vector{ commit('a'), commit('a') });
		expect(one.State == core::comparison_state::ready && one.Base == commit('a'), "one merge base");
		const auto two = core::decide_comparison(std::vector{ commit('b'), commit('a') });
		expect(two.State == core::comparison_state::several_bases && two.Bases.size() == 2U && !two.Base, "several bases are exposed, none chosen");

		const auto base    = std::vector<core::git_tree_entry>{ { .Path = "a.txt", .Mode = 0100644, .Object = entry_oid('1') },
																{ .Path = "b.txt", .Mode = 0100644, .Object = entry_oid('2') },
																{ .Path = "run.sh", .Mode = 0100644, .Object = entry_oid('3') } };
		const auto head    = std::vector<core::git_tree_entry>{ { .Path = "a.txt", .Mode = 0100644, .Object = entry_oid('4') },
																{ .Path = "c.txt", .Mode = 0100644, .Object = entry_oid('5') },
																{ .Path = "run.sh", .Mode = 0100755, .Object = entry_oid('3') } };
		const auto changes = core::compare_trees(base, head);
		expect(changes.size() == 4U && changes[0].Change == core::draft_change::modified && changes[1].Change == core::draft_change::deleted
				   && changes[2].Change == core::draft_change::added && changes[3].Change == core::draft_change::mode,
			   "the committed change, path by path");

		const auto working = std::vector<core::working_file>{
			{ .Path = "a.txt", .Edit = core::working_edit::modified, .Mode = 0100644, .Object = entry_oid('6'), .Size = 3U },
			{ .Path = "notes.md", .Edit = core::working_edit::untracked, .Mode = 0100644, .Object = entry_oid('7'), .Size = 4U },
			{ .Path = "big.bin", .Edit = core::working_edit::untracked, .Mode = 0100644, .Qualification = core::working_qualification::too_large },
		};
		const auto branchOnly = core::compose_candidate_changes(changes, base, working, std::vector<std::string>{ });
		expect(branchOnly && *branchOnly == changes, "nothing unselected enters");
		const auto selected = core::compose_candidate_changes(changes, base, working, std::vector<std::string>{ "notes.md" });
		expect(selected && selected->size() == 5U && (*selected)[3].Path == "notes.md" && (*selected)[3].Right->Origin == core::draft_material_origin::working,
			   "an untracked file enters only when selected");
		const auto edited = core::compose_candidate_changes(changes, base, working, std::vector<std::string>{ "a.txt" });
		expect(edited && (*edited)[0].Right->Object == entry_oid('6') && (*edited)[0].Left->Object == entry_oid('1'), "a selected edit replaces the head side; the base stays left");
		expect(core::compose_candidate_changes(changes, base, working, std::vector<std::string>{ "big.bin" }).error() == core::draft_candidate_refusal::qualified_working_file,
			   "a qualified file cannot be captured exactly");
		expect(core::compose_candidate_changes(changes, base, working, std::vector<std::string>{ "missing" }).error() == core::draft_candidate_refusal::unknown_working_file,
			   "only observed working files");
	}

	auto first_candidate() -> core::local_draft_candidate
	{
		return { .Head = commit('a'), .TargetTip = commit('b'), .Base = commit('c'), .Changes = { { .Path = "a.txt", .Change = core::draft_change::added, .Right = core::draft_side{ .Mode = 0100644, .Object = entry_oid('1') } } } };
	}

	auto make_draft(std::vector<core::local_draft>& aDrafts, const char aDigit, std::string aBranch, const std::int64_t aNow) -> core::local_draft
	{
		auto created = core::create_local_draft(
			aDrafts, { std::string(32U, aDigit) }, { .Location = "/work/feature", .Repository = { .Volume = 1, .ObjectLow = 10 }, .Worktree = { .Volume = 1, .ObjectLow = 11 } },
			std::move(aBranch), { .Target = { .Remote = "origin", .Repository = github("acme", "engine"), .Branch = "main" } }, { { .Remote = "origin", .Location = "/src/primary" } },
			first_candidate(), aNow);
		expect(created.has_value(), "a draft is created");
		return *created;
	}

	void test_resolution_and_transitions()
	{
		auto drafts = std::vector<core::local_draft>{ };
		drafts.push_back(make_draft(drafts, '1', "feature/search-results", 100));
		expect(drafts[0].Title == "Search results", "the title starts from the branch");

		auto observed = core::workspace_root_observation{ .Kind = core::workspace_root_kind::git_checkout, .Location = "/work/feature", .Root = { .Volume = 1, .ObjectLow = 9 },
														  .Repository = core::workspace_object_identity{ .Volume = 1, .ObjectLow = 10 },
														  .Worktree   = core::workspace_object_identity{ .Volume = 1, .ObjectLow = 11 } };
		expect(core::resolve_worktree(drafts, observed, "feature/search-results").Resume == 0U, "the same worktree and branch resume");
		const auto switched = core::resolve_worktree(drafts, observed, "other");
		expect(!switched.Resume && switched.OtherBranches.size() == 1U, "a branch switch does not retarget the draft");
		auto moved     = observed;
		moved.Location = "/elsewhere/feature";
		const auto mv  = core::resolve_worktree(drafts, moved, "feature/search-results");
		expect(!mv.Resume && mv.Moved == 0U, "a moved folder needs an explicit rebind");
		auto reused       = observed;
		reused.Repository = core::workspace_object_identity{ .Volume = 1, .ObjectLow = 99 };
		const auto reuse  = core::resolve_worktree(drafts, reused, "feature/search-results");
		expect(!reuse.Resume && !reuse.Moved && reuse.OtherBranches.empty(), "another repository at the same path is not this draft");

		const auto version = drafts[0].Version;
		auto       noted   = core::add_note(drafts[0], version, "Check the cancellation path.", "a.txt", core::note_origin::author, 110);
		expect(noted && noted->Notes.size() == 1U && noted->Version == version + 1U, "a note");
		expect(core::add_note(drafts[0], version + 5U, "late", "", core::note_origin::author, 111).error() == core::draft_refusal::stale, "a competing write is stale");
		auto next = first_candidate();
		next.Head = commit('d');
		auto two  = core::add_candidate(*noted, noted->Version, next, 120);
		expect(two && two->Candidates.size() == 2U && two->Candidates[1].Ordinal == 2U && two->Notes.size() == 1U, "a successor keeps the earlier candidate and notes");
		expect(core::add_candidate(*two, two->Version, next, 121).error() == core::draft_refusal::unchanged, "an identical capture records nothing");

		auto full = *two;
		for (auto ordinal = full.Candidates.size(); ordinal < core::maximum_local_draft_candidates; ++ordinal) {
			auto candidate = first_candidate();
			candidate.Head = commit(static_cast<char>('a' + (ordinal % 6U)));
			candidate.Changes[0].Path = "f" + std::to_string(ordinal);
			auto grown = core::add_candidate(full, full.Version, candidate, 130);
			expect(grown.has_value(), "candidates up to the bound");
			full = *grown;
		}
		auto another = first_candidate();
		another.Changes[0].Path = "z";
		expect(core::add_candidate(full, full.Version, another, 140).error() == core::draft_refusal::candidates_full && full.Candidates.front().Ordinal == 1U,
			   "a full draft refuses; nothing is evicted");

		auto many = std::vector<core::local_draft>(core::maximum_local_drafts, drafts[0]);
		expect(core::create_local_draft(many, { std::string(32U, 'f') }, drafts[0].Worktree, "x", drafts[0].Target, { }, first_candidate(), 1).error() == core::draft_refusal::drafts_full,
			   "a full store refuses a new draft");

		auto associated = core::associate_draft(*two, two->Version, { .Host = "github.com", .Owner = "Acme", .Name = "engine", .Number = 7, .HeadRef = "feature/search-results" }, "main", 150);
		expect(associated.has_value(), "the matching pull request associates");
		expect(!core::associate_draft(*two, two->Version, { .Host = "github.com", .Owner = "acme", .Name = "engine", .Number = 8, .HeadRef = "other" }, "main", 150),
			   "a pull request of another branch does not");
		auto both = std::vector{ *associated, make_draft(drafts, '2', "fix/it", 200) };
		expect(core::drafts_in_preparation(both) == std::vector<std::size_t>{ 1U }, "In preparation lists only drafts with no pull request");
		expect(core::draft_for_pull_request(both, "github.com", "acme", "engine", 7) == 0U, "the pull request's row finds its preparation");
	}

	void test_codec()
	{
		auto drafts = std::vector<core::local_draft>{ };
		auto draft  = make_draft(drafts, '3', "topic", 5);
		draft       = *core::add_note(draft, draft.Version, "line one\nline two", "", core::note_origin::harness, 6);
		const auto text    = core::encode_local_draft(draft);
		const auto decoded = core::decode_local_draft(text);
		expect(decoded && *decoded == draft, "the stored form round-trips");
		expect(!core::decode_local_draft(std::string{ text }.replace(text.find("bpr.local-draft"), 15U, "bpr.other-thing")), "another format is refused");
		expect(!core::decode_local_draft("{}"), "an empty document is refused");
	}
}

auto main() -> int
{
	test_remote_chain();
	test_target_choice();
	test_comparison();
	test_resolution_and_transitions();
	test_codec();
	std::cout << "local-draft:pass\n";
	return EXIT_SUCCESS;
}
