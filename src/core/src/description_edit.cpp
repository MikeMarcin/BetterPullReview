#include "bpr/core/description_edit.hpp"

#include "bpr/core/review_role.hpp"

#include <algorithm>

namespace bpr::core {
	auto name(const description_edit_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case description_edit_refusal::none:          return "none";
		case description_edit_refusal::not_author:    return "not_author";
		case description_edit_refusal::unchanged:     return "unchanged";
		case description_edit_refusal::too_long:      return "too_long";
		case description_edit_refusal::not_permitted: return "not_permitted";
		case description_edit_refusal::conflict:      return "conflict";
		}
		return { };
	}

	auto name(const description_save_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case description_save_outcome::write:         return "write";
		case description_save_outcome::already_saved: return "already_saved";
		case description_save_outcome::not_permitted: return "not_permitted";
		case description_save_outcome::conflict:      return "conflict";
		}
		return { };
	}

	auto description_characters(const std::string_view aText) noexcept -> std::size_t
	{
		// A continuation byte is 10xxxxxx; every other byte begins a code point.
		return static_cast<std::size_t>(std::ranges::count_if(aText, [](const char aByte) {
			return (static_cast<unsigned char>(aByte) & 0xC0U) != 0x80U;
		}));
	}

	auto description_edit_offered(const std::string_view aAuthor, const std::string_view aViewer) noexcept -> bool
	{
		return !aAuthor.empty() && !aViewer.empty() && same_login(aAuthor, aViewer);
	}

	auto decide_description_edit(const description_edit_input& aInput) noexcept -> description_edit_refusal
	{
		if (!description_edit_offered(aInput.Author, aInput.Viewer)) {
			return description_edit_refusal::not_author;
		}
		if (aInput.Edited == aInput.Original) {
			return description_edit_refusal::unchanged;
		}
		if (description_characters(aInput.Edited) > maximum_description_characters) {
			return description_edit_refusal::too_long;
		}
		return description_edit_refusal::none;
	}

	auto decide_description_save(const description_save_observation& aObservation) noexcept -> description_save_outcome
	{
		if (aObservation.Current == aObservation.Edited) {
			return description_save_outcome::already_saved;
		}
		if (!aObservation.ViewerCanUpdate) {
			return description_save_outcome::not_permitted;
		}
		if (aObservation.Current != aObservation.Original) {
			return description_save_outcome::conflict;
		}
		return description_save_outcome::write;
	}
} // namespace bpr::core
