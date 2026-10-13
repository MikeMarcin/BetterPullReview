#include "bpr/core/review_cycle_share.hpp"

#include "bpr/core/review_cycle_send.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		auto find_note(const std::span<const cycle_next_round_item> aNotes, const cycle_feedback_item_id aItem) -> const cycle_next_round_item*
		{
			const auto found = std::ranges::find(aNotes, aItem, &cycle_next_round_item::Item);
			return found == aNotes.end() ? nullptr : &*found;
		}

		auto included(const cycle_round_selection& aSelection, const cycle_feedback_item_id aItem) -> bool
		{
			return std::ranges::find(aSelection.Items, aItem, &cycle_round_item_selection::Item) != aSelection.Items.end();
		}

		auto include(const cycle_round_selection& aSelection, const std::span<const cycle_next_round_item> aNotes, const include_round_item& aEdit) -> std::optional<cycle_round_selection>
		{
			if (aEdit.Include == included(aSelection, aEdit.Item)) {
				return std::nullopt;
			}
			auto next = aSelection;
			if (!aEdit.Include) {
				std::erase_if(next.Items, [&](const cycle_round_item_selection& aSelected) { return aSelected.Item == aEdit.Item; });
				for (auto& selected : next.Items) {
					std::erase(selected.Related, aEdit.Item);
				}
				return next;
			}
			const auto* note = find_note(aNotes, aEdit.Item);
			if (note == nullptr || note->Lifecycle != private_thread_lifecycle::open) {
				return std::nullopt;
			}
			next.Items.push_back({ .Item = note->Item, .Wording = note->Wording, .Suggestions = note->Suggestions });
			return next;
		}

		auto move(const cycle_round_selection& aSelection, const move_round_item& aEdit) -> std::optional<cycle_round_selection>
		{
			const auto at = std::ranges::find(aSelection.Items, aEdit.Item, &cycle_round_item_selection::Item);
			if (at == aSelection.Items.end()) {
				return std::nullopt;
			}
			const auto index = static_cast<std::size_t>(at - aSelection.Items.begin());
			if (aEdit.Earlier ? index == 0U : index + 1U >= aSelection.Items.size()) {
				return std::nullopt;
			}
			auto next = aSelection;
			std::swap(next.Items[index], next.Items[aEdit.Earlier ? index - 1U : index + 1U]);
			return next;
		}
	}

	auto stable_name(const cycle_share_route aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_share_route::copy_document:  return "copy_document";
		case cycle_share_route::save_document:  return "save_document";
		case cycle_share_route::save_bundle:    return "save_bundle";
		case cycle_share_route::external_agent: return "external_agent";
		case cycle_share_route::send_to_agent:  return "send_to_agent";
		}
		return "copy_document";
	}

	auto share_channel(const cycle_share_route aRoute) noexcept -> cycle_disclosure_channel
	{
		switch (aRoute) {
		case cycle_share_route::copy_document:  return cycle_disclosure_channel::clipboard;
		case cycle_share_route::save_document:
		case cycle_share_route::save_bundle:    return cycle_disclosure_channel::local_file;
		case cycle_share_route::external_agent: return cycle_disclosure_channel::external_retrieval;
		case cycle_share_route::send_to_agent:  return cycle_disclosure_channel::agent_adapter;
		}
		return cycle_disclosure_channel::agent_adapter;
	}

	auto share_routes(const cycle_disclosure_policy& aPolicy, const cycle_workspace_origin& aOrigin, const cycle_share_capabilities& aCapabilities) -> std::vector<cycle_share_route_offer>
	{
		auto routes = std::vector<cycle_share_route>{ cycle_share_route::copy_document, cycle_share_route::save_document, cycle_share_route::save_bundle };
		if (aCapabilities.ExternalInterface) {
			routes.push_back(cycle_share_route::external_agent);
		}
		if (aCapabilities.Adapter) {
			routes.push_back(cycle_share_route::send_to_agent);
		}
		auto offers = std::vector<cycle_share_route_offer>{ };
		offers.reserve(routes.size());
		for (const auto route : routes) {
			offers.push_back({ .Route = route, .Decision = channel_permission(aPolicy, aOrigin.Classification, share_channel(route)) });
		}
		return offers;
	}

	auto cycle_origin(const review_cycle_state& aCycle) noexcept -> cycle_workspace_origin
	{
		if (!aCycle.Workspace) {
			return { };
		}
		switch (aCycle.Workspace->Root.Kind) {
		case workspace_root_kind::folder:
		case workspace_root_kind::file:                return { .Origin = cycle_source_origin::local_folder };
		case workspace_root_kind::git_checkout:
		case workspace_root_kind::git_linked_worktree: return { .Origin = cycle_source_origin::local_git_checkout };
		}
		return { };
	}

	auto first_composition(const std::span<const cycle_next_round_item> aNotes, const cycle_source_revision_id aSource) -> cycle_round_selection
	{
		// The same plan as a next round, with nothing to carry.
		return plan_next_round({ }, aNotes, aSource).Selection;
	}

	auto edit_composition(const cycle_round_selection& aSelection, const std::span<const cycle_next_round_item> aNotes, const cycle_compose_edit& aEdit)
		-> std::optional<cycle_round_selection>
	{
		if (const auto* change = std::get_if<include_round_item>(&aEdit)) {
			return include(aSelection, aNotes, *change);
		}
		if (const auto* change = std::get_if<move_round_item>(&aEdit)) {
			return move(aSelection, *change);
		}
		auto next = aSelection;
		if (const auto* change = std::get_if<set_round_action>(&aEdit)) {
			next.Action = change->Action;
		} else if (const auto* goal = std::get_if<set_round_goal>(&aEdit)) {
			next.Goal = goal->Goal;
		} else if (const auto* context = std::get_if<include_round_context>(&aEdit)) {
			// The same piece twice is one piece; freezing checks it against its revision.
			if (std::ranges::find(next.Context, context->Context) == next.Context.end()) {
				next.Context.push_back(context->Context);
			}
		} else {
			std::erase_if(next.Context, [&](const cycle_context_selection& aSelected) { return aSelected.Path == std::get<exclude_round_context>(aEdit).Path; });
		}
		return next == aSelection ? std::nullopt : std::optional{ std::move(next) };
	}

	auto refresh_composition(const cycle_round_selection& aSelection, const std::span<const cycle_next_round_item> aNotes) -> std::optional<cycle_round_selection>
	{
		auto next = aSelection;
		for (auto& selected : next.Items) {
			const auto* note = find_note(aNotes, selected.Item);
			if (note == nullptr || note->Lifecycle != private_thread_lifecycle::open) {
				continue;
			}
			selected.Wording = note->Wording;
			for (auto& pin : selected.Suggestions) {
				const auto current = std::ranges::find(note->Suggestions, pin.Id, &cycle_round_suggestion_pin::Id);
				if (current != note->Suggestions.end()) {
					pin.Revision = current->Revision;
				}
			}
		}
		return next == aSelection ? std::nullopt : std::optional{ std::move(next) };
	}

	auto stable_name(const cycle_delivery_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_delivery_problem::wrong_round:            return "wrong_round";
		case cycle_delivery_problem::wrong_channel:          return "wrong_channel";
		case cycle_delivery_problem::output_required:        return "output_required";
		case cycle_delivery_problem::destination_not_a_name: return "destination_not_a_name";
		case cycle_delivery_problem::unexpected_destination: return "unexpected_destination";
		}
		return "wrong_round";
	}

	auto delivery_destination_name(const std::string_view aName) noexcept -> bool
	{
		if (aName.empty() || aName.size() > maximum_delivery_name_bytes || aName == "." || aName == "..") {
			return false;
		}
		return std::ranges::none_of(aName, [](const char aSymbol) {
			const auto byte = static_cast<unsigned char>(aSymbol);
			return byte < 0x20U || byte == 0x7FU || aSymbol == '/' || aSymbol == '\\' || aSymbol == ':';
		});
	}

	auto admit_delivery(const local_cycle_subject& aSubject, const cycle_round& aRound, const cycle_delivery_observation& aObservation) -> std::optional<cycle_delivery_problem>
	{
		if (!belongs_to(aRound.Id, aSubject) || aObservation.Round != aRound.Id || aObservation.Digest != aRound.Digest) {
			return cycle_delivery_problem::wrong_round;
		}
		using enum cycle_disclosure_channel;
		const auto channel = aObservation.Channel;
		auto       fits    = true;
		auto       output  = true;
		switch (aObservation.State) {
		case cycle_delivery_state::copied:              fits = channel == clipboard; break;
		case cycle_delivery_state::saved:               fits = channel == local_file; break;
		case cycle_delivery_state::retrieved:           fits = channel == external_retrieval || channel == terminal_output; break;
		case cycle_delivery_state::accepted_by_adapter: fits = channel == agent_adapter; break;
		case cycle_delivery_state::recipient_acknowledged:
			fits   = channel == agent_adapter || channel == external_retrieval;
			output = false;
			break;
		case cycle_delivery_state::failed:
		case cycle_delivery_state::uncertain: output = false; break;
		}
		if (!fits) {
			return cycle_delivery_problem::wrong_channel;
		}
		if (output && !aObservation.Output) {
			return cycle_delivery_problem::output_required;
		}
		const auto saved = aObservation.State == cycle_delivery_state::saved;
		if (saved && !delivery_destination_name(aObservation.Destination)) {
			return cycle_delivery_problem::destination_not_a_name;
		}
		// [[BPR-1180]]. An adapter's acknowledgment may name the session its
		// harness reported, and nothing else.
		const auto session = aObservation.State == cycle_delivery_state::recipient_acknowledged && channel == agent_adapter && parse_send_session_record(aObservation.Destination);
		if (!saved && !session && !aObservation.Destination.empty()) {
			return cycle_delivery_problem::unexpected_destination;
		}
		return std::nullopt;
	}
}

