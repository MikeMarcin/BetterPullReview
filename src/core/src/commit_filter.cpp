#include "bpr/core/commit_filter.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ranges>
#include <string_view>
#include <utility>

namespace bpr::core {
	namespace {
		/// The diff pipeline's own path bound (`maximum_identified_path_bytes`).
		constexpr std::size_t maximum_filter_path_bytes = 4U * 1024U;

		/// \brief A listed commit's position, sorted by commit for lookup.
		struct listed_position
		{
			git_commit_oid Oid;
			std::size_t    Listed = 0U;

			auto operator<=>(const listed_position&) const = default;
		};

		[[nodiscard]] auto find_listed(const std::vector<listed_position>& aIndex, const git_commit_oid& aOid) noexcept
			-> std::optional<std::size_t>
		{
			const auto found = std::ranges::lower_bound(aIndex, aOid, { }, &listed_position::Oid);
			return found != aIndex.end() && found->Oid == aOid ? std::optional{ found->Listed } : std::nullopt;
		}

		/// \brief The text cut to a retained bound at a UTF-8 boundary.
		[[nodiscard]] auto retained_headline(std::string_view aHeadline, const std::size_t aBound = maximum_filter_headline_bytes) -> std::string
		{
			if (aHeadline.size() <= aBound) {
				return std::string{ aHeadline };
			}
			auto end = aBound;
			while (end > 0U && (static_cast<unsigned char>(aHeadline[end]) & 0xC0U) == 0x80U) {
				--end;
			}
			return std::string{ aHeadline.substr(0U, end) };
		}

		[[nodiscard]] auto hidden_oid(const commit_filter_state& aState, const git_commit_oid& aOid) noexcept -> bool
		{
			return std::ranges::binary_search(aState.Hidden, aOid, { }, &filter_hidden_commit::Oid);
		}

		/// \brief The chain position a listed commit is filtered at: its own, or
		/// its merge's for a side commit. None for a side commit no merge reaches.
		[[nodiscard]] auto filtered_position(const filter_commit_graph& aGraph, const filter_commit_entry& aEntry) noexcept
			-> std::optional<std::size_t>
		{
			if (aEntry.Chain) {
				return aEntry.Chain;
			}
			if (!aEntry.Merge) {
				return std::nullopt;
			}
			const auto* merge = find_filter_commit(aGraph, *aEntry.Merge);
			return merge != nullptr ? merge->Chain : std::nullopt;
		}

		[[nodiscard]] auto chain_position(const filter_commit_graph& aGraph, const git_commit_oid& aOid) noexcept
			-> std::optional<std::size_t>
		{
			const auto found = std::ranges::find(aGraph.Chain, aOid);
			return found != aGraph.Chain.end() ? std::optional{ static_cast<std::size_t>(found - aGraph.Chain.begin()) } : std::nullopt;
		}

		[[nodiscard]] auto headline_of(const filter_commit_graph& aGraph, const git_commit_oid& aOid) -> std::string
		{
			const auto* entry = find_filter_commit(aGraph, aOid);
			return entry != nullptr && entry->Listed < aGraph.Headlines.size() ? aGraph.Headlines[entry->Listed] : std::string{ };
		}

		[[nodiscard]] auto valid_filter_path(std::string_view aPath) noexcept -> bool
		{
			return !aPath.empty() && aPath.size() <= maximum_filter_path_bytes;
		}

		[[nodiscard]] auto author_of(const filter_commit_graph& aGraph, const git_commit_oid& aOid) -> std::string
		{
			const auto* entry = find_filter_commit(aGraph, aOid);
			return entry != nullptr && entry->Listed < aGraph.Authors.size() ? aGraph.Authors[entry->Listed] : std::string{ };
		}

