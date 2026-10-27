#include "bpr/core/comment_edit.hpp"

#include <array>
#include <utility>

namespace bpr::core {
	auto name(const comment_edit_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case comment_edit_kind::review_comment:       return "review_comment";
		case comment_edit_kind::pull_request_comment: return "pull_request_comment";
		case comment_edit_kind::review_body:          return "review_body";
		}
		return { };
	}

	auto name(const comment_edit_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case comment_edit_refusal::none:      return "none";
		case comment_edit_refusal::not_mine:  return "not_mine";
		case comment_edit_refusal::unchanged: return "unchanged";
		case comment_edit_refusal::empty:     return "empty";
		case comment_edit_refusal::too_long:  return "too_long";
		}
		return { };
	}

	auto comment_edit_subject_of(const std::string_view aPost) noexcept -> std::optional<comment_edit_subject>
	{
		constexpr auto prefixes = std::array{
			std::pair{ std::string_view{ "review-comment:" }, comment_edit_kind::review_comment },
			std::pair{ std::string_view{ "issue-comment:" }, comment_edit_kind::pull_request_comment },
			std::pair{ std::string_view{ "review:" }, comment_edit_kind::review_body },
		};
		for (const auto& [prefix, kind] : prefixes) {
			if (aPost.starts_with(prefix) && aPost.size() > prefix.size()) {
				return comment_edit_subject{ .Kind = kind, .Node = aPost.substr(prefix.size()) };
			}
		}
		return std::nullopt;
	}

	auto comment_edit_offered(const std::string_view aPost, const bool aMine, const bool aAnonymous) noexcept -> bool
	{
		return aMine && !aAnonymous && comment_edit_subject_of(aPost).has_value();
	}

	auto decide_comment_edit(const comment_edit_input& aInput) noexcept -> comment_edit_refusal
	{
		if (!aInput.Mine || aInput.Anonymous) {
			return comment_edit_refusal::not_mine;
		}
		if (aInput.Edited == aInput.Original) {
			return comment_edit_refusal::unchanged;
		}
		if (aInput.Edited.find_first_not_of(" \t\r\n") == std::string_view::npos) {
			return comment_edit_refusal::empty;
		}
		if (description_characters(aInput.Edited) > maximum_comment_characters) {
			return comment_edit_refusal::too_long;
		}
		return comment_edit_refusal::none;
	}

	auto decide_comment_save(const description_save_observation& aObservation) noexcept -> description_save_outcome
	{
		return decide_description_save(aObservation);
	}
} // namespace bpr::core
