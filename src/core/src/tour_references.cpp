#include "bpr/core/tour_references.hpp"

#include <algorithm>

namespace bpr::core {
	auto rewrite_tour_references(const std::string_view aMarkdown, const agent_reference_files& aFiles, const tour_reference_form& aForm) -> tour_reference_rewrite
	{
		auto result = tour_reference_rewrite{ };
		auto at     = std::size_t{ 0U };
		for (const auto& found : agent_markdown_references(aMarkdown, false)) {
			const auto& reference = found.Reference;
			if (reference.Begin < at) {
				continue;
			}
			const auto written = std::string{ aMarkdown.substr(reference.Begin, reference.Length) };
			const auto paths   = resolve_agent_reference(reference.Path, aFiles);
			result.Text.append(aMarkdown.substr(at, reference.Begin - at));
			at = reference.Begin + reference.Length;
			if (paths.size() != 1U) {
				result.Text += written;
				if (std::ranges::find(result.Unresolved, written) == result.Unresolved.end()) {
					result.Unresolved.push_back(written);
				}
				continue;
			}
			const auto shown = aForm ? aForm(tour_text_reference{ .Written = written, .Path = paths.front(), .First = reference.First, .Last = std::max(reference.First, reference.Last), .Before = reference.Before, .Alone = found.Alone })
									 : std::nullopt;
			result.Text += shown.value_or(written);
		}
		result.Text.append(aMarkdown.substr(std::min(at, aMarkdown.size())));
		return result;
	}

	auto unresolved_tour_references(const std::string_view aMarkdown, const agent_reference_files& aFiles) -> std::vector<std::string>
	{
		return rewrite_tour_references(aMarkdown, aFiles, { }).Unresolved;
	}

	auto tour_reference_snippet_last(const tour_text_reference& aReference) noexcept -> std::uint32_t
	{
		return std::min(aReference.Last, aReference.First + tour_reference_snippet_lines - 1U);
	}
} // namespace bpr::core
