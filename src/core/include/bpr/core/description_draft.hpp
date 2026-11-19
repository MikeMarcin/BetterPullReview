#ifndef BPR_CORE_DESCRIPTION_DRAFT_HPP
#define BPR_CORE_DESCRIPTION_DRAFT_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2281]]. Draft with agent beside a pull request's description
/// in Author (owner, 2026-11-19: "I would put it near the description field
/// in Author"; "it's one of those system editable prompts").
///
/// \details The run's instructions are the Description writing system prompt
/// (the profile's or the shipped one) and travel apart, as the agent's system
/// prompt; this is the run's material: the pull request's title, its current
/// description and its change, bounded. The answer is the description's
/// Markdown, which the author reads in the editor and saves; nothing here is
/// sent anywhere.
namespace bpr::core {
	/// \brief The most bytes of the change one draft carries; the rest of the
	/// files are named and said to be left out.
	inline constexpr std::size_t maximum_description_draft_diff_bytes = 192U * 1024U;
	/// \brief The most bytes of the current description one draft carries.
	inline constexpr std::size_t maximum_description_draft_body_bytes = 16U * 1024U;
	/// \brief The first line of every draft's material, by which it is known.
	inline constexpr std::string_view description_draft_marker = "Pull request description draft (bpr.description-draft.v1)";

	/// \brief One changed file, as the accepted read gave it.
	struct description_draft_file
	{
		std::string Path;
		/// added, modified, removed or renamed.
		std::string Change;
		/// GitHub's unified patch at the head; none when GitHub sent none.
		std::optional<std::string> Patch;

		auto operator==(const description_draft_file&) const -> bool = default;
	};

	/// \brief What one draft is asked about.
	struct description_draft_material
	{
		std::string                         Title;
		std::string                         Body;
		std::vector<description_draft_file> Files;

		auto operator==(const description_draft_material&) const -> bool = default;
	};

	/// \brief The run's message: the marker, the title, the description as it
	/// is (or that there is none), then each file with its patch, in the read's
	/// order, until the diff bound; files past it are named as left out.
	[[nodiscard]] auto compose_description_draft(const description_draft_material& aMaterial) -> std::string;

	/// \brief The description an answer holds: trimmed, and unwrapped when the
	/// whole answer is one fenced block. None for an empty answer or one
	/// longer than GitHub keeps (`maximum_description_characters`).
	[[nodiscard]] auto read_description_draft(std::string_view aAnswer) -> std::optional<std::string>;
} // namespace bpr::core

#endif