namespace bpr::core {
	namespace {
		/// Display lines as `project_lines` counts them: LF and CRLF each end
		/// one. `Ends` excludes the ending; `Nexts` includes it.
		struct display_lines
		{
			std::vector<std::uint64_t> Starts;
			std::vector<std::uint64_t> Ends;
			std::vector<std::uint64_t> Nexts;
		};

		auto display_lines_of(const std::span<const std::uint8_t> aText) -> display_lines
		{
			auto lines = display_lines{ };
			auto start = std::uint64_t{ 0 };
			for (std::size_t index = 0; index < aText.size(); ++index) {
				// [[BPR-1432]]. Only LF ends a line; a lone CR is content.
				if (aText[index] != '\n') {
					continue;
				}
				const auto crlf = index > start && aText[index - 1U] == '\r';
				lines.Starts.push_back(start);
				lines.Ends.push_back(crlf ? index - 1U : index);
				lines.Nexts.push_back(index + 1U);
				start = index + 1U;
			}
			lines.Starts.push_back(start);
			lines.Ends.push_back(aText.size());
			lines.Nexts.push_back(aText.size());
			return lines;
		}

		auto text_between(const std::span<const std::uint8_t> aText, const std::uint64_t aBegin, const std::uint64_t aEnd) -> std::string
		{
			return { reinterpret_cast<const char*>(aText.data()) + aBegin, static_cast<std::size_t>(aEnd - aBegin) };
		}

