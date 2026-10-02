#include "bpr/core/review_cycle_share.hpp"

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
		if (!saved && !aObservation.Destination.empty()) {
			return cycle_delivery_problem::unexpected_destination;
		}
		return std::nullopt;
	}
}
