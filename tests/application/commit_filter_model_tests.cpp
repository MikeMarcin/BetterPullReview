// RFC-BPR-0089 M1: the commit filter model joins the snapshot, the comparison,
// the chain's tree diffs and the reviewer's filter. M3: a split file's
// per-commit diffs, and the rematch offers after a force push.
#include "bpr/application/commit_filter_model.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
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

	// RFC-BPR-0089 M2. Attribute answers for exact paths only; the pattern
	// matcher is not under test here.
	class exact_path_rules final : public core::generated_attribute_rules
	{
	public:
		explicit exact_path_rules(std::map<std::string, core::git_attribute_state, std::less<>> aStates)
			: States{ std::move(aStates) }
		{
		}

		[[nodiscard]] auto linguist_generated(const std::string_view aPath) const -> core::git_attribute_state override
		{
			const auto found = States.find(aPath);
			return found != States.end() ? found->second : core::git_attribute_state{ };
		}

	private:
		std::map<std::string, core::git_attribute_state, std::less<>> States;
	};

	auto generated_rules() -> std::shared_ptr<const core::generated_attribute_rules>
	{
		const auto set = core::git_attribute_state{ .Kind = core::git_attribute_state::kind::set, .Value = { } };
		return std::make_shared<const exact_path_rules>(std::map<std::string, core::git_attribute_state, std::less<>>{ { "gen/x.cpp", set }, { "gen/y.cpp", set } });
	}

	auto set_aside(const application::commit_filter_files& aFiles) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (const auto& entry : aFiles.SetAside) {
			result.push_back(entry.Record.Path);
		}
		return result;
	}

	auto adopt_after(application::commit_filter_model& aModel, const std::string& aPath, const core::generated_gesture aGesture) -> core::generated_file_mark
	{
		const auto mark = aModel.generated_mark_after({ .Path = aPath, .Gesture = aGesture });
		expect(mark.has_value(), "the gesture is offered for the file");
		// The composition persists the mark, then adopts what the ledger holds.
		auto next = core::apply_generated_gesture(aModel.generated_marks(), mark->File, aGesture);
		expect(next.has_value() && aModel.adopt_generated_marks(*next), "the persisted marks are adopted");
		return *mark;
	}

	auto gestures_of(const std::vector<application::generated_file_row>& aRows, const std::string_view aPath) -> std::vector<core::generated_gesture>
	{
		const auto found = std::ranges::find(aRows, aPath, &application::generated_file_row::Path);
		return found != aRows.end() ? found->Gestures : std::vector<core::generated_gesture>{ };
	}

	void test_generated_files_are_set_aside()
	{
		using kind = core::commit_filter_operation_kind;
		auto model = ready();
		expect(!model.files(), "without attributes or marks nothing is set aside");
		expect(model.projection().OrdinaryGestures == std::vector{ core::generated_gesture::treat_as_generated },
			   "an ordinary file offers Treat as generated");

		model.adopt_generated_attributes(generated_rules());
		auto set = model.files();
		expect(set && !set->CommitFiltered && paths(*set) == std::vector<std::string>{ "src/a.cpp", "src/both.cpp", "src/new.cpp" }
				   && set_aside(*set) == std::vector<std::string>{ "gen/x.cpp", "gen/y.cpp" },
			   "linguist-generated files leave the set, walk and counts in Whole PR");
		auto projection = model.projection();
		expect(projection.Generated.size() == 2U && projection.GeneratedAdditions == 6U && projection.AttributesRead
				   && projection.FilteredFiles == std::optional<std::size_t>{ 3U } && !projection.CommitFiltered
				   && !application::commit_filter_chip_shown(projection),
			   "the generated row counts them and their lines; set-aside files are not a filter");
		expect(projection.Generated[0].Reason == core::generated_reason::attribute_set
				   && projection.Generated[0].Gestures == std::vector{ core::generated_gesture::review_anyway, core::generated_gesture::show_as_normal },
			   "a set-aside file offers Review anyway and Show as normal");

		// Generated status holds under every commit filter, and Clear all filters keeps it (owner, Q3).
		expect(model.apply(gesture(kind::hide, { '3' })) == core::commit_filter_refusal::none, "Hide C3");
		set = model.files();
		expect(set && set->CommitFiltered && set_aside(*set) == std::vector<std::string>{ "gen/x.cpp", "gen/y.cpp" }
				   && std::ranges::none_of(paths(*set), [](const std::string& aPath) { return aPath == "gen/x.cpp"; }),
			   "under a commit filter generated files stay behind their row");
		expect(model.apply(gesture(kind::clear, { })) == core::commit_filter_refusal::none, "Clear all filters");
		set = model.files();
		expect(set && set_aside(*set) == std::vector<std::string>{ "gen/x.cpp", "gen/y.cpp" }, "Clear all filters leaves generated files behind their row");

		// Review anyway returns gen/x.cpp, still generated.
		const auto anyway = adopt_after(model, "gen/x.cpp", core::generated_gesture::review_anyway);
		expect(anyway.ReviewAnyway && anyway.Mark == core::generated_mark::none, "Review anyway is a mark of its own");
		set = model.files();
		expect(set && set_aside(*set) == std::vector<std::string>{ "gen/y.cpp" } && set->Files.front().Record.Path == "gen/x.cpp"
				   && set->Files.front().Generated.ReviewAnyway,
			   "a file reviewed anyway returns to the set and stays generated");
		projection = model.projection();
		expect(gestures_of(projection.Marked, "gen/x.cpp") == std::vector{ core::generated_gesture::set_aside_again, core::generated_gesture::show_as_normal },
			   "a file reviewed anyway offers Set aside again and Show as normal");

		// Show as normal on gen/y.cpp; Treat as generated on src/new.cpp.
		adopt_after(model, "gen/y.cpp", core::generated_gesture::show_as_normal);
		adopt_after(model, "src/new.cpp", core::generated_gesture::treat_as_generated);
		set = model.files();
		expect(set && set_aside(*set) == std::vector<std::string>{ "src/new.cpp" } && set->SetAside.front().Generated.Reason == core::generated_reason::marked_generated,
			   "Treat as generated sets a file aside and Show as normal returns one");
		projection = model.projection();
		expect(gestures_of(projection.Marked, "gen/y.cpp") == std::vector{ core::generated_gesture::treat_as_generated, core::generated_gesture::follow_attributes },
			   "a file shown as normal offers Treat as generated and Follow .gitattributes");

		expect(!model.generated_mark_after({ .Path = "not/listed.cpp", .Gesture = core::generated_gesture::treat_as_generated }),
			   "a file the review does not list is refused");
		expect(!model.generated_mark_after({ .Path = "src/a.cpp", .Gesture = core::generated_gesture::review_anyway }),
			   "a gesture the file does not offer is refused");
		expect(!model.adopt_generated_marks({ .Marks = { anyway, anyway } }), "malformed marks are refused");

		// Without the attributes only the marks decide.
		model.adopt_generated_attributes(nullptr);
		set = model.files();
		expect(set && set_aside(*set) == std::vector<std::string>{ "src/new.cpp" }, "without attributes only the reviewer's marks set files aside");
	}

	/// \brief [[BPR-1692]]. The head's `.gitattributes` are asked for once per
	/// head and directory set, read by the core, and say why when not read.
	void test_head_attributes_are_read()
	{
		using status      = application::generated_attributes_status;
		using read        = application::generated_attributes_read;
		auto       model  = ready();
		const auto wanted = model.generated_attributes_wanted();
		expect(wanted == std::optional{ std::vector<std::string>{ "", "gen", "src" } }, "the root and every listed path's ancestors");
		expect(model.projection().Attributes == read::pending && !model.files(), "nothing is set aside before they are read");
		expect(model.begin_generated_attributes(*wanted), "the read begins");
		expect(!model.generated_attributes_wanted(), "asked once while it is read");

		auto answer = application::generated_attributes_observation{
			.Request  = { .WorkbenchAuthority = { }, .Head = oid('3'), .Directories = *wanted },
			.Status   = status::resolved,
			.Files    = { { .Directory = "", .Contents = "gen/** linguist-generated\n" }, { .Directory = "gen", .Contents = "y.cpp -linguist-generated\n" } },
			.Exceeded = std::nullopt,
		};
		auto other         = answer;
		other.Request.Head = oid('2');
		model.adopt_generated_attributes(other);
		expect(model.projection().Attributes == read::pending, "an answer for another head is ignored");
		model.adopt_generated_attributes(answer);
		auto set = model.files();
		expect(model.projection().Attributes == read::read && model.projection().AttributesRead, "the head's attributes are read");
		expect(set && set_aside(*set) == std::vector<std::string>{ "gen/x.cpp" } && paths(*set) == std::vector<std::string>{ "gen/y.cpp", "src/a.cpp", "src/both.cpp", "src/new.cpp" },
			   "gen/** sets gen/x.cpp aside and the nested -linguist-generated returns gen/y.cpp");
		expect(!model.generated_attributes_wanted(), "not asked again under the same head");

		// A refused request is asked again; a bound says why and reads nothing.
		auto bounded = ready();
		expect(bounded.begin_generated_attributes(*wanted), "the read begins");
		bounded.abandon_generated_attributes();
		expect(bounded.generated_attributes_wanted() == wanted, "a request the host refused is asked again");
		expect(bounded.begin_generated_attributes(*wanted), "the read begins again");
		auto large     = answer;
		large.Status   = status::file_too_large;
		large.Files    = { };
		large.Exceeded = "gen";
		bounded.adopt_generated_attributes(large);
		auto projection = bounded.projection();
		expect(projection.Attributes == read::file_too_large && projection.AttributesExceeded == std::optional<std::string>{ "gen" } && !projection.AttributesRead
				   && !bounded.files(),
			   "a file past 512 KiB is named and nothing is set aside by the attributes");
		auto many  = ready();
		auto files = std::vector<core::generated_attribute_file>{ };
		for (std::size_t index = 0U; index <= core::maximum_generated_attribute_files; ++index) {
			files.push_back({ .Directory = "d" + std::to_string(100U + index), .Contents = { } });
		}
		expect(many.begin_generated_attributes(*wanted), "the read begins");
		auto counted  = answer;
		counted.Files = files;
		many.adopt_generated_attributes(counted);
		expect(many.projection().Attributes == read::too_many_files, "the core refuses a 65th file too");
		auto wide        = ready();
		auto directories = std::vector<std::string>(application::maximum_generated_attribute_directories + 1U);
		expect(!wide.begin_generated_attributes(std::move(directories)) && wide.projection().Attributes == read::too_many_directories,
			   "more directories than one read looks in are not asked for, and say so");

		// A new head forgets the old head's rules.
		auto pushed                = snapshot();
		pushed.PullRequest.HeadOid = application::commit_oid{ hex('4') };
		pushed.Commits.push_back(commit('4', { '3' }, "More"));
		pushed.PullRequest.ReportedCommitCount = 4U;
		model.adopt(pushed);
		expect(model.projection().Attributes == read::pending && !model.projection().AttributesRead && model.generated_attributes_wanted().has_value(),
			   "a new head asks again");
	}

	void test_hide_files_these_commits_changed()
	{
		using kind = core::commit_filter_operation_kind;
		auto model = ready();
		expect(model.hide_files_changed_by({ hex('2') }) == core::commit_filter_refusal::none, "Hide files these commits changed on C2");
		auto set = model.files();
		expect(set && !set->CommitFiltered && set->FileFiltered == 3U && paths(*set) == std::vector<std::string>{ "src/a.cpp", "src/new.cpp" },
			   "every path C2 changed is hidden, in C3 as well");
		auto projection = model.projection();
		expect(projection.FileFilterPaths == 3U && projection.FileFilteredFiles == 3U && application::commit_filter_chip_shown(projection),
			   "the chip shows while a file filter names paths");

		expect(model.apply(gesture(kind::show_only, { '3' })) == core::commit_filter_refusal::none, "Show only C3");
		set = model.files();
		expect(set && set->CommitFiltered && paths(*set) == std::vector<std::string>{ "src/a.cpp" }, "file filters combine with the commit filter");

		expect(model.apply(gesture(kind::clear, { })) == core::commit_filter_refusal::none && model.file_filter().Hidden.empty() && !model.files(),
			   "Clear all filters forgets the file filters too");
		expect(model.hide_files_changed_by({ "nope" }) == core::commit_filter_refusal::unknown_commit && model.projection().Refusal,
			   "a malformed commit is refused and named");

		auto unread = application::commit_filter_model{ };
		unread.adopt(snapshot());
		unread.adopt_comparison(core::filter_comparison{ });
		expect(unread.hide_files_changed_by({ hex('2') }) == core::commit_filter_refusal::suspended, "the trees must be read first");
	}

	void test_bulk_label()
	{
		auto model = application::commit_filter_model{ };
		model.adopt(snapshot());
		model.adopt_comparison(core::filter_comparison{ });
		auto answer = touches(model, application::commit_filter_touches_status::resolved);
		answer.Touches[1].Paths.clear();
		for (int index = 0; index < 100; ++index) {
			answer.Touches[1].Paths.push_back(touched("gen/f" + std::to_string(1000 + index) + ".cpp", 'A'));
		}
		model.begin_touches();
		model.adopt_touches(answer);
		const auto projection = model.projection();
		expect(projection.Rows[1].Files == std::optional<std::size_t>{ 100U } && projection.Rows[1].Bulk, "a commit changing 100 files is labelled bulk");
		expect(!projection.Rows[0].Bulk && !projection.Rows[2].Bulk, "smaller commits are not");
		expect(!model.files(), "the label filters nothing");
	}
} // namespace