		/// \brief A commit's changes: bounded, valid paths, sorted and unique,
		/// each with a blob on at least one side, of the commit's own format.
		[[nodiscard]] auto valid_changes(const std::vector<filter_path_change>& aChanges, const git_hash_algorithm aAlgorithm) noexcept -> bool
		{
			if (aChanges.size() > maximum_filter_commit_paths) {
				return false;
			}
			const auto blob = [aAlgorithm](const std::optional<git_blob_oid>& aBlob) { return !aBlob || (aBlob->valid() && aBlob->algorithm() == aAlgorithm); };
			for (std::size_t index = 0U; index < aChanges.size(); ++index) {
				const auto& change = aChanges[index];
				if (!valid_filter_path(change.Path) || (change.PreviousPath && !valid_filter_path(*change.PreviousPath)) || (!change.Left && !change.Right)
					|| !blob(change.Left) || !blob(change.Right) || (index != 0U && !(aChanges[index - 1U].Path < change.Path))) {
					return false;
				}
			}
			return true;
		}

		/// \brief The changes `aChanges` names for one commit, when valid.
		[[nodiscard]] auto changes_of(std::span<const filter_commit_changes> aChanges, const git_commit_oid& aOid) noexcept
			-> const std::vector<filter_path_change>*
		{
			const auto found = std::ranges::find(aChanges, aOid, &filter_commit_changes::Commit);
			return found != aChanges.end() && valid_changes(found->Changes, aOid.algorithm()) ? &found->Changes : nullptr;
		}

		/// \brief The touches' total path count when they cover exactly the
		/// span's chain commits in order, each sorted, unique and bounded.
		[[nodiscard]] auto checked_touches(const filter_commit_graph& aGraph, const filter_commit_span& aSpan,
										   std::span<const filter_commit_touch> aTouches) noexcept -> std::optional<std::size_t>
		{
			if (aSpan.End > aGraph.Chain.size() || aSpan.Begin > aSpan.End || aTouches.size() != aSpan.size()) {
				return std::nullopt;
			}
			auto total = std::size_t{ 0U };
			for (std::size_t offset = 0U; offset < aTouches.size(); ++offset) {
				const auto& touch = aTouches[offset];
				if (touch.Commit != aGraph.Chain[aSpan.Begin + offset] || touch.Paths.size() > maximum_filter_commit_paths) {
					return std::nullopt;
				}
				for (std::size_t path = 0U; path < touch.Paths.size(); ++path) {
					if (!valid_filter_path(touch.Paths[path]) || (path != 0U && !(touch.Paths[path - 1U] < touch.Paths[path]))) {
						return std::nullopt;
					}
				}
				total += touch.Paths.size();
				if (total > maximum_filter_touched_paths) {
					return std::nullopt;
				}
			}
			return total;
		}
	} // namespace

