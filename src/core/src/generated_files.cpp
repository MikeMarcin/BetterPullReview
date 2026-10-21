#include "bpr/core/generated_files.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string_view>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto says_something(const generated_file_mark& aMark) noexcept -> bool
		{
			return aMark.Mark != generated_mark::none || aMark.ReviewAnyway;
		}

		[[nodiscard]] auto attribute_decision(const git_attribute_state& aAttribute) noexcept -> generated_decision
		{
			auto decision = generated_decision{ };
			switch (aAttribute.Kind) {
			case git_attribute_state::kind::unspecified:
				break;
			case git_attribute_state::kind::set:
				decision.Generated = true;
				decision.Reason    = generated_reason::attribute_set;
				break;
			case git_attribute_state::kind::unset:
				decision.Reason = generated_reason::attribute_unset;
				break;
			case git_attribute_state::kind::value:
				decision.Generated = aAttribute.Value != "false";
				decision.Reason    = decision.Generated ? generated_reason::attribute_set : generated_reason::attribute_false;
				break;
			}
			return decision;
		}

		/// \brief The mark a gesture leaves, from the file's current one.
		[[nodiscard]] constexpr auto gestured(generated_file_mark aMark, const generated_gesture aGesture) noexcept
			-> generated_file_mark
		{
			switch (aGesture) {
			case generated_gesture::show_as_normal:
				aMark.Mark         = generated_mark::show_as_normal;
				aMark.ReviewAnyway = false;
				break;
			case generated_gesture::treat_as_generated:
				aMark.Mark         = generated_mark::treat_as_generated;
				aMark.ReviewAnyway = false;
				break;
			case generated_gesture::review_anyway:
				aMark.ReviewAnyway = true;
				break;
			case generated_gesture::set_aside_again:
				aMark.ReviewAnyway = false;
				break;
			case generated_gesture::follow_attributes:
				aMark.Mark         = generated_mark::none;
				aMark.ReviewAnyway = false;
				break;
			}
			return aMark;
		}
	} // namespace

	auto valid(const generated_file_marks& aMarks) noexcept -> bool
	{
		if (aMarks.Marks.size() > maximum_generated_file_marks) {
			return false;
		}
		for (std::size_t index = 0U; index < aMarks.Marks.size(); ++index) {
			const auto& mark = aMarks.Marks[index];
			if (!mark.File.valid() || !says_something(mark) || mark.Mark > generated_mark::treat_as_generated
				|| (index != 0U && !(aMarks.Marks[index - 1U].File < mark.File))) {
				return false;
			}
		}
		return true;
	}

	auto find_generated_mark(const generated_file_marks& aMarks, const file_identity& aFile) noexcept
		-> const generated_file_mark*
	{
		const auto found = std::ranges::lower_bound(aMarks.Marks, aFile, { }, &generated_file_mark::File);
		return found != aMarks.Marks.end() && found->File == aFile ? &*found : nullptr;
	}

	auto apply_generated_gesture(const generated_file_marks& aMarks, const file_identity& aFile,
								 const generated_gesture aGesture) -> std::expected<generated_file_marks, generated_mark_refusal>
	{
		if (!aFile.valid()) {
			return std::unexpected{ generated_mark_refusal::invalid_file };
		}
		if (!valid(aMarks)) {
			return std::unexpected{ generated_mark_refusal::invalid_state };
		}
		const auto at      = std::ranges::lower_bound(aMarks.Marks, aFile, { }, &generated_file_mark::File);
		const auto found   = at != aMarks.Marks.end() && at->File == aFile;
		const auto current = found ? *at : generated_file_mark{ .File = aFile, .Mark = generated_mark::none, .ReviewAnyway = false };
		const auto next    = gestured(current, aGesture);
		const auto offset  = at - aMarks.Marks.begin();
		auto       result  = aMarks;
		if (next == current) {
			return result;
		}
		if (!says_something(next)) {
			result.Marks.erase(result.Marks.begin() + offset);
		} else if (found) {
			result.Marks[static_cast<std::size_t>(offset)] = next;
		} else {
			if (result.Marks.size() >= maximum_generated_file_marks) {
				return std::unexpected{ generated_mark_refusal::too_many_marks };
			}
			result.Marks.insert(result.Marks.begin() + offset, next);
		}
		return result;
	}

	auto decide_generated(const generated_file_mark* aMark, const git_attribute_state& aAttribute) noexcept
		-> generated_decision
	{
		auto decision = generated_decision{ };
		if (aMark != nullptr && aMark->Mark == generated_mark::show_as_normal) {
			decision.Reason = generated_reason::marked_normal;
		} else if (aMark != nullptr && aMark->Mark == generated_mark::treat_as_generated) {
			decision.Generated = true;
			decision.Reason    = generated_reason::marked_generated;
		} else {
			decision = attribute_decision(aAttribute);
		}
		decision.ReviewAnyway = decision.Generated && aMark != nullptr && aMark->ReviewAnyway;
		decision.SetAside     = decision.Generated && !decision.ReviewAnyway;
		return decision;
	}

	auto generated_gesture_available(const generated_decision& aDecision, const generated_file_mark* aMark,
									 const generated_gesture aGesture) noexcept -> bool
	{
		const auto mark = aMark != nullptr ? aMark->Mark : generated_mark::none;
		switch (aGesture) {
		case generated_gesture::show_as_normal:     return aDecision.Generated;
		case generated_gesture::treat_as_generated: return !aDecision.Generated;
		case generated_gesture::review_anyway:      return aDecision.SetAside;
		case generated_gesture::set_aside_again:    return aDecision.ReviewAnyway;
		case generated_gesture::follow_attributes:  return mark != generated_mark::none;
		}
		return false;
	}

	auto decide_generated_files(std::span<const std::string> aPaths, const generated_file_marks& aMarks,
								const generated_attribute_rules* aRules) -> std::vector<generated_decision>
	{
		const auto marks     = valid(aMarks) ? std::span{ aMarks.Marks } : std::span<const generated_file_mark>{ };
		auto       decisions = std::vector<generated_decision>{ };
		decisions.reserve(aPaths.size());
		for (const auto& path : aPaths) {
			const auto  file      = file_identity_of(path);
			const auto  found     = std::ranges::lower_bound(marks, file, { }, &generated_file_mark::File);
			const auto* mark      = file.valid() && found != marks.end() && found->File == file ? &*found : nullptr;
			const auto  attribute = aRules != nullptr ? aRules->linguist_generated(path) : git_attribute_state{ };
			decisions.push_back(decide_generated(mark, attribute));
		}
		return decisions;
	}

	auto generated_attribute_directories(std::span<const std::string> aPaths) -> std::vector<std::string>
	{
		auto directories = std::vector<std::string>{ std::string{ } };
		for (const auto& path : aPaths) {
			for (auto slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1U)) {
				if (slash != 0U) {
					directories.emplace_back(path.substr(0U, slash));
				}
			}
		}
		// A parent is a prefix of its children, so it sorts before them.
		std::ranges::sort(directories);
		const auto repeated = std::ranges::unique(directories);
		directories.erase(repeated.begin(), repeated.end());
		return directories;
	}
} // namespace bpr::core
