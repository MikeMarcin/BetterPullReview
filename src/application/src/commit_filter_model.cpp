#include "bpr/application/commit_filter_model.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
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

		/// \brief A commit's own change to one path, as a file change.
		[[nodiscard]] auto change_of(const commit_filter_touched_path& aTouched) noexcept -> github_file_change
		{
			switch (aTouched.Change) {
			case 'A': return github_file_change::added;
			case 'D': return github_file_change::removed;
			case 'R': return aTouched.PreviousPath ? github_file_change::renamed : github_file_change::added;
			case 'C': return aTouched.PreviousPath ? github_file_change::copied : github_file_change::added;
			case 'T': return github_file_change::type_changed;
			default:  return github_file_change::modified;
			}
		}

		[[nodiscard]] auto workbench_change(const github_file_change aChange) noexcept -> workbench_file_change_kind
		{
			switch (aChange) {
			case github_file_change::added:         return workbench_file_change_kind::added;
			case github_file_change::modified:      return workbench_file_change_kind::modified;
			case github_file_change::removed:       return workbench_file_change_kind::removed;
			case github_file_change::renamed:       return workbench_file_change_kind::renamed;
			case github_file_change::copied:        return workbench_file_change_kind::copied;
			case github_file_change::type_changed:  return workbench_file_change_kind::type_changed;
			case github_file_change::unchanged:     return workbench_file_change_kind::unchanged;
			case github_file_change::uninitialized: return workbench_file_change_kind::uninitialized;
			}
			return workbench_file_change_kind::uninitialized;
		}

		[[nodiscard]] auto repository_path(const std::string& aPath) -> std::filesystem::path
		{
			return std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() } };
		}
	} // namespace

	auto workbench_segments(const std::vector<commit_filter_segment>& aSegments) -> std::vector<workbench_split_segment>
	{
		auto result = std::vector<workbench_split_segment>{ };
		result.reserve(aSegments.size());
		for (const auto& segment : aSegments) {
			auto entry      = workbench_split_segment{ };
			entry.Endpoints = segment.Endpoints;
			entry.Change    = workbench_change(segment.Change);
			if (segment.PreviousPath) {
				entry.PreviousPath = repository_path(*segment.PreviousPath);
			}
			result.push_back(std::move(entry));
		}
		return result;
	}

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
			commit.Author   = record.Author ? record.Author->Login : std::string{ };
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
			// [[BPR-1692]]. Generated status follows the head's attributes.
			AttributeRules.reset();
			AttributesAsked.reset();
			AttributesLoading = false;
			Attributes        = generated_attributes_read::pending;
			AttributesExceeded.reset();
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

	auto commit_filter_model::restore_files(core::file_filter_state aState) -> bool
	{
		if (!core::valid(aState)) {
			return false;
		}
		FileFilter = std::move(aState);
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
		// RFC-BPR-0089 M3. What the hidden commits change, while it is known.
		record_hidden_changes();
	}

	auto commit_filter_model::chain_changes() const -> std::vector<core::filter_commit_changes>
	{
		auto result = std::vector<core::filter_commit_changes>{ };
		if (!Graph || Trees != commit_filter_trees::ready || Touches.size() != Graph->Chain.size()) {
			return result;
		}
		result.reserve(Touches.size());
		for (const auto& touch : Touches) {
			auto changes   = core::filter_commit_changes{ };
			changes.Commit = touch.Commit;
			changes.Changes.reserve(touch.Paths.size());
			for (const auto& path : touch.Paths) {
				auto change         = core::filter_path_change{ };
				change.Path         = path.Path;
				change.PreviousPath = path.PreviousPath;
				change.Left         = path.LeftBlob;
				change.Right        = path.RightBlob;
				changes.Changes.push_back(std::move(change));
			}
			result.push_back(std::move(changes));
		}
		return result;
	}

	void commit_filter_model::record_hidden_changes()
	{
		if (Graph && !State.Hidden.empty()) {
			State = core::record_hidden_changes(State, *Graph, chain_changes());
		}
	}

	auto commit_filter_model::rematch(const commit_filter_rematch_gesture& aGesture) -> core::commit_filter_refusal
	{
		Refusal.reset();
		const auto refuse = [this](const core::commit_filter_refusal aRefusal) {
			Refusal = aRefusal;
			return aRefusal;
		};
		const auto gone      = parse_commit(aGesture.Gone);
		const auto candidate = parse_commit(aGesture.Candidate);
		if (!Graph) {
			return refuse(core::commit_filter_refusal::unavailable);
		}
		if (!gone || !candidate) {
			return refuse(core::commit_filter_refusal::unknown_commit);
		}
		const auto changes = chain_changes();
		const auto offers  = core::rematch_offers(State, *Graph, changes);
		const auto offer   = std::ranges::find_if(offers, [&](const core::filter_rematch_offer& aOffer) { return aOffer.Gone == *gone && aOffer.Candidate == *candidate; });
		if (offer == offers.end()) {
			return refuse(core::commit_filter_refusal::not_offered);
		}
		auto next = core::apply_rematch(State, *Graph, changes, *offer);
		if (!next) {
			return refuse(next.error());
		}
		State = std::move(*next);
		return core::commit_filter_refusal::none;
	}

	auto commit_filter_model::split_parts(const std::string_view aPath) const -> std::optional<split_diff_projection>
	{
		const auto filtered = files();
		if (!filtered || !Graph) {
			return std::nullopt;
		}
		const auto named = [&](const std::vector<commit_filter_file>& aFiles) -> const commit_filter_file* {
			const auto found = std::ranges::find_if(aFiles, [&](const commit_filter_file& aFile) { return aFile.Record.Path == aPath; });
			return found != aFiles.end() ? &*found : nullptr;
		};
		const auto* file = named(filtered->Files);
		file             = file != nullptr ? file : named(filtered->SetAside);
		if (file == nullptr || !file->Split || file->Segments.empty()) {
			return std::nullopt;
		}
		const auto headline = [this](const core::git_commit_oid& aOid) {
			const auto* entry = core::find_filter_commit(*Graph, aOid);
			return entry != nullptr && entry->Listed < Graph->Headlines.size() ? Graph->Headlines[entry->Listed] : std::string{ };
		};
		auto result = split_diff_projection{ };
		result.Path = file->Record.Path;
		for (const auto& segment : file->Segments) {
			auto part        = split_diff_part{ };
			part.Commit      = hex(segment.Endpoints.Right);
			part.ShortCommit = short_hex(segment.Endpoints.Right);
			part.Headline    = headline(segment.Endpoints.Right);
			for (const auto& hidden : segment.HiddenAfter) {
				part.HiddenAfter.push_back({ .ShortCommit = short_hex(hidden), .Headline = headline(hidden) });
			}
			result.Parts.push_back(std::move(part));
		}
		return result;
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
			State      = core::commit_filter_state{ };
			FileFilter = core::file_filter_state{ };
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
		record_hidden_changes();
		return core::commit_filter_refusal::none;
	}

	auto commit_filter_model::state() const noexcept -> const core::commit_filter_state&
	{
		return State;
	}

	auto commit_filter_model::graph() const noexcept -> const core::filter_commit_graph*
	{
		return Graph.has_value() ? &*Graph : nullptr;
	}

	auto commit_filter_model::filtering() const -> bool
	{
		if (!Graph || Trees != commit_filter_trees::ready) {
			return false;
		}
		const auto covered = span();
		return covered && *covered && core::summarize_commit_filter(State, Graph, covered).Status == core::commit_filter_status::active;
	}

	auto commit_filter_model::span_touches(const core::filter_commit_span& aSpan) const -> std::vector<core::filter_commit_touch>
	{
		auto touches = std::vector<core::filter_commit_touch>{ };
		if (!Graph || aSpan.End > Touches.size()) {
			return touches;
		}
		touches.reserve(aSpan.size());
		for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
			auto touch   = core::filter_commit_touch{ };
			touch.Commit = Graph->Chain[position];
			touch.Paths.reserve(Touches[position].Paths.size());
			for (const auto& path : Touches[position].Paths) {
				touch.Paths.push_back(path.Path);
			}
			touches.push_back(std::move(touch));
		}
		return touches;
	}

	auto commit_filter_model::commit_filtered_files() const -> std::optional<commit_filter_files>
	{
		if (!filtering()) {
			return std::nullopt;
		}
		const auto& graph   = *Graph;
		const auto  covered = **span();
		const auto  touches = span_touches(covered);
		const auto  decided = core::filter_paths(State, graph, covered, touches);
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
						// M3. Each per-commit diff is that commit's own change to the path.
						const auto  position = chain_position(graph, segment.Right);
						const auto* own      = position && *position < Touches.size() ? touched(Touches[*position], path.Path) : nullptr;
						if (own == nullptr) {
							return std::nullopt;
						}
						auto part         = commit_filter_segment{ };
						part.Endpoints    = core::filtered_diff_endpoints{ .Left = segment.Left, .Right = segment.Right };
						part.Change       = change_of(*own);
						part.PreviousPath = part.Change == github_file_change::renamed || part.Change == github_file_change::copied ? own->PreviousPath : std::nullopt;
						part.HiddenAfter  = segment.HiddenAfter;
						file.Segments.push_back(std::move(part));
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

	auto commit_filter_model::generated_row(const commit_filter_file& aFile) const -> generated_file_row
	{
		const auto* mark = core::find_generated_mark(GeneratedMarks, core::file_identity_of(aFile.Record.Path));
		auto        row  = generated_file_row{
			.Path         = aFile.Record.Path,
			.Reason       = aFile.Generated.Reason,
			.Generated    = aFile.Generated.Generated,
			.ReviewAnyway = aFile.Generated.ReviewAnyway,
			.Additions    = aFile.Record.Additions,
			.Deletions    = aFile.Record.Deletions,
			.Gestures     = { },
		};
		for (const auto gesture : { core::generated_gesture::review_anyway, core::generated_gesture::set_aside_again, core::generated_gesture::show_as_normal,
									core::generated_gesture::treat_as_generated, core::generated_gesture::follow_attributes }) {
			if (core::generated_gesture_available(aFile.Generated, mark, gesture)) {
				row.Gestures.push_back(gesture);
			}
		}
		return row;
	}

	void commit_filter_model::project_generated(commit_filter_projection& aProjection) const
	{
		aProjection.OrdinaryGestures = generated_row(commit_filter_file{ }).Gestures;
		const auto marked            = [this](const commit_filter_file& aFile) {
			return aFile.Generated.Generated || core::find_generated_mark(GeneratedMarks, core::file_identity_of(aFile.Record.Path)) != nullptr;
		};
		const auto filtered = files();
		if (!filtered) {
			// Nothing is set aside; files reviewed anyway or marked still say so.
			for (const auto& record : SnapshotFiles) {
				auto file      = commit_filter_file{ };
				file.Record    = record;
				file.Generated = generated_decision_of(record.Path);
				if (marked(file)) {
					aProjection.Marked.push_back(generated_row(file));
				}
			}
			std::ranges::sort(aProjection.Marked, { }, &generated_file_row::Path);
			return;
		}
		aProjection.CommitFiltered    = filtered->CommitFiltered;
		aProjection.FileFilteredFiles = filtered->FileFiltered;
		aProjection.FilteredFiles     = filtered->Files.size();
		aProjection.SplitFiles        = filtered->SplitFiles;
		aProjection.Generated.reserve(filtered->SetAside.size());
		for (const auto& file : filtered->SetAside) {
			aProjection.Generated.push_back(generated_row(file));
			aProjection.GeneratedAdditions += file.Record.Additions;
			aProjection.GeneratedDeletions += file.Record.Deletions;
		}
		for (const auto& file : filtered->Files) {
			if (marked(file)) {
				aProjection.Marked.push_back(generated_row(file));
			}
		}
		std::ranges::sort(aProjection.Marked, { }, &generated_file_row::Path);
	}

	auto commit_filter_model::files() const -> std::optional<commit_filter_files>
	{
		auto       result         = commit_filtered_files();
		const auto commitFiltered = result.has_value();
		if (!result) {
			result = commit_filter_files{ };
			result->Files.reserve(SnapshotFiles.size());
			for (const auto& record : SnapshotFiles) {
				result->Files.push_back({ .Record = record, .Filter = std::nullopt, .Split = false, .SplitHidden = { }, .Segments = { }, .Generated = { } });
			}
		}
		result->CommitFiltered = commitFiltered;

		// File filters first: a path they name leaves the set however it is formed.
		auto listed = std::vector<commit_filter_file>{ };
		listed.reserve(result->Files.size());
		for (auto& file : result->Files) {
			if (core::file_filter_hides(FileFilter, file.Record.Path)) {
				++result->FileFiltered;
			} else {
				listed.push_back(std::move(file));
			}
		}

		// Generated files are set aside behind their own row, unless reviewed anyway.
		auto paths = std::vector<std::string>{ };
		paths.reserve(listed.size());
		for (const auto& file : listed) {
			paths.push_back(file.Record.Path);
		}
		const auto decisions = core::decide_generated_files(paths, GeneratedMarks, AttributeRules.get());
		result->Files.clear();
		result->SplitFiles = 0U;
		for (std::size_t index = 0U; index < listed.size(); ++index) {
			auto& file     = listed[index];
			file.Generated = decisions[index];
			if (file.Generated.SetAside) {
				result->SetAside.push_back(std::move(file));
				continue;
			}
			result->SplitFiles += file.Split ? 1U : 0U;
			result->Files.push_back(std::move(file));
		}
		if (!commitFiltered && result->FileFiltered == 0U && result->SetAside.empty()) {
			return std::nullopt;
		}
		return result;
	}

	auto commit_filter_model::hide_files_changed_by(const std::vector<std::string>& aCommits) -> core::commit_filter_refusal
	{
		Refusal.reset();
		const auto refuse = [this](const core::commit_filter_refusal aRefusal) {
			Refusal = aRefusal;
			return aRefusal;
		};
		if (!Graph) {
			return refuse(core::commit_filter_refusal::unavailable);
		}
		const auto covered = span();
		if (!covered || !*covered || Trees != commit_filter_trees::ready) {
			return refuse(core::commit_filter_refusal::suspended);
		}
		auto selection = std::vector<core::git_commit_oid>{ };
		selection.reserve(aCommits.size());
		for (const auto& text : aCommits) {
			const auto oid = parse_commit(text);
			if (!oid) {
				return refuse(core::commit_filter_refusal::unknown_commit);
			}
			selection.push_back(*oid);
		}
		auto next = core::hide_files_changed_by(FileFilter, *Graph, **covered, span_touches(**covered), selection);
		if (!next) {
			return refuse(next.error());
		}
		FileFilter = std::move(*next);
		return core::commit_filter_refusal::none;
	}

	auto commit_filter_model::file_filter() const noexcept -> const core::file_filter_state&
	{
		return FileFilter;
	}

	auto commit_filter_model::adopt_generated_marks(core::generated_file_marks aMarks) -> bool
	{
		if (!core::valid(aMarks)) {
			return false;
		}
		GeneratedMarks = std::move(aMarks);
		return true;
	}

	auto commit_filter_model::generated_marks() const noexcept -> const core::generated_file_marks&
	{
		return GeneratedMarks;
	}

	void commit_filter_model::adopt_generated_attributes(std::shared_ptr<const core::generated_attribute_rules> aRules)
	{
		AttributeRules = std::move(aRules);
		Attributes     = AttributeRules != nullptr ? generated_attributes_read::read : generated_attributes_read::pending;
	}

	auto commit_filter_model::generated_attributes_wanted() const -> std::optional<std::vector<std::string>>
	{
		if (!Head || !Known || AttributesLoading) {
			return std::nullopt;
		}
		auto paths = std::vector<std::string>{ };
		paths.reserve(SnapshotFiles.size());
		for (const auto& record : SnapshotFiles) {
			paths.push_back(record.Path);
		}
		// A filtered diff can name a path the comparison does not list.
		for (const auto& touch : Touches) {
			for (const auto& touched : touch.Paths) {
				paths.push_back(touched.Path);
			}
		}
		auto directories = core::generated_attribute_directories(paths);
		if (AttributesAsked && AttributesAsked->first == *Head && AttributesAsked->second == directories) {
			return std::nullopt;
		}
		return directories;
	}

	auto commit_filter_model::begin_generated_attributes(std::vector<std::string> aDirectories) -> bool
	{
		if (!Head || AttributesLoading) {
			return false;
		}
		AttributesAsked.emplace(*Head, std::move(aDirectories));
		AttributesExceeded.reset();
		if (AttributesAsked->second.size() > maximum_generated_attribute_directories) {
			AttributeRules.reset();
			Attributes = generated_attributes_read::too_many_directories;
			return false;
		}
		// Rules read for fewer directories under this head stay until the
		// wider answer arrives: the directories only grow.
		AttributesLoading = true;
		return true;
	}

	void commit_filter_model::abandon_generated_attributes()
	{
		if (AttributesLoading) {
			AttributesLoading = false;
			AttributesAsked.reset();
		}
	}

	void commit_filter_model::adopt_generated_attributes(const generated_attributes_observation& aObservation)
	{
		if (!AttributesLoading || !AttributesAsked || AttributesAsked->first != aObservation.Request.Head
			|| AttributesAsked->second != aObservation.Request.Directories) {
			return;
		}
		AttributesLoading = false;
		AttributesExceeded.reset();
		switch (aObservation.Status) {
		case generated_attributes_status::resolved: {
			auto rules = core::read_generated_attribute_rules(aObservation.Files);
			if (rules) {
				AttributeRules = std::move(*rules);
				Attributes     = generated_attributes_read::read;
				return;
			}
			AttributeRules.reset();
			switch (rules.error()) {
			case core::generated_attributes_refusal::too_many_files: Attributes = generated_attributes_read::too_many_files; break;
			case core::generated_attributes_refusal::file_too_large: Attributes = generated_attributes_read::file_too_large; break;
			case core::generated_attributes_refusal::too_many_rules: Attributes = generated_attributes_read::too_many_rules; break;
			case core::generated_attributes_refusal::invalid_files:  Attributes = generated_attributes_read::failed; break;
			}
			return;
		}
		case generated_attributes_status::too_many_files:
			AttributeRules.reset();
			Attributes = generated_attributes_read::too_many_files;
			return;
		case generated_attributes_status::file_too_large:
			AttributeRules.reset();
			Attributes         = generated_attributes_read::file_too_large;
			AttributesExceeded = aObservation.Exceeded;
			return;
		case generated_attributes_status::objects_unavailable:
			AttributeRules.reset();
			Attributes = generated_attributes_read::objects_unavailable;
			return;
		case generated_attributes_status::stale:
		case generated_attributes_status::cancelled:
			// Asked again on the next refresh; rules already read stay.
			AttributesAsked.reset();
			return;
		case generated_attributes_status::failure:
			AttributeRules.reset();
			Attributes = generated_attributes_read::failed;
			return;
		}
	}

	auto commit_filter_model::generated_attributes() const noexcept -> generated_attributes_read
	{
		return Attributes;
	}

	auto commit_filter_model::generated_attributes_exceeded() const noexcept -> const std::optional<std::string>&
	{
		return AttributesExceeded;
	}

	auto commit_filter_model::generated_decision_of(const std::string_view aPath) const -> core::generated_decision
	{
		const auto* mark      = core::find_generated_mark(GeneratedMarks, core::file_identity_of(aPath));
		const auto  attribute = AttributeRules != nullptr ? AttributeRules->linguist_generated(aPath) : core::git_attribute_state{ };
		return core::decide_generated(mark, attribute);
	}

	auto commit_filter_model::listed_path(const std::string_view aPath) const -> bool
	{
		if (std::ranges::any_of(SnapshotFiles, [aPath](const github_file_record& aRecord) { return aRecord.Path == aPath; })) {
			return true;
		}
		const auto filtered = commit_filtered_files();
		return filtered && std::ranges::any_of(filtered->Files, [aPath](const commit_filter_file& aFile) { return aFile.Record.Path == aPath; });
	}

	auto commit_filter_model::generated_mark_after(const generated_file_gesture& aGesture) const
		-> std::optional<core::generated_file_mark>
	{
		const auto file = core::file_identity_of(aGesture.Path);
		if (!file.valid() || !listed_path(aGesture.Path)) {
			return std::nullopt;
		}
		const auto* mark = core::find_generated_mark(GeneratedMarks, file);
		if (!core::generated_gesture_available(generated_decision_of(aGesture.Path), mark, aGesture.Gesture)) {
			return std::nullopt;
		}
		const auto next = core::apply_generated_gesture(GeneratedMarks, file, aGesture.Gesture);
		if (!next) {
			return std::nullopt;
		}
		const auto* kept = core::find_generated_mark(*next, file);
		return kept != nullptr ? *kept : core::generated_file_mark{ .File = file, .Mark = core::generated_mark::none, .ReviewAnyway = false };
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
		// RFC-BPR-0089 M3. What each gone commit may be matched to, offered only.
		if (Graph) {
			for (const auto& offer : core::rematch_offers(State, *Graph, chain_changes())) {
				const auto  gone      = std::ranges::find(State.Hidden, offer.Gone, &core::filter_hidden_commit::Oid);
				const auto* listed    = core::find_filter_commit(*Graph, offer.Candidate);
				auto        row       = commit_filter_rematch_row{ };
				row.Gone              = hex(offer.Gone);
				row.Candidate         = hex(offer.Candidate);
				row.GoneShort         = short_hex(offer.Gone);
				row.GoneHeadline      = gone != State.Hidden.end() ? gone->Headline : std::string{ };
				row.CandidateShort    = short_hex(offer.Candidate);
				row.CandidateHeadline = listed != nullptr && listed->Listed < Graph->Headlines.size() ? Graph->Headlines[listed->Listed] : std::string{ };
				row.Basis             = offer.Basis;
				result.Rematches.push_back(std::move(row));
			}
		}
		result.ComparisonFiles    = SnapshotFiles.size();
		result.FileFilterPaths    = FileFilter.Hidden.size();
		result.AttributesRead     = AttributeRules != nullptr;
		result.Attributes         = Attributes;
		result.AttributesExceeded = AttributesExceeded;
		project_generated(result);
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
				row.Bulk  = core::bulk_commit(*row.Files);
			}
			if (entry.Merge) {
				row.Merge = short_hex(*entry.Merge);
			}
			result.Rows.push_back(std::move(row));
		}
		return result;
	}
} // namespace bpr::application