	auto build_filter_commit_graph(const filter_commit_list& aList)
		-> std::expected<filter_commit_graph, commit_filter_unavailable>
	{
		if (aList.Truncated) {
			return std::unexpected{ commit_filter_unavailable::truncated };
		}
		if (aList.Commits.empty() || aList.Commits.size() > maximum_filter_commits || !aList.Head.valid()) {
			return std::unexpected{ commit_filter_unavailable::invalid_list };
		}
		const auto algorithm = aList.Head.algorithm();
		auto       index     = std::vector<listed_position>{ };
		index.reserve(aList.Commits.size());
		for (std::size_t listed = 0U; listed < aList.Commits.size(); ++listed) {
			const auto& commit = aList.Commits[listed];
			if (!commit.Oid.valid() || commit.Oid.algorithm() != algorithm) {
				return std::unexpected{ commit_filter_unavailable::invalid_list };
			}
			if (commit.Parents.size() > maximum_filter_commit_parents) {
				return std::unexpected{ commit_filter_unavailable::too_many_parents };
			}
			if (!std::ranges::all_of(commit.Parents, [algorithm](const git_commit_oid& aParent) { return aParent.valid() && aParent.algorithm() == algorithm; })) {
				return std::unexpected{ commit_filter_unavailable::invalid_list };
			}
			index.push_back({ .Oid = commit.Oid, .Listed = listed });
		}
		std::ranges::sort(index);
		if (std::ranges::adjacent_find(index, { }, &listed_position::Oid) != index.end()) {
			return std::unexpected{ commit_filter_unavailable::duplicate_commit };
		}
		auto head = find_listed(index, aList.Head);
		if (!head) {
			return std::unexpected{ commit_filter_unavailable::head_missing };
		}

		// The head's first-parent chain, newest first, while it stays listed.
		auto graph       = filter_commit_graph{ };
		graph.Head       = aList.Head;
		auto onChain     = std::vector<bool>(aList.Commits.size(), false);
		auto chainListed = std::vector<std::size_t>{ };
		for (auto current = head; current;) {
			if (onChain[*current]) {
				return std::unexpected{ commit_filter_unavailable::invalid_list };
			}
			onChain[*current] = true;
			chainListed.push_back(*current);
			const auto& commit = aList.Commits[*current];
			if (commit.Parents.empty()) {
				return std::unexpected{ commit_filter_unavailable::unrelated_history };
			}
			const auto parent = find_listed(index, commit.Parents.front());
			if (!parent) {
				graph.Fork = commit.Parents.front();
			}
			current = parent;
		}
		std::ranges::reverse(chainListed);
		graph.Chain.reserve(chainListed.size());
		auto chainOf = std::vector<std::optional<std::size_t>>(aList.Commits.size());
		for (std::size_t position = 0U; position < chainListed.size(); ++position) {
			graph.Chain.push_back(aList.Commits[chainListed[position]].Oid);
			chainOf[chainListed[position]] = position;
		}

		// Side commits sit under the oldest chain merge whose other parents
		// reach them through listed commits off the chain.
		auto mergeOf = std::vector<std::optional<std::size_t>>(aList.Commits.size());
		for (const auto listed : chainListed) {
			const auto& merge = aList.Commits[listed];
			if (merge.Parents.size() < 2U) {
				continue;
			}
			auto pending = std::vector<std::size_t>{ };
			for (auto parent = std::next(merge.Parents.begin()); parent != merge.Parents.end(); ++parent) {
				if (const auto found = find_listed(index, *parent); found && !onChain[*found] && !mergeOf[*found]) {
					mergeOf[*found] = listed;
					pending.push_back(*found);
				}
			}
			while (!pending.empty()) {
				const auto side = pending.back();
				pending.pop_back();
				for (const auto& parent : aList.Commits[side].Parents) {
					if (const auto found = find_listed(index, parent); found && !onChain[*found] && !mergeOf[*found]) {
						mergeOf[*found] = listed;
						pending.push_back(*found);
					}
				}
			}
		}

		graph.Entries.reserve(aList.Commits.size());
		graph.Headlines.reserve(aList.Commits.size());
		for (std::size_t listed = 0U; listed < aList.Commits.size(); ++listed) {
			const auto& commit = aList.Commits[listed];
			auto        entry  = filter_commit_entry{ };
			entry.Oid          = commit.Oid;
			entry.Listed       = listed;
			if (chainOf[listed]) {
				entry.Chain = chainOf[listed];
				if (commit.Parents.size() > 1U) {
					const auto foreign = std::any_of(std::next(commit.Parents.begin()), commit.Parents.end(),
													 [&index](const git_commit_oid& aParent) { return !find_listed(index, aParent); });
					entry.Role         = foreign ? filter_commit_role::base_merge : filter_commit_role::chain_merge;
				} else {
					entry.Role = filter_commit_role::chain;
				}
			} else {
				entry.Role = filter_commit_role::side;
				if (mergeOf[listed]) {
					entry.Merge = aList.Commits[*mergeOf[listed]].Oid;
				}
			}
			graph.Entries.push_back(entry);
			graph.Headlines.push_back(retained_headline(commit.Headline));
			graph.Authors.push_back(retained_headline(commit.Author, maximum_filter_author_bytes));
		}
		return graph;
	}

