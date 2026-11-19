#include "bpr/core/tour_publication_head.hpp"

#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_update.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto blank(const std::string_view aValue) -> bool
		{
			return aValue.find_first_not_of(" \r\n\t") == std::string_view::npos;
		}

		[[nodiscard]] auto stop_text_changed(const tour_stop& aStop) -> bool
		{
			if (!aStop.Baseline) {
				return false;
			}
			if (const auto proposed = propose_tour_stop_text(aStop)) {
				return *proposed != aStop.Baseline->Raw;
			}
			return aStop.Baseline->DraftText && *aStop.Baseline->DraftText != aStop.Body;
		}
	}

	auto tour_stop_publishable(const review_tour& aTour, const tour_stop& aStop) noexcept -> bool
	{
		// [[BPR-2014]], RFC-BPR-0106: every stop Accepted, with its words and lines.
		return !blank(aStop.Title) && !blank(aStop.Body) && publication_source(aStop).has_value() && tour_stop_accepted(aTour, aStop);
	}

	auto tour_unpublished_changes(const review_tour& aTour) -> std::size_t
	{
		if (!aTour.Remote) {
			return 0U;
		}
		auto count = aTour.Retired.size();
		for (const auto& stop : aTour.Stops) {
			if (needs_tour_destination(stop) || stop_text_changed(stop)) {
				++count;
			}
		}
		if (count == 0U && std::ranges::none_of(aTour.Stops, needs_tour_destination)) {
			// What no part names on its own: the route reordered. The update
			// planner knows which published comments would change.
			if (const auto planned = plan_tour_update(aTour, { })) {
				count = planned->Edits.size();
			}
		}
		return count;
	}

	auto decide_tour_head(const review_tour* const aDraft, const tour_head_progress& aProgress) -> tour_head
	{
		using state  = tour_head_state;
		using action = tour_head_action;
		if (aDraft == nullptr || aDraft->ReadOnly) {
			return { };
		}
		auto head  = tour_head{ };
		head.Stops = aDraft->Stops.size();
		for (const auto& stop : aDraft->Stops) {
			if (!tour_stop_publishable(*aDraft, stop)) {
				++head.ToAccept;
			}
		}
		head.Step  = aProgress.Step;
		head.Steps = aProgress.Steps;
		if (aProgress.Phase == tour_publication_progress::publishing) {
			head.State   = state::publishing;
			head.Actions = { action::details };
			return head;
		}
		if (aProgress.Phase == tour_publication_progress::stopped) {
			head.State   = state::stopped;
			head.Actions = { action::resume, action::details };
			return head;
		}
		head.Step  = 0U;
		head.Steps = 0U;
		if (aDraft->Remote) {
			head.Changes = tour_unpublished_changes(*aDraft);
			head.State   = head.Changes == 0U ? state::published : state::published_changed;
			head.Actions = head.Changes == 0U ? std::vector{ action::on_github } : std::vector{ action::update, action::on_github };
			return head;
		}
		head.Missing         = !aDraft->Conflicts.empty() || aDraft->RemoteStale ? tour_head_missing::conflict
			: aDraft->Stops.empty()                                              ? tour_head_missing::stops
			: head.ToAccept > 0U                                                 ? tour_head_missing::acceptance
																				 : tour_head_missing::nothing;
		head.State           = head.Missing == tour_head_missing::nothing ? state::publishable : state::not_publishable;
		head.PublishDisabled = head.State == state::not_publishable;
		head.Actions         = { action::publish };
		return head;
	}

	auto tour_lead_sentence(const std::string_view aBody, const std::size_t aLimit) -> std::string
	{
		auto       text  = aBody;
		const auto begin = text.find_first_not_of(" \r\n\t");
		if (begin == std::string_view::npos) {
			return { };
		}
		text.remove_prefix(begin);
		auto end = text.find("\n\n");
		if (const auto crlf = text.find("\r\n\r\n"); crlf < end) {
			end = crlf;
		}
		for (std::size_t at = 0U; at + 1U < text.size() && at < end; ++at) {
			const auto c = text[at];
			if ((c == '.' || c == '?' || c == '!') && (text[at + 1U] == ' ' || text[at + 1U] == '\n' || text[at + 1U] == '\r')) {
				end = at + 1U;
				break;
			}
		}
		auto       sentence = std::string{ };
		const auto raw      = text.substr(0U, std::min(end, text.size()));
		// A row reads as words: emphasis and code marks go, their words stay.
		for (std::size_t at = 0U; at < raw.size(); ++at) {
			if (raw[at] == '`' || ((raw[at] == '*' || raw[at] == '_') && at + 1U < raw.size() && raw[at + 1U] == raw[at])) {
				at += raw[at] == '`' ? 0U : 1U;
				continue;
			}
			sentence.push_back(raw[at] == '\n' || raw[at] == '\r' || raw[at] == '\t' ? ' ' : raw[at]);
		}
		while (!sentence.empty() && sentence.back() == ' ') {
			sentence.pop_back();
		}
		if (sentence.size() > aLimit) {
			auto cut = aLimit;
			while (cut > 0U && (static_cast<unsigned char>(sentence[cut]) & 0xC0U) == 0x80U) {
				--cut;
			}
			sentence.resize(cut);
			while (!sentence.empty() && sentence.back() == ' ') {
				sentence.pop_back();
			}
			sentence += "\xE2\x80\xA6";
		}
		return sentence;
	}
} // namespace bpr::core