		/// One suggested change placed on the file's lines: it replaces lines
		/// [`First`, `First` + `Removed`) with `Added`.
		struct placed_change
		{
			std::uint64_t            First   = 0;
			std::uint64_t            Removed = 0;
			std::vector<std::string> Added;
		};
	}

	auto excerpt_note(const std::span<const std::uint8_t> aFile, const std::optional<cycle_byte_range> aNoted, const std::span<const cycle_excerpt_change> aChanges, const std::size_t aContext,
					  const std::size_t aRowLimit) -> std::optional<cycle_note_excerpt>
	{
		if (aNoted && (aNoted->Start > aNoted->End || aNoted->End > aFile.size())) {
			return std::nullopt;
		}
		const auto lines  = display_lines_of(aFile);
		const auto count  = static_cast<std::uint64_t>(lines.Starts.size());
		const auto lineAt = [&](const std::uint64_t aOffset) {
			return static_cast<std::uint64_t>(std::ranges::upper_bound(lines.Starts, aOffset) - lines.Starts.begin()) - 1U;
		};
		auto result = cycle_note_excerpt{ };
		// The empty line after a final line ending is no line of the file.
		result.FileLines = count > 1U && lines.Starts.back() == aFile.size() ? count - 1U : count;

		// Each part is a run of lines [first, last + 1) that the excerpt shows with its context.
		auto parts = std::vector<std::pair<std::uint64_t, std::uint64_t>>{ };
		if (aNoted) {
			const auto first = lineAt(aNoted->Start);
			const auto last  = aNoted->Start == aNoted->End ? first : lineAt(aNoted->End - 1U);
			result.Lines     = cycle_line_span{ .First = first + 1U, .Last = last + 1U };
			parts.emplace_back(first, last + 1U);
		}
		auto ordered = std::vector<const cycle_excerpt_change*>{ };
		for (const auto& change : aChanges) {
			ordered.push_back(&change);
		}
		std::ranges::stable_sort(ordered, [](const cycle_excerpt_change* aLeft, const cycle_excerpt_change* aRight) { return std::pair{ aLeft->Start, aLeft->End } < std::pair{ aRight->Start, aRight->End }; });
		auto changes = std::vector<placed_change>{ };
		auto taken   = std::uint64_t{ 0 };
		for (const auto* change : ordered) {
			const auto holds = change->Start <= change->End && change->End <= aFile.size() && text_between(aFile, change->Start, change->End) == change->Original;
			if (!holds) {
				++result.Unplaced;
				continue;
			}
			const auto  first       = lineAt(change->Start);
			const auto  last        = change->Start == change->End ? first : lineAt(change->End - 1U);
			const auto& replacement = change->Replacement;
			// Whole lines inserted at a line start replace nothing.
			const auto wholeLines = change->Start == change->End && change->Start == lines.Starts[first] && !replacement.empty()
				&& (replacement.back() == '\n' || replacement.back() == '\r');
			const auto removed = wholeLines ? std::uint64_t{ 0 } : last - first + 1U;
			if (!changes.empty() && first < taken) {
				++result.Unplaced;
				continue;
			}
			const auto edited      = wholeLines ? replacement
												: text_between(aFile, lines.Starts[first], change->Start) + replacement + text_between(aFile, change->End, lines.Nexts[last]);
			const auto editedBytes = std::span{ reinterpret_cast<const std::uint8_t*>(edited.data()), edited.size() };
			const auto editedLines = display_lines_of(editedBytes);
			auto       added       = std::vector<std::string>{ };
			for (std::size_t index = 0; index < editedLines.Starts.size(); ++index) {
				// The empty line after the edited text's final ending is not one of its lines.
				if (index + 1U == editedLines.Starts.size() && editedLines.Starts[index] == edited.size()) {
					break;
				}
				added.push_back(text_between(editedBytes, editedLines.Starts[index], editedLines.Ends[index]));
			}
			changes.push_back({ .First = first, .Removed = removed, .Added = std::move(added) });
			taken = first + removed;
			parts.emplace_back(first, first + removed);
			++result.Changes;
		}

		// Each part with its context, joined where they meet or one line apart.
		auto end = result.FileLines;
		for (const auto& part : parts) {
			end = std::max(end, part.second);
		}
		std::ranges::sort(parts);
		auto windows = std::vector<std::pair<std::uint64_t, std::uint64_t>>{ };
		for (const auto& [first, last] : parts) {
			const auto begin = first > aContext ? first - aContext : 0U;
			const auto stop  = std::min<std::uint64_t>(end, last + aContext);
			if (!windows.empty() && begin <= windows.back().second + 1U) {
				windows.back().second = std::max(windows.back().second, stop);
			} else {
				windows.emplace_back(begin, stop);
			}
		}
		if (windows.empty() && result.FileLines > 0U) {
			// A whole-file note with no change: the file's opening lines.
			windows.emplace_back(0U, std::min<std::uint64_t>(result.FileLines, 2U * aContext + 1U));
		}
		const auto noted = [&](const std::uint64_t aLine) {
			return result.Lines && aLine + 1U >= result.Lines->First && aLine + 1U <= result.Lines->Last;
		};
		const auto push = [&](cycle_excerpt_row aRow) {
			if (result.Rows.size() >= aRowLimit) {
				result.Cut = true;
				return false;
			}
			result.Rows.push_back(std::move(aRow));
			return true;
		};
		auto next = changes.begin();
		for (std::size_t index = 0; index < windows.size(); ++index) {
			const auto [begin, stop] = windows[index];
			if (index > 0U && !push({ .Kind = cycle_excerpt_row_kind::omitted, .Omitted = begin - windows[index - 1U].second })) {
				return result;
			}
			for (auto line = begin; line < stop;) {
				if (next != changes.end() && next->First == line) {
					for (std::uint64_t removed = 0; removed < next->Removed; ++removed) {
						const auto at = line + removed;
						if (!push({ .Kind = cycle_excerpt_row_kind::removed, .Line = at + 1U, .Text = text_between(aFile, lines.Starts[at], lines.Ends[at]) })) {
							return result;
						}
					}
					for (std::size_t added = 0; added < next->Added.size(); ++added) {
						if (!push({ .Kind = cycle_excerpt_row_kind::added, .Line = line + 1U + added, .Text = next->Added[added] })) {
							return result;
						}
					}
					line += next->Removed;
					++next;
					continue;
				}
				if (line >= count) {
					break;
				}
				if (!push({ .Kind = noted(line) ? cycle_excerpt_row_kind::noted : cycle_excerpt_row_kind::context, .Line = line + 1U, .Text = text_between(aFile, lines.Starts[line], lines.Ends[line]) })) {
					return result;
				}
				++line;
			}
		}
		return result;
	}
}