	auto find_filter_commit(const filter_commit_graph& aGraph, const git_commit_oid& aOid) noexcept
		-> const filter_commit_entry*
	{
		const auto found = std::ranges::find(aGraph.Entries, aOid, &filter_commit_entry::Oid);
		return found != aGraph.Entries.end() ? &*found : nullptr;
	}

	auto chain_parent(const filter_commit_graph& aGraph, const std::size_t aChain) noexcept -> git_commit_oid
	{
		if (aChain >= aGraph.Chain.size()) {
			return { };
		}
		return aChain == 0U ? aGraph.Fork : aGraph.Chain[aChain - 1U];
	}

	auto filter_span_for(const filter_commit_graph& aGraph, const filter_comparison& aComparison)
		-> std::expected<filter_commit_span, commit_filter_suspension>
	{
		if (aComparison.WholePullRequest) {
			return filter_commit_span{ .Begin = 0U, .End = aGraph.Chain.size() };
		}
		if (aComparison.Left == aComparison.Right) {
			return std::unexpected{ commit_filter_suspension::empty_comparison };
		}
		const auto right = chain_position(aGraph, aComparison.Right);
		const auto left  = chain_position(aGraph, aComparison.Left);
		if (!right) {
			// The fork as the right side of a chain commit is a reversed range.
			return std::unexpected{ aComparison.Right == aGraph.Fork && left ? commit_filter_suspension::reversed
																			 : commit_filter_suspension::not_first_parent_range };
		}
		if (aComparison.Left == aGraph.Fork) {
			return filter_commit_span{ .Begin = 0U, .End = *right + 1U };
		}
		if (!left) {
			return std::unexpected{ commit_filter_suspension::not_first_parent_range };
		}
		if (*left > *right) {
			return std::unexpected{ commit_filter_suspension::reversed };
		}
		return filter_commit_span{ .Begin = *left + 1U, .End = *right + 1U };
	}

	auto valid(const commit_filter_state& aState) noexcept -> bool
	{
		if (aState.Hidden.size() > maximum_filter_hidden_commits) {
			return false;
		}
		for (std::size_t index = 0U; index < aState.Hidden.size(); ++index) {
			const auto& entry = aState.Hidden[index];
			if (!entry.Oid.valid() || entry.Headline.size() > maximum_filter_headline_bytes || entry.Author.size() > maximum_filter_author_bytes
				|| (entry.Changes && !valid_changes(*entry.Changes, entry.Oid.algorithm()))
				|| (index != 0U && !(aState.Hidden[index - 1U].Oid < entry.Oid))) {
				return false;
			}
		}
		return true;
	}

