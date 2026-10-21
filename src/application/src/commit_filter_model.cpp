#include "bpr/application/commit_filter_model.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto parse_commit(std::string_view aText) -> std::optional<core::git_commit_oid>
		{
			auto parsed = core::parse_git_commit_oid(aText);
			return parsed && parsed->valid() ? parsed : std::nullopt;
		}

		[[nodiscard]] auto hex(const core::git_commit_oid& aOid) -> std::string
		{
			return std::string{ core::canonical_hex(aOid).view() };
		}

		[[nodiscard]] auto short_hex(const core::git_commit_oid& aOid) -> std::string
		{
			auto text = hex(aOid);
			text.resize(std::min<std::size_t>(text.size(), 7U));
			return text;
		}

		[[nodiscard]] auto trees_for(const commit_filter_touches_status aStatus) noexcept -> commit_filter_trees
		{
			switch (aStatus) {
			case commit_filter_touches_status::resolved:            return commit_filter_trees::ready;
			case commit_filter_touches_status::objects_unavailable: return commit_filter_trees::objects_unavailable;
			case commit_filter_touches_status::limit_exceeded:      return commit_filter_trees::limit_exceeded;
			case commit_filter_touches_status::stale:
			case commit_filter_touches_status::cancelled:           return commit_filter_trees::idle;
			case commit_filter_touches_status::failure:             return commit_filter_trees::failed;
			}
			return commit_filter_trees::failed;
		}

		[[nodiscard]] auto touched(const commit_filter_commit_touch& aTouch, std::string_view aPath) -> const commit_filter_touched_path*
		{
			const auto found = std::ranges::lower_bound(aTouch.Paths, aPath, { }, &commit_filter_touched_path::Path);
			return found != aTouch.Paths.end() && found->Path == aPath ? &*found : nullptr;
		}

		[[nodiscard]] auto chain_position(const core::filter_commit_graph& aGraph, const core::git_commit_oid& aOid) -> std::optional<std::size_t>
		{
			const auto found = std::ranges::find(aGraph.Chain, aOid);
			return found != aGraph.Chain.end() ? std::optional{ static_cast<std::size_t>(found - aGraph.Chain.begin()) } : std::nullopt;
		}
	} // namespace

	void commit_filter_model::adopt(const github_pull_request_snapshot& aSnapshot)
	{
		const auto head = parse_commit(aSnapshot.PullRequest.HeadOid.value());
		auto       list = core::filter_commit_list{ };
		list.Head       = head.value_or(core::git_commit_oid{ });
		auto parents    = true;
		auto authors    = std::vector<std::string>{ };
		list.Commits.reserve(aSnapshot.Commits.size());
		authors.reserve(aSnapshot.Commits.size());
		auto malformed = !head.has_value();
		for (const auto& record : aSnapshot.Commits) {
			auto commit = core::filter_commit{ };
			if (const auto oid = parse_commit(record.Oid.value())) {
				commit.Oid = *oid;
			} else {
				malformed = true;
			}
			if (!record.Parents) {
				parents = false;
			} else {
				for (const auto& parent : *record.Parents) {
					if (const auto oid = parse_commit(parent.value())) {
						commit.Parents.push_back(*oid);
					} else {
						malformed = true;
					}
				}
			}
			commit.Headline = record.Headline;
			list.Commits.push_back(std::move(commit));
			authors.push_back(record.Author ? record.Author->Login : std::string{ });
		}
		const auto reported = aSnapshot.PullRequest.ReportedCommitCount;
		list.Truncated      = reported ? *reported > aSnapshot.Commits.size() : aSnapshot.Commits.size() >= core::maximum_filter_commits;
		if (list.Truncated) {
			Graph = std::unexpected{ core::commit_filter_unavailable::truncated };
		} else if (malformed) {
			Graph = std::unexpected{ core::commit_filter_unavailable::invalid_list };
		} else if (!parents) {
			Graph = std::unexpected{ core::commit_filter_unavailable::parents_unknown };
		} else {
			Graph = core::build_filter_commit_graph(list);
		}
		if (Head != head) {
			Touches.clear();
			Trees = commit_filter_trees::idle;
		}
		Head          = head;
		Known         = true;
		Authors       = std::move(authors);
		SnapshotFiles = aSnapshot.Files;
		if (Graph && Trees == commit_filter_trees::ready && Touches.size() != Graph->Chain.size()) {
			Touches.clear();
			Trees = commit_filter_trees::idle;
		}
	}

	void commit_filter_model::clear()
	{
		*this = commit_filter_model{ };
	}

	void commit_filter_model::adopt_comparison(std::optional<core::filter_comparison> aComparison)
	{
		Comparison = aComparison;
	}

	auto commit_filter_model::restore(core::commit_filter_state aState) -> bool
	{
		if (!core::valid(aState)) {
			return false;
		}
		State = std::move(aState);
		return true;
	}

	auto commit_filter_model::touches_steps() const -> std::optional<std::vector<commit_filter_touch_step>>
	{
		if (!Graph || Trees != commit_filter_trees::idle || !Head) {
			return std::nullopt;
		}
		auto steps = std::vector<commit_filter_touch_step>{ };
		steps.reserve(Graph->Chain.size());
		for (std::size_t position = 0U; position < Graph->Chain.size(); ++position) {
			steps.push_back({ .Parent = core::chain_parent(*Graph, position), .Commit = Graph->Chain[position] });
		}
		return steps;
	}

	auto commit_filter_model::head() const noexcept -> const std::optional<core::git_commit_oid>&
	{
		return Head;
	}

	void commit_filter_model::begin_touches()
	{
		if (Trees == commit_filter_trees::idle) {
			Trees = commit_filter_trees::loading;
		}
	}

	void commit_filter_model::adopt_touches(const commit_filter_touches_observation& aObservation)
	{
		if (Trees != commit_filter_trees::loading || !Graph || std::optional{ aObservation.Request.Head } != Head) {
			return;
		}
		const auto& steps = aObservation.Request.Steps;
		if (steps.size() != Graph->Chain.size()) {
			return;
		}
		for (std::size_t position = 0U; position < steps.size(); ++position) {
			if (steps[position].Commit != Graph->Chain[position] || steps[position].Parent != core::chain_parent(*Graph, position)) {
				return;
			}
		}
		Trees = trees_for(aObservation.Status);
		if (Trees != commit_filter_trees::ready) {
			Touches.clear();
			return;
		}
		if (aObservation.Touches.size() != steps.size()) {
			Trees = commit_filter_trees::failed;
			Touches.clear();
			return;
		}
		for (std::size_t position = 0U; position < steps.size(); ++position) {
			if (aObservation.Touches[position].Commit != steps[position].Commit) {
				Trees = commit_filter_trees::failed;
				Touches.clear();
				return;
			}
		}
		Touches = aObservation.Touches;
	}

	auto commit_filter_model::span() const -> std::optional<std::expected<core::filter_commit_span, core::commit_filter_suspension>>
	{
		if (!Graph || !Comparison) {
			return std::nullopt;
		}
		return core::filter_span_for(*Graph, *Comparison);
	}

	auto commit_filter_model::apply(const commit_filter_gesture& aGesture) -> core::commit_filter_refusal
	{
		Refusal.reset();
		if (aGesture.Kind == core::commit_filter_operation_kind::clear) {
			State = core::commit_filter_state{ };
			return core::commit_filter_refusal::none;
		}
		const auto refuse = [this](const core::commit_filter_refusal aRefusal) {
			Refusal = aRefusal;
			return aRefusal;
		};
		if (!Graph) {
			return refuse(core::commit_filter_refusal::unavailable);
		}
		const auto covered = span();
		if (!covered || !*covered) {
			return refuse(core::commit_filter_refusal::suspended);
		}
		auto operation = core::commit_filter_operation{ .Kind = aGesture.Kind, .Selection = { } };
		operation.Selection.reserve(aGesture.Commits.size());
		for (const auto& text : aGesture.Commits) {
			const auto oid = parse_commit(text);
			if (!oid) {
				return refuse(core::commit_filter_refusal::unknown_commit);
			}
			operation.Selection.push_back(*oid);
		}
		auto next = core::apply_commit_filter(State, *Graph, **covered, operation);
		if (!next) {
			return refuse(next.error());
		}
		State = std::move(*next);
		return core::commit_filter_refusal::none;
	}

	auto commit_filter_model::state() const noexcept -> const core::commit_filter_state&
	{
		return State;
	}

	auto commit_filter_model::filtering() const -> bool
	{
		if (!Graph || Trees != commit_filter_trees::ready) {
			return false;
		}
		const auto covered = span();
		return covered && *covered && core::summarize_commit_filter(State, Graph, covered).Status == core::commit_filter_status::active;
	}

	auto commit_filter_model::files() const -> std::optional<commit_filter_files>
	{
		if (!filtering()) {
			return std::nullopt;
		}
		const auto& graph   = *Graph;
		const auto  covered = **span();
		auto        touches = std::vector<core::filter_commit_touch>{ };
		touches.reserve(covered.size());
		for (std::size_t position = covered.Begin; position < covered.End; ++position) {
			auto touch   = core::filter_commit_touch{ };
			touch.Commit = graph.Chain[position];
			touch.Paths.reserve(Touches[position].Paths.size());
			for (const auto& path : Touches[position].Paths) {
				touch.Paths.push_back(path.Path);
			}
			touches.push_back(std::move(touch));
		}
		const auto decided = core::filter_paths(State, graph, covered, touches);
		if (!decided) {
			return std::nullopt;
		}

		auto listed = std::map<std::string_view, std::size_t>{ };
		for (std::size_t index = 0U; index < SnapshotFiles.size(); ++index) {
			listed.emplace(SnapshotFiles[index].Path, index);
		}
		auto placed = std::vector<std::pair<std::size_t, commit_filter_file>>{ };
		auto extras = std::vector<commit_filter_file>{ };
		auto result = commit_filter_files{ };
		for (const auto& path : decided->Paths) {
			const auto found    = listed.find(path.Path);
			const auto snapshot = found != listed.end() ? &SnapshotFiles[found->second] : nullptr;
			auto       file     = commit_filter_file{ };
			if (path.Kind == core::filtered_path_kind::unchanged) {
				// The comparison's own diff; a path it does not list shows nothing.
				if (snapshot == nullptr) {
					continue;
				}
				file.Record = *snapshot;
			} else {
				file.Record.Path        = path.Path;
				file.Record.ContentKind = snapshot != nullptr ? snapshot->ContentKind : github_file_content_kind::unknown;
				file.Record.ViewedState = snapshot != nullptr ? snapshot->ViewedState : github_file_viewed_state::unavailable;
				file.Record.Change      = github_file_change::modified;
				if (path.Kind == core::filtered_path_kind::split) {
					file.Split = true;
					for (const auto& segment : path.Segments) {
						file.SplitHidden.insert(file.SplitHidden.end(), segment.HiddenAfter.begin(), segment.HiddenAfter.end());
					}
					++result.SplitFiles;
				} else {
					// What the file is between the two commits, from the first
					// shown commit's own change and the last one's.
					const auto first = path.Left == graph.Fork ? std::optional<std::size_t>{ 0U } : [&]() -> std::optional<std::size_t> {
						const auto left = chain_position(graph, path.Left);
						return left ? std::optional{ *left + 1U } : std::nullopt;
					}();
					const auto last = chain_position(graph, path.Right);
					if (!first || !last || *first >= Touches.size() || *last >= Touches.size()) {
						return std::nullopt;
					}
					const auto* opened = touched(Touches[*first], path.Path);
					const auto* closed = touched(Touches[*last], path.Path);
					if (opened == nullptr || closed == nullptr) {
						return std::nullopt;
					}
					const auto renamed     = opened->Change == 'R' && opened->PreviousPath.has_value();
					const auto copied      = opened->Change == 'C' && opened->PreviousPath.has_value();
					const auto leftExists  = opened->Change != 'A' && opened->Change != 'R' && opened->Change != 'C';
					const auto rightExists = closed->Change != 'D';
					if (!rightExists) {
						if (!leftExists && !renamed) {
							// Created and removed between the two commits: no side has it.
							continue;
						}
						file.Record.Change = github_file_change::removed;
					} else if (renamed) {
						file.Record.Change       = github_file_change::renamed;
						file.Record.PreviousPath = opened->PreviousPath;
					} else if (copied) {
						file.Record.Change       = github_file_change::copied;
						file.Record.PreviousPath = opened->PreviousPath;
					} else if (!leftExists) {
						file.Record.Change = github_file_change::added;
					} else if (opened->Change == 'T' || closed->Change == 'T') {
						file.Record.Change = github_file_change::type_changed;
					}
					file.Filter = core::filtered_diff_endpoints{ .Left = path.Left, .Right = path.Right };
				}
			}
			if (snapshot != nullptr) {
				placed.emplace_back(found->second, std::move(file));
			} else {
				extras.push_back(std::move(file));
			}
		}
		std::ranges::sort(placed, { }, &std::pair<std::size_t, commit_filter_file>::first);
		result.Files.reserve(placed.size() + extras.size());
		for (auto& [index, file] : placed) {
			static_cast<void>(index);
			result.Files.push_back(std::move(file));
		}
		for (auto& file : extras) {
			result.Files.push_back(std::move(file));
		}
		return result;
	}

	auto commit_filter_model::projection() const -> commit_filter_projection
	{
		auto       result  = commit_filter_projection{ };
		const auto covered = span();
		const auto summary = core::summarize_commit_filter(State, Graph, covered);
		result.Status      = summary.Status;
		result.Suspension  = summary.Suspension;
		result.Unavailable = Known ? summary.Unavailable : std::nullopt;
		result.Shown       = summary.Shown;
		result.Spanned     = summary.Spanned;
		result.Trees       = Trees;
		result.Refusal     = Refusal;
		for (const auto& gone : summary.Gone) {
			result.Gone.push_back({ .ShortCommit = short_hex(gone.Oid), .Headline = gone.Headline });
		}
		result.ComparisonFiles = SnapshotFiles.size();
		if (const auto filtered = files()) {
			result.FilteredFiles = filtered->Files.size();
			result.SplitFiles    = filtered->SplitFiles;
		}
		if (!Graph) {
			return result;
		}
		const auto& graph = *Graph;
		// Under a suspended or unresolved comparison the rows still say what
		// the filter hides, across the whole chain.
		const auto rowSpan = covered && *covered ? **covered : core::filter_commit_span{ .Begin = 0U, .End = graph.Chain.size() };
		result.Rows.reserve(graph.Entries.size());
		for (const auto& entry : graph.Entries) {
			auto row        = commit_filter_row{ };
			row.Commit      = hex(entry.Oid);
			row.ShortCommit = short_hex(entry.Oid);
			row.Headline    = entry.Listed < graph.Headlines.size() ? graph.Headlines[entry.Listed] : std::string{ };
			row.Author      = entry.Listed < Authors.size() ? Authors[entry.Listed] : std::string{ };
			row.Role        = entry.Role;
			row.State       = core::commit_state(State, graph, rowSpan, entry);
			if (Trees == commit_filter_trees::ready && entry.Chain && *entry.Chain < Touches.size()) {
				row.Files = Touches[*entry.Chain].Paths.size();
			}
			if (entry.Merge) {
				row.Merge = short_hex(*entry.Merge);
			}
			result.Rows.push_back(std::move(row));
		}
		return result;
	}
} // namespace bpr::application