namespace {
	auto blob(const char aDigit) -> core::git_blob_oid
	{
		return *core::parse_git_blob_oid(std::string(40U, aDigit));
	}

	auto changed(std::string aPath, const char aChange, const char aLeft, const char aRight) -> application::commit_filter_touched_path
	{
		auto value      = touched(std::move(aPath), aChange);
		value.LeftBlob  = aChange == 'A' ? std::nullopt : std::optional{ blob(aLeft) };
		value.RightBlob = aChange == 'D' ? std::nullopt : std::optional{ blob(aRight) };
		return value;
	}

	/// RFC-BPR-0089 M3 (owner, Q2): src/both.cpp, which hidden C2 changes
	/// between shown C1 and C3, is one file with two exact per-commit diffs.
	void test_split_segments()
	{
		auto model = ready();
		expect(model.apply(gesture(core::commit_filter_operation_kind::hide, { '2' })) == core::commit_filter_refusal::none, "Hide C2");
		const auto files = model.files();
		expect(files.has_value() && files->SplitFiles == 1U, "one file is split");
		const auto both = std::ranges::find(files->Files, std::string{ "src/both.cpp" }, [](const application::commit_filter_file& aFile) -> const std::string& { return aFile.Record.Path; });
		expect(both != files->Files.end() && both->Split && !both->Filter && both->Segments.size() == 2U, "it keeps one row with two per-commit diffs");
		expect(both->Segments[0].Endpoints == core::filtered_diff_endpoints{ .Left = oid('b'), .Right = oid('1') } && both->Segments[0].HiddenAfter == std::vector{ oid('2') }
				   && both->Segments[1].Endpoints == core::filtered_diff_endpoints{ .Left = oid('2'), .Right = oid('3') } && both->Segments[1].HiddenAfter.empty()
				   && both->Segments[0].Change == application::github_file_change::modified,
			   "C1's own diff, then C3's, with C2 named after the first");
		const auto entries = application::workbench_segments(both->Segments);
		expect(entries.size() == 2U && entries[1].Endpoints == both->Segments[1].Endpoints && entries[1].Change == application::workbench_file_change_kind::modified && !entries[1].PreviousPath,
			   "the workbench reads the same two diffs");
		const auto parts = model.split_parts("src/both.cpp");
		expect(parts && parts->Path == "src/both.cpp" && parts->Parts.size() == 2U && parts->Parts[0].ShortCommit == "1111111"
				   && parts->Parts[0].Headline == "Change the generator input" && parts->Parts[0].HiddenAfter.size() == 1U
				   && parts->Parts[0].HiddenAfter[0].ShortCommit == "2222222" && parts->Parts[0].HiddenAfter[0].Headline == "Regenerate"
				   && parts->Parts[1].Commit == hex('3'),
			   "the strip names each shown commit and the hidden one between");
		expect(!model.split_parts("src/a.cpp"), "a file with one exact diff has no strip");
		expect(model.apply(gesture(core::commit_filter_operation_kind::clear, { })) == core::commit_filter_refusal::none && !model.split_parts("src/both.cpp"),
			   "Clear all filters forgets the split");
	}