	auto apply_commit_filter(const commit_filter_state& aState, const filter_commit_graph& aGraph,
							 const filter_commit_span& aSpan, const commit_filter_operation& aOperation)
		-> std::expected<commit_filter_state, commit_filter_refusal>
	{
		if (!valid(aState) || aSpan.End > aGraph.Chain.size() || aSpan.Begin > aSpan.End) {
			return std::unexpected{ commit_filter_refusal::invalid_state };
		}
		if (aOperation.Kind == commit_filter_operation_kind::clear) {
			return commit_filter_state{ };
		}
		if (aOperation.Selection.empty() || aOperation.Selection.size() > maximum_filter_commits) {
			return std::unexpected{ commit_filter_refusal::empty_selection };
		}

		// The selection, mapped to the chain commits it acts through.
		auto selected = std::vector<bool>(aGraph.Chain.size(), false);
		auto any      = false;
		for (const auto& oid : aOperation.Selection) {
			const auto* entry = find_filter_commit(aGraph, oid);
			if (entry == nullptr) {
				return std::unexpected{ commit_filter_refusal::unknown_commit };
			}
			const auto position = filtered_position(aGraph, *entry);
			if (position && aSpan.contains(*position)) {
				selected[*position] = true;
				any                 = true;
			}
		}
		if (!any) {
			return std::unexpected{ commit_filter_refusal::empty_selection };
		}

		// Membership after the gesture, for the chain commits the span covers.
		auto hidden = std::vector<bool>(aGraph.Chain.size(), false);
		for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
			hidden[position] = hidden_oid(aState, aGraph.Chain[position]);
		}
		switch (aOperation.Kind) {
		case commit_filter_operation_kind::show_only:
			for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
				hidden[position] = !selected[position];
			}
			break;
		case commit_filter_operation_kind::hide:
			for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
				hidden[position] = hidden[position] || selected[position];
			}
			break;
		case commit_filter_operation_kind::show: {
			auto reveals = false;
			for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
				reveals          = reveals || (selected[position] && hidden[position]);
				hidden[position] = hidden[position] && !selected[position];
			}
			if (!reveals) {
				return std::unexpected{ commit_filter_refusal::nothing_hidden };
			}
			break;
		}
		case commit_filter_operation_kind::clear:
			break;
		}

		// A merge from the base branch is the base's own work: it may stay
		// shown beside the pull request's commits, never alone.
		auto shownAny        = false;
		auto shownPullCommit = false;
		for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
			if (hidden[position]) {
				continue;
			}
			shownAny          = true;
			const auto* entry = find_filter_commit(aGraph, aGraph.Chain[position]);
			shownPullCommit   = shownPullCommit || (entry != nullptr && entry->Role != filter_commit_role::base_merge);
		}
		if (shownAny && !shownPullCommit) {
			return std::unexpected{ commit_filter_refusal::base_merge_alone };
		}

		// Entries the span does not cover, and gone ones, are kept as they were.
		auto result = commit_filter_state{ };
		for (const auto& entry : aState.Hidden) {
			const auto position = chain_position(aGraph, entry.Oid);
			if (!position || !aSpan.contains(*position)) {
				result.Hidden.push_back(entry);
			}
		}
		for (std::size_t position = aSpan.Begin; position < aSpan.End; ++position) {
			if (!hidden[position]) {
				continue;
			}
			const auto& oid      = aGraph.Chain[position];
			const auto  existing = std::ranges::find(aState.Hidden, oid, &filter_hidden_commit::Oid);
			result.Hidden.push_back(existing != aState.Hidden.end()
										? *existing
										: filter_hidden_commit{ .Oid = oid, .Headline = headline_of(aGraph, oid), .Author = author_of(aGraph, oid), .Changes = std::nullopt });
		}
		std::ranges::sort(result.Hidden, { }, &filter_hidden_commit::Oid);
		if (!valid(result)) {
			return std::unexpected{ commit_filter_refusal::invalid_state };
		}
		return result;
	}

	auto commit_filter_operation_available(const commit_filter_state& aState, const filter_commit_graph& aGraph,
										   const filter_commit_span& aSpan, const commit_filter_operation& aOperation)
		-> bool
	{
		return apply_commit_filter(aState, aGraph, aSpan, aOperation).has_value();
	}

	auto commit_state(const commit_filter_state& aState, const filter_commit_graph& aGraph,
					  const filter_commit_span& aSpan, const filter_commit_entry& aEntry) noexcept
		-> filter_commit_state
	{
		const auto position = filtered_position(aGraph, aEntry);
		if (!position || !aSpan.contains(*position) || *position >= aGraph.Chain.size()) {
			return filter_commit_state::outside_comparison;
		}
		return hidden_oid(aState, aGraph.Chain[*position]) ? filter_commit_state::hidden : filter_commit_state::shown;
	}

	auto summarize_commit_filter(const commit_filter_state&                                                        aState,
								 const std::expected<filter_commit_graph, commit_filter_unavailable>&              aGraph,
								 const std::optional<std::expected<filter_commit_span, commit_filter_suspension>>& aSpan)
		-> commit_filter_summary
	{
		auto summary = commit_filter_summary{ };
		if (!aGraph) {
			summary.Status      = commit_filter_status::unavailable;
			summary.Unavailable = aGraph.error();
			return summary;
		}
		const auto& graph = *aGraph;
		auto        named = false;
		for (const auto& entry : aState.Hidden) {
			if (find_filter_commit(graph, entry.Oid) == nullptr) {
				summary.Gone.push_back(entry);
			} else {
				named = true;
			}
		}
		if (!aSpan) {
			return summary;
		}
		if (!*aSpan) {
			summary.Suspension = aSpan->error();
			summary.Status     = named ? commit_filter_status::suspended : commit_filter_status::inactive;
			return summary;
		}
		auto applies = false;
		for (const auto& entry : graph.Entries) {
			const auto state = commit_state(aState, graph, **aSpan, entry);
			if (state == filter_commit_state::outside_comparison) {
				continue;
			}
			++summary.Spanned;
			if (state == filter_commit_state::shown) {
				++summary.Shown;
			} else {
				applies = true;
			}
		}
		summary.Status = applies ? commit_filter_status::active : commit_filter_status::inactive;
		return summary;
	}

	auto filter_paths(const commit_filter_state& aState, const filter_commit_graph& aGraph,
					  const filter_commit_span& aSpan, std::span<const filter_commit_touch> aTouches)
		-> std::expected<filtered_path_set, filtered_path_refusal>
	{
		const auto checked = checked_touches(aGraph, aSpan, aTouches);
		if (!valid(aState) || !checked) {
			return std::unexpected{ filtered_path_refusal::invalid_touches };
		}
		const auto total   = *checked;
		auto       shown   = std::vector<bool>(aTouches.size(), true);
		auto       applies = false;
		for (std::size_t offset = 0U; offset < aTouches.size(); ++offset) {
			shown[offset] = !hidden_oid(aState, aTouches[offset].Commit);
			applies       = applies || !shown[offset];
		}
		if (!applies) {
			return std::unexpected{ filtered_path_refusal::inactive };
		}

		// Every (path, commit) touch, grouped by path in chain order.
		auto touches = std::vector<std::pair<std::string_view, std::size_t>>{ };
		touches.reserve(total);
		for (std::size_t offset = 0U; offset < aTouches.size(); ++offset) {
			for (const auto& path : aTouches[offset].Paths) {
				touches.emplace_back(path, offset);
			}
		}
		std::ranges::sort(touches);

		const auto commit = [&](const std::size_t aOffset) { return aGraph.Chain[aSpan.Begin + aOffset]; };
		const auto parent = [&](const std::size_t aOffset) { return chain_parent(aGraph, aSpan.Begin + aOffset); };
		auto       result = filtered_path_set{ };
		for (auto begin = touches.begin(); begin != touches.end();) {
			const auto end = std::find_if(begin, touches.end(), [&](const auto& aTouch) { return aTouch.first != begin->first; });
			auto       all = std::vector<std::size_t>{ };
			all.reserve(static_cast<std::size_t>(end - begin));
			for (auto touch = begin; touch != end; ++touch) {
				all.push_back(touch->second);
			}
			const auto path = begin->first;
			begin           = end;

			const auto firstShown = std::ranges::find_if(all, [&](const std::size_t aOffset) { return shown[aOffset]; });
			if (firstShown == all.end()) {
				++result.Excluded;
				continue;
			}
			auto entry = filtered_path{ };
			entry.Path = std::string{ path };
			if (std::ranges::all_of(all, [&](const std::size_t aOffset) { return shown[aOffset]; })) {
				entry.Kind = filtered_path_kind::unchanged;
				result.Paths.push_back(std::move(entry));
				continue;
			}
			const auto lastShown = std::ranges::find_if(all | std::views::reverse, [&](const std::size_t aOffset) { return shown[aOffset]; });
			const auto first     = *firstShown;
			const auto last      = *lastShown;
			const auto split     = std::ranges::any_of(all, [&](const std::size_t aOffset) { return aOffset > first && aOffset < last && !shown[aOffset]; });
			if (!split) {
				entry.Kind  = filtered_path_kind::exact;
				entry.Left  = parent(first);
				entry.Right = commit(last);
				result.Paths.push_back(std::move(entry));
				continue;
			}
			entry.Kind = filtered_path_kind::split;
			for (const auto offset : all) {
				if (offset < first || offset > last) {
					continue;
				}
				if (shown[offset]) {
					entry.Segments.push_back({ .Left = parent(offset), .Right = commit(offset), .HiddenAfter = { } });
				} else {
					entry.Segments.back().HiddenAfter.push_back(commit(offset));
				}
			}
			result.Paths.push_back(std::move(entry));
		}
		return result;
	}

	auto record_hidden_changes(const commit_filter_state& aState, const filter_commit_graph& aGraph,
							   std::span<const filter_commit_changes> aChanges) -> commit_filter_state
	{
		auto result = aState;
		for (auto& entry : result.Hidden) {
			if (find_filter_commit(aGraph, entry.Oid) == nullptr) {
				continue;
			}
			if (entry.Author.empty()) {
				entry.Author = author_of(aGraph, entry.Oid);
			}
			if (!entry.Changes) {
				if (const auto* changes = changes_of(aChanges, entry.Oid); changes != nullptr) {
					entry.Changes = *changes;
				}
			}
		}
		return valid(result) ? result : aState;
	}

	auto rematch_offers(const commit_filter_state& aState, const filter_commit_graph& aGraph,
						std::span<const filter_commit_changes> aChanges) -> std::vector<filter_rematch_offer>
	{
		auto offers = std::vector<filter_rematch_offer>{ };
		if (!valid(aState)) {
			return offers;
		}
		for (const auto& gone : aState.Hidden) {
			if (find_filter_commit(aGraph, gone.Oid) != nullptr) {
				continue;
			}
			auto same    = std::vector<filter_rematch_offer>{ };
			auto guessed = std::vector<filter_rematch_offer>{ };
			for (const auto& candidate : aGraph.Chain) {
				const auto* entry = find_filter_commit(aGraph, candidate);
				if (entry == nullptr || entry->Role == filter_commit_role::base_merge || hidden_oid(aState, candidate)) {
					continue;
				}
				if (gone.Changes && !gone.Changes->empty()) {
					if (const auto* changes = changes_of(aChanges, candidate); changes != nullptr && *changes == *gone.Changes) {
						same.push_back({ .Gone = gone.Oid, .Candidate = candidate, .Basis = filter_rematch_basis::same_change });
						continue;
					}
				}
				if (!gone.Headline.empty() && !gone.Author.empty() && headline_of(aGraph, candidate) == gone.Headline && author_of(aGraph, candidate) == gone.Author) {
					guessed.push_back({ .Gone = gone.Oid, .Candidate = candidate, .Basis = filter_rematch_basis::same_headline_and_author });
				}
			}
			const auto& chosen = same.empty() ? guessed : same;
			offers.insert(offers.end(), chosen.begin(), chosen.end());
		}
		return offers;
	}

	auto apply_rematch(const commit_filter_state& aState, const filter_commit_graph& aGraph,
					   std::span<const filter_commit_changes> aChanges, const filter_rematch_offer& aOffer)
		-> std::expected<commit_filter_state, commit_filter_refusal>
	{
		if (!valid(aState)) {
			return std::unexpected{ commit_filter_refusal::invalid_state };
		}
		const auto offers = rematch_offers(aState, aGraph, aChanges);
		if (std::ranges::find(offers, aOffer) == offers.end()) {
			return std::unexpected{ commit_filter_refusal::not_offered };
		}
		auto result = commit_filter_state{ };
		for (const auto& entry : aState.Hidden) {
			if (entry.Oid != aOffer.Gone) {
				result.Hidden.push_back(entry);
			}
		}
		auto hidden     = filter_hidden_commit{ };
		hidden.Oid      = aOffer.Candidate;
		hidden.Headline = headline_of(aGraph, aOffer.Candidate);
		hidden.Author   = author_of(aGraph, aOffer.Candidate);
		if (const auto* changes = changes_of(aChanges, aOffer.Candidate); changes != nullptr) {
			hidden.Changes = *changes;
		}
		result.Hidden.push_back(std::move(hidden));
		std::ranges::sort(result.Hidden, { }, &filter_hidden_commit::Oid);
		if (!valid(result)) {
			return std::unexpected{ commit_filter_refusal::invalid_state };
		}
		return result;
	}

	auto combined_split_receipt(const file_identity& aFile, std::span<const std::optional<file_receipt>> aSegments)
		-> std::optional<file_receipt>
	{
		if (!aFile.valid() || std::ranges::none_of(aSegments, [](const std::optional<file_receipt>& aReceipt) { return aReceipt.has_value(); })) {
			return std::nullopt;
		}
		auto result     = file_receipt{ };
		result.File     = aFile;
		result.Read     = file_read_state::read;
		result.Accepted = true;
		result.Excluded = true;
		for (const auto& segment : aSegments) {
			if (!segment) {
				result.Read     = file_read_state::unread;
				result.Accepted = false;
				result.Excluded = false;
				continue;
			}
			if (!result.Material.valid()) {
				result.Material = segment->Material;
			}
			result.Read     = std::min(result.Read, segment->Read);
			result.Accepted = result.Accepted && segment->Accepted;
			result.Excluded = result.Excluded && segment->Excluded;
		}
		return result;
	}

	auto valid(const file_filter_state& aState) noexcept -> bool
	{
		if (aState.Hidden.size() > maximum_filter_touched_paths) {
			return false;
		}
		for (std::size_t index = 0U; index < aState.Hidden.size(); ++index) {
			if (!valid_filter_path(aState.Hidden[index]) || (index != 0U && !(aState.Hidden[index - 1U] < aState.Hidden[index]))) {
				return false;
			}
		}
		return true;
	}

	auto file_filter_hides(const file_filter_state& aState, const std::string_view aPath) noexcept -> bool
	{
		return std::ranges::binary_search(aState.Hidden, aPath, std::less<>{ });
	}

	auto hide_files_changed_by(const file_filter_state& aState, const filter_commit_graph& aGraph,
							   const filter_commit_span& aSpan, std::span<const filter_commit_touch> aTouches,
							   std::span<const git_commit_oid> aSelection)
		-> std::expected<file_filter_state, commit_filter_refusal>
	{
		if (!valid(aState) || !checked_touches(aGraph, aSpan, aTouches)) {
			return std::unexpected{ commit_filter_refusal::invalid_state };
		}
		if (aSelection.empty() || aSelection.size() > maximum_filter_commits) {
			return std::unexpected{ commit_filter_refusal::empty_selection };
		}
		auto selected = std::vector<bool>(aTouches.size(), false);
		auto any      = false;
		for (const auto& oid : aSelection) {
			const auto* entry = find_filter_commit(aGraph, oid);
			if (entry == nullptr) {
				return std::unexpected{ commit_filter_refusal::unknown_commit };
			}
			const auto position = filtered_position(aGraph, *entry);
			if (position && aSpan.contains(*position)) {
				selected[*position - aSpan.Begin] = true;
				any                               = true;
			}
		}
		if (!any) {
			return std::unexpected{ commit_filter_refusal::empty_selection };
		}
		auto result = aState;
		for (std::size_t offset = 0U; offset < aTouches.size(); ++offset) {
			if (selected[offset]) {
				result.Hidden.insert(result.Hidden.end(), aTouches[offset].Paths.begin(), aTouches[offset].Paths.end());
			}
		}
		std::ranges::sort(result.Hidden);
		const auto repeated = std::ranges::unique(result.Hidden);
		result.Hidden.erase(repeated.begin(), repeated.end());
		if (!valid(result)) {
			return std::unexpected{ commit_filter_refusal::invalid_state };
		}
		return result;
	}
} // namespace bpr::core
