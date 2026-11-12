/// \file
/// \brief RFC-BPR-0113 and RFC-BPR-0114 as amended 2026-11-12 ([[BPR-2062]],
/// [[BPR-2065]]). Author state's Local Changes without Git: the changes of a
/// file and the exact index text Stage Change and Unstage Change write
/// (line endings, a missing final newline, a stale change, an inexact one),
/// the commit and push decisions, the push command, Edit's refusals and
/// Save's decision, gutter marks, and the agent's commit-message request and
/// answer.

#include "bpr/core/local_changes.hpp"
#include "product_diff.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

	auto bytes(const std::string_view aText) -> std::vector<std::uint8_t> { return { aText.begin(), aText.end() }; }
	auto text(const std::vector<std::uint8_t>& aBytes) -> std::string { return { aBytes.begin(), aBytes.end() }; }

	auto commit(const char aDigit) -> core::git_commit_oid { return *core::parse_git_commit_oid(std::string(40U, aDigit)); }

	auto compare(const std::string_view aLeft, const std::string_view aRight) -> core::computed_material_diff
	{
		const auto left    = bytes(aLeft);
		const auto right   = bytes(aRight);
		const auto options = core::diff_options{ };
		const auto key     = core::local_change_comparison(core::local_comparison::uncommitted, "a.txt", std::span<const std::uint8_t>{ left }, std::span<const std::uint8_t>{ right }, options);
		expect(key.has_value(), "a comparison key");
		auto outcome = tests::product_material_diff({ .Key = *key, .Options = options, .LeftBytes = std::span<const std::uint8_t>{ left }, .RightBytes = std::span<const std::uint8_t>{ right }, .Limits = { } });
		expect(outcome.Result.has_value(), "the comparison computes");
		return std::move(*outcome.Result);
	}

	void stage_and_unstage_one_change()
	{
		const auto index    = std::string_view{ "one\ntwo\nthree\nfour\nfive\nsix\nseven\n" };
		const auto worktree = std::string_view{ "one\nTWO\nthree\nfour\nfive\nsix\nseven\neight\n" };
		const auto diff     = compare(index, worktree);
		const auto hunks    = core::change_hunks(diff);
		expect(hunks.size() == 2U, "two changes");
		expect(hunks[0] == core::local_change_hunk{ 1U, 2U, 1U, 2U }, "the first change replaces line 2");
		expect(hunks[1] == core::local_change_hunk{ 7U, 7U, 7U, 8U }, "the second adds a last line");

		const auto first = core::apply_change(diff, hunks[0]);
		expect(first && text(*first) == "one\nTWO\nthree\nfour\nfive\nsix\nseven\n", "staging the first change takes only it");
		const auto second = core::apply_change(diff, hunks[1]);
		expect(second && text(*second) == "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\n", "staging the second takes only it");

		// Unstage: the index (right) loses one change back to the last commit (left).
		const auto staged = compare(index, text(*first) + "eight\n");
		const auto again  = core::change_hunks(staged);
		expect(again.size() == 2U, "two staged changes");
		const auto unstaged = core::revert_change(staged, again[1]);
		expect(unstaged && text(*unstaged) == text(*first), "unstaging returns exactly one change");

		const auto counts = core::line_counts(diff);
		expect(counts.Added == 2U && counts.Removed == 1U, "line counts");

		const auto stale = core::apply_change(diff, core::local_change_hunk{ 3U, 4U, 3U, 4U });
		expect(!stale && stale.error() == core::local_stage_refusal::stale, "a change that is not one of the file's is refused as stale");
	}

	void bands_say_what_each_change_offers()
	{
		const auto head     = std::string_view{ "a\nb\nc\nd\ne\nf\ng\nh\n" };
		const auto index    = std::string_view{ "a\nB\nc\nd\ne\nf\ng\nh\n" };
		const auto worktree = std::string_view{ "a\nB\nc\nd\ne\nf\nG\nh\n" };
		const auto shown    = core::change_hunks(compare(head, worktree));
		const auto staged   = core::change_hunks(compare(head, index));
		const auto unstaged = core::change_hunks(compare(index, worktree));
		const auto bands    = core::uncommitted_bands(shown, staged, unstaged);
		expect(bands.size() == 2U, "two bands");
		expect(bands[0].Staged && !bands[0].Unstaged && bands[0].Unstage && !bands[0].Stage, "the first change is staged: Unstage Change");
		expect(!bands[1].Staged && bands[1].Unstaged && bands[1].Stage && !bands[1].Unstage, "the second is not: Stage Change");
		const auto stagedSecond = core::apply_change(compare(index, worktree), *bands[1].Stage);
		expect(stagedSecond && text(*stagedSecond) == worktree, "Stage Change writes the index the worktree has");
		const auto unstagedFirst = core::revert_change(compare(head, index), *bands[0].Unstage);
		expect(unstagedFirst && text(*unstagedFirst) == head, "Unstage Change returns the index to the last commit");
	}

	void line_endings_survive()
	{
		const auto index    = std::string_view{ "a\r\nb\r\nc" };
		const auto worktree = std::string_view{ "a\r\nB\r\nc" };
		const auto diff     = compare(index, worktree);
		const auto hunks    = core::change_hunks(diff);
		expect(hunks.size() == 1U, "one change");
		const auto staged = core::apply_change(diff, hunks[0]);
		expect(staged && text(*staged) == "a\r\nB\r\nc", "CRLF and the missing final newline survive staging");

		// An equal line whose terminator differs cannot be split by change.
		const auto ending = compare("a\nb", "a\nb\nc\n");
		const auto pieces = core::change_hunks(ending);
		if (!pieces.empty()) {
			const auto refused = core::apply_change(ending, pieces[0]);
			expect(!refused || text(*refused) == "a\nb\nc\n", "a change is applied exactly or refused");
			if (!refused) {
				expect(refused.error() == core::local_stage_refusal::inexact, "the refusal says only the whole file can be staged");
			}
		}
	}

	auto status(std::vector<core::local_status_entry> aStaged, std::vector<core::local_status_entry> aUnstaged) -> core::local_worktree_status
	{
		return { .Branch = "fix/port-parsing", .Head = commit('a'), .Staged = std::move(aStaged), .Unstaged = std::move(aUnstaged) };
	}

	void commit_decisions()
	{
		const auto staged = core::local_status_entry{ .Path = "src/a.cpp", .Status = core::local_file_status::modified };
		const auto clean  = status({ }, { });
		const auto one    = status({ staged }, { });

		expect(core::commit_message("  Reject bad ports \n", "\nThe port is parsed strictly.\n\n") == "Reject bad ports\n\nThe port is parsed strictly.\n", "the message is trimmed and ends in one newline");
		expect(core::commit_message("Subject", "") == "Subject\n", "a subject alone");
		expect(core::commit_message("Subject\nmore", "Body") == "Subject\n\nmore\n\nBody\n", "a second subject line moves to the description");

		const auto plan = core::decide_commit(one, { .Subject = "Test leading zeros", .Description = "" }, false);
		expect(plan && plan->Files == 1U && plan->Message == "Test leading zeros\n" && !plan->Amend, "Commit 1 File");
		expect(core::decide_commit(clean, { .Subject = "x" }, false).error() == core::local_commit_refusal::nothing_staged, "nothing staged");
		expect(core::decide_commit(one, { .Subject = "  " }, false).error() == core::local_commit_refusal::no_subject, "no subject");
		auto detached   = one;
		detached.Branch = { };
		expect(core::decide_commit(detached, { .Subject = "x" }, false).error() == core::local_commit_refusal::detached, "detached");
		auto conflicted = one;
		conflicted.Unstaged.push_back({ .Path = "b", .Status = core::local_file_status::unmerged });
		expect(core::decide_commit(conflicted, { .Subject = "x" }, false).error() == core::local_commit_refusal::unmerged, "unmerged");

		// Amend: a reword needs nothing staged, and only while GitHub lacks the commit.
		expect(core::decide_commit(clean, { .Subject = "Reword", .Amend = true }, false).has_value(), "amend with nothing staged rewords");
		expect(core::decide_commit(clean, { .Subject = "Reword", .Amend = true }, true).error() == core::local_commit_refusal::amend_pushed, "amend refused once pushed");
		auto unborn = clean;
		unborn.Head.reset();
		expect(core::decide_commit(unborn, { .Subject = "x", .Amend = true }, false).error() == core::local_commit_refusal::unborn, "nothing to amend");
		expect(core::amend_offered(clean, false) && !core::amend_offered(clean, true) && !core::amend_offered(unborn, false), "amend offered");
	}

	void push_decisions()
	{
		const auto clean  = status({ }, { });
		const auto dirty  = status({ }, { { .Path = "a", .Status = core::local_file_status::modified } });
		auto       hosted = core::local_hosted_branch{ .Observed = true, .Tip = commit('b'), .TipKnown = true, .Ahead = 2U, .Behind = 0U };

		auto decision = core::decide_push(clean, hosted);
		expect(decision.State == core::local_push_state::ahead && decision.Offered && decision.Default, "2 ahead: Push, blue");
		expect(!core::decide_push(dirty, hosted).Default && core::decide_push(dirty, hosted).Offered, "uncommitted files: Push offered, not the next step");
		expect(!core::head_on_host(clean, hosted), "an unpushed head is not on GitHub");

		hosted.Ahead = 0U;
		expect(core::decide_push(clean, hosted).State == core::local_push_state::up_to_date && !core::decide_push(clean, hosted).Offered, "up to date");
		expect(core::head_on_host(clean, hosted), "an up-to-date head is on GitHub");

		hosted.Behind = 2U;
		expect(core::decide_push(clean, hosted).State == core::local_push_state::behind && !core::decide_push(clean, hosted).Offered, "behind: refused");
		hosted.Ahead = 1U;
		expect(core::decide_push(clean, hosted).State == core::local_push_state::diverged && !core::decide_push(clean, hosted).Offered, "diverged: refused");
		hosted.TipKnown = false;
		expect(core::decide_push(clean, hosted).State == core::local_push_state::unknown_tip && !core::decide_push(clean, hosted).Offered, "an unknown tip: refused");
		expect(core::head_on_host(clean, hosted), "with an unknown tip, amend is refused");

		hosted = { .Observed = true };
		expect(core::decide_push(clean, hosted).State == core::local_push_state::missing && core::decide_push(clean, hosted).Offered, "missing: Push creates it");
		expect(core::decide_push(clean, { }).State == core::local_push_state::unobserved && !core::decide_push(clean, { }).Offered, "unobserved");

		const auto command = core::make_push_command("/work/repo", "git@github.com:owner/repo.git", "fix/port-parsing");
		expect(command.has_value(), "a push command");
		expect(command->Arguments == std::vector<std::string>{ "push", "--no-follow-tags", "git@github.com:owner/repo.git", "refs/heads/fix/port-parsing:refs/heads/fix/port-parsing" },
			   "the push names the branch on both sides, never forced");
		expect(command->Text == "git push --no-follow-tags git@github.com:owner/repo.git refs/heads/fix/port-parsing:refs/heads/fix/port-parsing", "the shown command is the run one");
		for (const auto& argument : command->Arguments) {
			expect(argument.find("force") == std::string::npos && !argument.starts_with('+') && argument != "--tags" && argument != "--delete", "no force, delete or tags");
		}
		expect(!core::make_push_command("/w", "-oProxy", "b"), "an option for a URL is refused");
		expect(!core::make_push_command("/w", "https://x/y.git", "-b"), "an option for a branch is refused");
		expect(!core::make_push_command("/w", "https://x/y.git", "a..b"), "a malformed branch is refused");
		expect(!core::make_push_command("/w", "https://x/y.git", "a:b"), "a refspec for a branch is refused");
	}

	void edit_and_save()
	{
		const auto file = core::local_edit_facts{ .Exists = true, .Regular = true, .Inside = true, .Size = 4U };
		expect(!core::decide_editable(file, bytes("abc\n")), "a small text file is editable");
		auto facts   = file;
		facts.Inside = false;
		expect(core::decide_editable(facts, { }) == core::local_edit_refusal::outside, "outside");
		facts        = file;
		facts.Exists = false;
		expect(core::decide_editable(facts, { }) == core::local_edit_refusal::deleted, "deleted");
		facts      = file;
		facts.Link = true;
		expect(core::decide_editable(facts, { }) == core::local_edit_refusal::link, "link");
		facts           = file;
		facts.Submodule = true;
		expect(core::decide_editable(facts, { }) == core::local_edit_refusal::submodule, "submodule");
		facts         = file;
		facts.Regular = false;
		expect(core::decide_editable(facts, { }) == core::local_edit_refusal::not_a_file, "a directory");
		facts      = file;
		facts.Size = core::maximum_editable_bytes + 1U;
		expect(core::decide_editable(facts, { }) == core::local_edit_refusal::too_large, "too large");
		facts          = file;
		facts.Filtered = true;
		expect(core::decide_editable(facts, bytes("x")) == core::local_edit_refusal::filtered, "filtered");
		expect(core::decide_editable(file, std::vector<std::uint8_t>{ 'a', 0U }) == core::local_edit_refusal::nul_byte, "NUL");
		expect(core::decide_editable(file, std::vector<std::uint8_t>{ 0xC3U, 0x28U }) == core::local_edit_refusal::not_utf8, "not UTF-8");

		expect(core::decide_save(bytes("a"), bytes("a"), bytes("b")) == core::local_save_decision::write, "unchanged disk: write");
		expect(core::decide_save(bytes("a"), bytes("c"), bytes("b")) == core::local_save_decision::changed_on_disk, "changed on disk: nothing written");
		expect(core::decide_save(bytes("a"), bytes("a"), bytes("a")) == core::local_save_decision::unchanged, "nothing to save");

		const auto marks = core::line_marks(compare("a\nb\nc\nd\n", "a\nB\nc\nnew\nd\n"));
		expect(marks.size() == 2U && marks[0].Line == 1U && marks[0].Kind == core::local_line_mark_kind::changed && marks[1].Line == 3U && marks[1].Kind == core::local_line_mark_kind::added,
			   "a changed and an added line");
		const auto removed = core::line_marks(compare("a\nb\nc\n", "a\nc\n"));
		expect(removed.size() == 1U && removed[0].Kind == core::local_line_mark_kind::removed_before && removed[0].Line == 1U && removed[0].LeftBegin == 1U && removed[0].LeftEnd == 2U,
			   "a removal marks the line after it, with the old lines");
	}

	void commit_message_request()
	{
		auto request = core::commit_message_request{
			.Instructions   = "Write a commit message.",
			.Branch         = "fix/port-parsing",
			.StagedDiff     = "diff --git a/a b/a\n+x\n",
			.RecentSubjects = { "Parse ports strictly", "Add endpoint tests" },
		};
		const auto prompt = core::compose_commit_message_prompt(request);
		expect(prompt.starts_with("Write a commit message.\n\nBranch: fix/port-parsing\n"), "instructions and branch lead");
		expect(prompt.find("- Parse ports strictly\n- Add endpoint tests\n") != std::string::npos, "recent subjects for style");
		expect(prompt.find("```diff\ndiff --git a/a b/a\n+x\n```\n") != std::string::npos, "the staged diff");
		expect(prompt.find("not sent") == std::string::npos, "nothing cut");
		request.StagedDiff = std::string(core::maximum_commit_message_diff_bytes + 10U, 'x');
		expect(core::compose_commit_message_prompt(request).find("(10 more bytes of the staged change were not sent.)") != std::string::npos, "a long diff is cut, exactly said");
		expect(core::compose_commit_message_prompt(request) == core::compose_commit_message_prompt(request), "the request is deterministic");

		auto answer = core::read_commit_message_answer("Subject: Test leading zeros\n\nA port with leading zeros parses to its value.\nIt is still bounded.\n");
		expect(answer && answer->Subject == "Test leading zeros" && answer->Description == "A port with leading zeros parses to its value.\nIt is still bounded.", "subject and description");
		answer = core::read_commit_message_answer("Here you go:\n```\nReject out-of-range ports\n\nBody line.\n```\n");
		expect(answer && answer->Subject == "Reject out-of-range ports" && answer->Description == "Body line.", "a fenced answer is unwrapped");
		answer = core::read_commit_message_answer("Only a subject");
		expect(answer && answer->Subject == "Only a subject" && answer->Description.empty(), "a subject alone");
		expect(!core::read_commit_message_answer("  \n\n"), "an empty answer suggests nothing");
	}

	void keys_and_status()
	{
		const auto left  = bytes("a\n");
		const auto right = bytes("b\n");
		const auto key1  = core::local_change_comparison(core::local_comparison::uncommitted, "p", std::span<const std::uint8_t>{ left }, std::span<const std::uint8_t>{ right }, { });
		const auto key2  = core::local_change_comparison(core::local_comparison::uncommitted, "p", std::span<const std::uint8_t>{ left }, std::span<const std::uint8_t>{ right }, { });
		const auto key3  = core::local_change_comparison(core::local_comparison::not_pushed, "p", std::span<const std::uint8_t>{ left }, std::span<const std::uint8_t>{ right }, { });
		const auto added = core::local_change_comparison(core::local_comparison::uncommitted, "p", std::nullopt, std::span<const std::uint8_t>{ right }, { });
		expect(key1 && key1 == key2 && key1 != key3, "keys are by comparison, path and content");
		expect(added && !added->LeftPresent && added->RightPresent, "an added file has no left side");
		expect(!core::local_change_comparison(core::local_comparison::uncommitted, "p", std::nullopt, std::nullopt, { }), "no sides, no key");

		auto observed = status({ { .Path = "b" }, { .Path = "a" }, { .Path = "a" } }, { { .Path = "c", .Status = core::local_file_status::untracked }, { .Path = "a" } });
		observed      = core::normalized(observed);
		expect(observed.Staged.size() == 2U && observed.Staged[0].Path == "a", "sorted and deduplicated");
		expect(core::uncommitted_paths(observed) == std::vector<std::string>{ "a", "b", "c" }, "uncommitted paths");
		expect(core::badge(core::local_file_status::untracked) == 'A' && core::badge(core::local_file_status::deleted) == 'D', "badges");
	}
}

auto main() -> int
{
	stage_and_unstage_one_change();
	bands_say_what_each_change_offers();
	line_endings_survive();
	commit_decisions();
	push_decisions();
	edit_and_save();
	commit_message_request();
	keys_and_status();
	std::cout << "local_changes: all passed\n";
	return EXIT_SUCCESS;
}