	/// RFC-BPR-0089 M3, AC06: a force push rewrites hidden C3; the commit
	/// carrying the same change is offered and applied only when chosen.
	void test_rematch_after_force_push()
	{
		auto model = application::commit_filter_model{ };
		model.adopt(snapshot());
		model.adopt_comparison(core::filter_comparison{ });
		auto answer    = touches(model, application::commit_filter_touches_status::resolved);
		answer.Touches = {
			{ .Commit = oid('1'), .Paths = { changed("src/a.cpp", 'M', 'a', 'b'), changed("src/both.cpp", 'M', 'a', 'b'), changed("src/new.cpp", 'A', '0', 'c') } },
			{ .Commit = oid('2'), .Paths = { changed("gen/x.cpp", 'A', '0', 'd'), changed("gen/y.cpp", 'A', '0', 'e'), changed("src/both.cpp", 'M', 'b', 'f') } },
			{ .Commit = oid('3'), .Paths = { changed("gen/x.cpp", 'M', 'd', '7'), changed("src/a.cpp", 'M', 'b', '8'), changed("src/both.cpp", 'M', 'f', '9') } },
		};
		model.begin_touches();
		model.adopt_touches(answer);
		expect(model.apply(gesture(core::commit_filter_operation_kind::hide, { '3' })) == core::commit_filter_refusal::none, "Hide C3");
		expect(model.state().Hidden.size() == 1U && model.state().Hidden[0].Changes && model.state().Hidden[0].Changes->size() == 3U && model.state().Hidden[0].Author == "octo-author",
			   "hiding C3 records what it changes, with its blobs, and its author");

		// The force push: C3 amended as C4 on C2, the same change.
		auto pushed                = snapshot();
		pushed.PullRequest.HeadOid = application::commit_oid{ hex('4') };
		pushed.Commits[2]          = commit('4', { '2' }, "Follow-ups, amended");
		model.adopt(pushed);
		auto projection = model.projection();
		expect(projection.Gone.size() == 1U && projection.Gone[0].ShortCommit == "3333333" && projection.Rematches.empty(),
			   "the gone commit is named; with a new headline and the trees unread nothing is offered yet");
		auto again              = touches(model, application::commit_filter_touches_status::resolved);
		again.Touches           = { answer.Touches[0], answer.Touches[1], answer.Touches[2] };
		again.Touches[2].Commit = oid('4');
		model.begin_touches();
		model.adopt_touches(again);
		projection = model.projection();
		expect(projection.Rematches.size() == 1U && projection.Rematches[0].Gone == hex('3') && projection.Rematches[0].Candidate == hex('4')
				   && projection.Rematches[0].Basis == core::filter_rematch_basis::same_change && projection.Rematches[0].CandidateHeadline == "Follow-ups, amended"
				   && projection.Rematches[0].GoneHeadline == "Follow-ups",
			   "once its trees are read, C4 is offered as the same change");
		expect(projection.Rows[2].State == core::filter_commit_state::shown && projection.Status == core::commit_filter_status::inactive,
			   "the offer applies nothing: C4 is shown");
		expect(model.rematch({ .Gone = hex('3'), .Candidate = hex('1') }) == core::commit_filter_refusal::not_offered && model.projection().Refusal == core::commit_filter_refusal::not_offered,
			   "a commit that is not offered is refused");
		expect(model.rematch({ .Gone = hex('3'), .Candidate = hex('4') }) == core::commit_filter_refusal::none, "choosing the offer is accepted");
		projection = model.projection();
		expect(projection.Gone.empty() && projection.Rematches.empty() && projection.Rows[2].State == core::filter_commit_state::hidden
				   && projection.Status == core::commit_filter_status::active && projection.Shown == 2U,
			   "C4 is hidden in C3's place and the gone commit is forgotten");
	}
} // namespace

auto main() -> int
{
	test_steps_and_rows();
	test_owner_filters();
	test_unavailable_and_suspended();
	test_force_push();
	test_generated_files_are_set_aside();
	test_head_attributes_are_read();
	test_hide_files_these_commits_changed();
	test_bulk_label();
	test_split_segments();
	test_rematch_after_force_push();
	std::cout << "commit-filter-model-tests:pass\n";
	return 0;
}
