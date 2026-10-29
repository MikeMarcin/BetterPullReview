#include "bpr/core/tour_stop_view.hpp"

#include <algorithm>

namespace bpr::core {
	auto stable_name(const tour_stop_view_mode aMode) noexcept -> std::string_view
	{
		switch (aMode) {
		case tour_stop_view_mode::read: return "read";
		case tour_stop_view_mode::edit: return "edit";
		}
		return "unknown";
	}

	auto stable_name(const tour_stop_view_event aEvent) noexcept -> std::string_view
	{
		switch (aEvent) {
		case tour_stop_view_event::open:        return "open";
		case tour_stop_view_event::begin_edit:  return "begin_edit";
		case tour_stop_view_event::finish_edit: return "finish_edit";
		}
		return "unknown";
	}

	auto tour_stop_unwritten(const tour_stop& aStop) noexcept -> bool
	{
		return std::ranges::all_of(aStop.Body, [](const char aCharacter) { return aCharacter == ' ' || aCharacter == '\t' || aCharacter == '\n' || aCharacter == '\r' || aCharacter == '\f' || aCharacter == '\v'; });
	}

	auto tour_stop_text_unsaved(const tour_stop_view_input& aInput) noexcept -> bool
	{
		if (aInput.Stop == nullptr) {
			return false;
		}
		if (aInput.Composing) {
			return true;
		}
		return aInput.Held && (aInput.Held->Title != aInput.Stop->Title || aInput.Held->Body != aInput.Stop->Body);
	}

	auto decide_tour_stop_view(const tour_stop_view& aCurrent, const tour_stop_view_input& aInput) noexcept -> tour_stop_view
	{
		if (aInput.Stop == nullptr) {
			return { };
		}
		const auto id = aInput.Stop->Id;
		if (!aInput.Editable) {
			return { .Stop = id, .Mode = tour_stop_view_mode::read };
		}
		if (tour_stop_text_unsaved(aInput)) {
			return { .Stop = id, .Mode = tour_stop_view_mode::edit };
		}
		switch (aInput.Event) {
		case tour_stop_view_event::open:
			if (aCurrent.Stop == id) {
				return aCurrent;
			}
			return { .Stop = id, .Mode = tour_stop_unwritten(*aInput.Stop) ? tour_stop_view_mode::edit : tour_stop_view_mode::read };
		case tour_stop_view_event::begin_edit:  return { .Stop = id, .Mode = tour_stop_view_mode::edit };
		case tour_stop_view_event::finish_edit: return { .Stop = id, .Mode = tour_stop_view_mode::read };
		}
		return { .Stop = id, .Mode = tour_stop_view_mode::read };
	}
} // namespace bpr::core
