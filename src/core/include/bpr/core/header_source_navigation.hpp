#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_HEADER_SOURCE_NAVIGATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_HEADER_SOURCE_NAVIGATION_HPP

#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	enum class header_source_family : std::uint8_t { header,
													 source };

	/// \brief Borrows the exact stem and directory of one supported review path.
	struct header_source_path
	{
		std::string_view     Directory;
		std::string_view     Stem;
		header_source_family Family = header_source_family::source;
	};

	/// \brief Classifies the final extension with ASCII-only case folding.
	/// \details A nonempty stem is case-sensitive; no previous-name alias or directory substitution is inferred.
	[[nodiscard]] auto classify_header_source_path(std::string_view aPath) noexcept -> std::optional<header_source_path>;

	/// \brief Removes and returns the last directory component for closest-suffix indexing.
	/// \details Comparing components in this order ranks longer shared trailing directory sequences first.
	[[nodiscard]] auto take_header_source_directory_component(std::string_view& aDirectory) noexcept -> std::string_view;

	/// \brief Summarizes one indexed candidate group without retaining its files.
	struct header_source_candidates
	{
		std::optional<file_identity> Unique;
		bool                         Ambiguous = false;

		/// \brief Adds a distinct stable identity; repeat identities do not create ambiguity.
		void include(file_identity aFile) noexcept;
		auto operator<=>(const header_source_candidates&) const = default;
	};

	enum class header_source_status : std::uint8_t {
		matched,
		no_match,
		ambiguous,
		unsupported_file,
		no_selection,
		unavailable_catalogue
	};

	/// \brief Reports a unique counterpart or the reason the view must remain unchanged.
	struct header_source_resolution
	{
		header_source_status         Status = header_source_status::unavailable_catalogue;
		std::optional<file_identity> File;
		header_source_family         SelectedFamily                                     = header_source_family::source;
		auto                         operator<=>(const header_source_resolution&) const = default;
	};

	/// \brief Prefers exact-directory candidates, then the deepest matching directory-suffix group.
	/// \details The suffix index's root summarizes all opposite-family candidates, including zero overlap.
	/// Equal best candidates are ambiguous; neither extension order nor catalogue order breaks a tie.
	[[nodiscard]] auto resolve_header_source_match(header_source_family            aSelectedFamily,
												   const header_source_candidates& aSameDirectory, const header_source_candidates& aClosestSuffix) noexcept -> header_source_resolution;
}

#endif
