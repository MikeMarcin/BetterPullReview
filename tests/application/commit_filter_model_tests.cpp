// RFC-BPR-0089 M1: the commit filter model joins the snapshot, the comparison,
// the chain's tree diffs and the reviewer's filter.
#include "bpr/application/commit_filter_model.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
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

	auto hex(const char aDigit) -> std::string
	{
		return std::string(40U, aDigit);
	}

	auto oid(const char aDigit) -> core::git_commit_oid
	{
		return *core::parse_git_commit_oid(hex(aDigit));
	}

	auto commit(const char aDigit, std::vector<char> aParents, std::string aHeadline) -> application::github_commit_record
	{
		auto record          = application::github_commit_record{ };
		record.Oid           = application::commit_oid{ hex(aDigit) };
		record.Node          = application::github_node_id{ std::string{ "C_" } + aDigit };
		record.Headline      = std::move(aHeadline);
		record.Author        = application::github_actor_record{ };
		record.Author->Login = "octo-author";
		record.Parents.emplace();
		for (const auto parent : aParents) {
			record.Parents->emplace_back(hex(parent));
		}
		return record;
	}

	auto file(const std::string_view aPath, const application::github_file_change aChange) -> application::github_file_record
	{
		auto record        = application::github_file_record{ };
		record.Path        = std::string{ aPath };
		record.Change      = aChange;
		record.Additions   = 3U;
		record.ContentKind = application::github_file_content_kind::text;
		return record;
	}

	// The owner's shape: base b, C1 source (1), C2 regeneration (2), C3 follow-ups (3).
	auto snapshot() -> application::github_pull_request_snapshot
	{
		auto value                            = application::github_pull_request_snapshot{ };
		value.PullRequest.HeadOid             = application::commit_oid{ hex('3') };
		value.PullRequest.ReportedCommitCount = 3U;
		value.Commits                         = { commit('1', { 'b' }, "Change the generator input"), commit('2', { '1' }, "Regenerate"),
												  commit('3', { '2' }, "Follow-ups") };
		value.Files                           = { file("gen/x.cpp", application::github_file_change::added), file("gen/y.cpp", application::github_file_change::added),
												  file("src/a.cpp", application::github_file_change::modified), file("src/both.cpp", application::github_file_change::modified),
												  file("src/new.cpp", application::github_file_change::added) };
		return value;
	}

	auto touched(std::string aPath, const char aChange) -> application::commit_filter_touched_path
	{
		return { .Path = std::move(aPath), .PreviousPath = std::nullopt, .Change = aChange };
	}

	auto touches(const application::commit_filter_model& aModel, const application::commit_filter_touches_status aStatus)
		-> application::commit_filter_touches_observation
	{
		auto       observation = application::commit_filter_touches_observation{ };
		const auto steps       = aModel.touches_steps();
		expect(steps.has_value() && aModel.head().has_value(), "the test asks for steps before the read begins");
		observation.Request = { .WorkbenchAuthority = { }, .Head = *aModel.head(), .Steps = *steps };
		observation.Status  = aStatus;
		if (aStatus == application::commit_filter_touches_status::resolved) {
			observation.Touches = {
				{ .Commit = oid('1'), .Paths = { touched("src/a.cpp", 'M'), touched("src/both.cpp", 'M'), touched("src/new.cpp", 'A') } },
				{ .Commit = oid('2'), .Paths = { touched("gen/x.cpp", 'A'), touched("gen/y.cpp", 'A'), touched("src/both.cpp", 'M') } },
				{ .Commit = oid('3'), .Paths = { touched("gen/x.cpp", 'M'), touched("src/a.cpp", 'M'), touched("src/both.cpp", 'M') } },
			};
		}
		return observation;
	}

	auto ready() -> application::commit_filter_model
	{
		auto model = application::commit_filter_model{ };
		model.adopt(snapshot());
		model.adopt_comparison(core::filter_comparison{ });
		const auto answer = touches(model, application::commit_filter_touches_status::resolved);
		model.begin_touches();
		model.adopt_touches(answer);
		return model;
	}

	auto gesture(const core::commit_filter_operation_kind aKind, std::vector<char> aCommits) -> application::commit_filter_gesture
	{
		auto value = application::commit_filter_gesture{ .Kind = aKind, .Commits = { } };
		for (const auto digit : aCommits) {
			value.Commits.push_back(hex(digit));
		}
		return value;
	}

	auto paths(const application::commit_filter_files& aFiles) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (const auto& entry : aFiles.Files) {
			result.push_back(entry.Record.Path);
		}
		return result;
	}

	void test_steps_and_rows()
	{
		auto model = application::commit_filter_model{ };
		model.adopt(snapshot());
		const auto steps = model.touches_steps();
		expect(steps && steps->size() == 3U && (*steps)[0].Parent == oid('b') && (*steps)[0].Commit == oid('1') && (*steps)[2].Parent == oid('2'),
			   "the chain's tree diffs are asked for each commit against its first parent");
		auto projection = model.projection();
		expect(projection.Rows.size() == 3U && projection.Rows[0].ShortCommit == "1111111" && projection.Rows[0].Headline == "Change the generator input"
				   && projection.Rows[0].Author == "octo-author" && !projection.Rows[0].Files && projection.Trees == application::commit_filter_trees::idle,
			   "the Commits tab lists the commits before the trees are read");
		const auto answer = touches(model, application::commit_filter_touches_status::resolved);
		model.begin_touches();
		expect(!model.touches_steps(), "the trees are asked for once");
		model.adopt_touches(answer);
		projection = model.projection();
		expect(projection.Trees == application::commit_filter_trees::ready && projection.Rows[1].Files == std::optional<std::size_t>{ 3U },
			   "each row counts the files its commit changes once the trees are read");
	}

	void test_owner_filters()
	{
		using kind = core::commit_filter_operation_kind;
		auto model = ready();
		expect(!model.filtering() && !model.files(), "no filter, no filtered set: the review is the comparison");

		// AC02: Hide C2.
		expect(model.apply(gesture(kind::hide, { '2' })) == core::commit_filter_refusal::none, "Hide selected C2");
		auto set = model.files();
		expect(model.filtering() && set && paths(*set) == std::vector<std::string>{ "gen/x.cpp", "src/a.cpp", "src/both.cpp", "src/new.cpp" },
			   "hiding C2 drops gen/y.cpp and keeps the comparison's order");
		expect(!set->Files[1].Filter && set->Files[1].Record.Additions == 3U && !set->Files[3].Filter,
			   "files C2 never touched keep the comparison's own record and diff");
		expect(set->Files[0].Filter == std::optional{ core::filtered_diff_endpoints{ .Left = oid('2'), .Right = oid('3') } }
				   && set->Files[0].Record.Change == application::github_file_change::modified && set->Files[0].Record.Additions == 0U,
			   "gen/x.cpp shows only C3's change, between C2 and C3, with its counts unknown");
		expect(set->Files[2].Split && set->Files[2].SplitHidden == std::vector{ oid('2') } && set->SplitFiles == 1U,
			   "src/both.cpp is split by hidden C2");
		auto projection = model.projection();
		expect(projection.Status == core::commit_filter_status::active && projection.Shown == 2U && projection.Spanned == 3U
				   && projection.FilteredFiles == std::optional<std::size_t>{ 4U } && projection.ComparisonFiles == 5U
				   && application::commit_filter_chip_shown(projection),
			   "the chip reads 2 of 3 commits and 4 of 5 files");

		// AC01: Show only C1, then Clear all filters.
		expect(model.apply(gesture(kind::show_only, { '1' })) == core::commit_filter_refusal::none, "Show only selected C1");
		set = model.files();
		expect(set && paths(*set) == std::vector<std::string>{ "src/a.cpp", "src/both.cpp", "src/new.cpp" }, "Show only C1 lists exactly C1's paths");
		expect(set->Files[0].Filter == std::optional{ core::filtered_diff_endpoints{ .Left = oid('b'), .Right = oid('1') } }
				   && set->Files[2].Record.Change == application::github_file_change::added && !set->Files[2].Filter,
			   "C1's diffs are C1^..C1; a file only C1 changed keeps its own record");
		expect(model.apply(gesture(kind::clear, { })) == core::commit_filter_refusal::none && !model.files() && model.state().Hidden.empty(),
			   "Clear all filters restores the comparison exactly");

		// Show only C2: a file C2 added and C3 changed shows as added at C2.
		expect(model.apply(gesture(kind::show_only, { '2' })) == core::commit_filter_refusal::none, "Show only selected C2");
		set = model.files();
		expect(set && set->Files[0].Record.Path == "gen/x.cpp" && set->Files[0].Record.Change == application::github_file_change::added
				   && set->Files[0].Filter == std::optional{ core::filtered_diff_endpoints{ .Left = oid('1'), .Right = oid('2') } },
			   "a file the shown commit added is added between its parent and it");

		expect(model.apply(gesture(kind::show, { '2' })) == core::commit_filter_refusal::nothing_hidden && model.projection().Refusal,
			   "a refused gesture is named until the next");
		expect(model.apply(gesture(kind::show, { '1' })) == core::commit_filter_refusal::none && !model.projection().Refusal, "the next gesture clears it");
		expect(model.apply({ .Kind = kind::hide, .Commits = { "not-a-commit" } }) == core::commit_filter_refusal::unknown_commit, "a malformed ID is refused");
	}

	void test_unavailable_and_suspended()
	{
		using kind                                = core::commit_filter_operation_kind;
		auto truncated                            = snapshot();
		truncated.PullRequest.ReportedCommitCount = 300U;
		auto model                                = application::commit_filter_model{ };
		model.adopt(truncated);
		model.adopt_comparison(core::filter_comparison{ });
		expect(model.projection().Unavailable == core::commit_filter_unavailable::truncated && model.projection().Rows.empty(),
			   "a list truncated at 250 says why it has no filter");
		expect(model.apply(gesture(kind::hide, { '2' })) == core::commit_filter_refusal::unavailable && !model.touches_steps(),
			   "and offers no gesture or tree read");
		expect(model.apply(gesture(kind::clear, { })) == core::commit_filter_refusal::none, "Clear all filters is always accepted");

		auto unknown = snapshot();
		unknown.Commits[1].Parents.reset();
		model.adopt(unknown);
		expect(model.projection().Unavailable == core::commit_filter_unavailable::parents_unknown, "unknown parents are said, not guessed");

		auto reversed = ready();
		expect(reversed.apply(gesture(kind::hide, { '2' })) == core::commit_filter_refusal::none, "hide C2 under Whole PR");
		reversed.adopt_comparison(core::filter_comparison{ .WholePullRequest = false, .Left = oid('3'), .Right = oid('1') });
		const auto projection = reversed.projection();
		expect(projection.Status == core::commit_filter_status::suspended && projection.Suspension == core::commit_filter_suspension::reversed
				   && !reversed.files() && projection.Rows[1].State == core::filter_commit_state::hidden,
			   "a reversed comparison suspends the filter, says why, and still shows what it hides");
		expect(reversed.apply(gesture(kind::hide, { '1' })) == core::commit_filter_refusal::suspended, "no gesture under a suspended filter");

		auto failed = application::commit_filter_model{ };
		failed.adopt(snapshot());
		const auto missing = touches(failed, application::commit_filter_touches_status::objects_unavailable);
		failed.begin_touches();
		failed.adopt_touches(missing);
		expect(failed.projection().Trees == application::commit_filter_trees::objects_unavailable, "missing commit trees are said");
		expect(!failed.restore(core::commit_filter_state{ .Hidden = { { .Oid = oid('3'), .Headline = "" }, { .Oid = oid('1'), .Headline = "" } } }),
			   "a malformed persisted filter is refused");
	}

	void test_force_push()
	{
		auto model = ready();
		expect(model.apply(gesture(core::commit_filter_operation_kind::hide, { '3' })) == core::commit_filter_refusal::none, "hide C3");
		auto pushed                = snapshot();
		pushed.PullRequest.HeadOid = application::commit_oid{ hex('4') };
		pushed.Commits.back()      = commit('4', { '2' }, "Follow-ups, amended");
		model.adopt(pushed);
		const auto projection = model.projection();
		expect(projection.Gone.size() == 1U && projection.Gone[0].ShortCommit == "3333333" && projection.Gone[0].Headline == "Follow-ups"
				   && projection.Status == core::commit_filter_status::inactive && application::commit_filter_chip_shown(projection),
			   "a force push that rewrites hidden C3 names it, and it stops applying");
		expect(projection.Trees == application::commit_filter_trees::idle && model.touches_steps() && (*model.touches_steps())[2].Commit == oid('4'),
			   "the new head's trees are asked for again");
		auto original = application::commit_filter_model{ };
		original.adopt(snapshot());
		const auto stale = touches(original, application::commit_filter_touches_status::resolved);
		model.begin_touches();
		model.adopt_touches(stale);
		expect(model.projection().Trees == application::commit_filter_trees::loading, "an answer for the old head is ignored");
	}
} // namespace

auto main() -> int
{
	test_steps_and_rows();
	test_owner_filters();
	test_unavailable_and_suspended();
	test_force_push();
	std::cout << "commit-filter-model-tests:pass\n";
	return 0;
}
