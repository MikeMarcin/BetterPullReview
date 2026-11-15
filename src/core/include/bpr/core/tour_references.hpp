#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_TOUR_REFERENCES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_TOUR_REFERENCES_HPP

#include "bpr/core/agent_reference.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1996]], RFC-BPR-0100. Code references in a tour's own text.
/// \details A stop's text, the Problem and the Solution cite code as
/// `path:first-last`, by the forms the agent replies' recogniser reads
/// (RFC-BPR-0101). In the application a reference is a link to those lines in
/// Review, and one alone on its line is a snippet of them; on GitHub it is a
/// commit-pinned permalink. This unit decides which references a text holds,
/// which of them resolve to exactly one file of the pull request, and writes
/// each resolved one in the form its caller names; one that does not resolve
/// is left as written and reported, so the author learns before publishing
/// that it will not link.
namespace bpr::core {
	/// \brief The most lines a snippet of a reference shows in the application.
	inline constexpr std::uint32_t tour_reference_snippet_lines = 12U;

	/// \brief One reference of a tour text that resolves to one file.
	struct tour_text_reference
	{
		/// The reference as written, backticks of a code span included.
		std::string Written;
		/// The pull request's file it names.
		std::string   Path;
		std::uint32_t First  = 0U;
		std::uint32_t Last   = 0U;
		bool          Before = false;
		/// Nothing else on its line: a snippet's place.
		bool Alone                                                = false;
		auto operator==(const tour_text_reference&) const -> bool = default;
	};

	/// \brief Writes one resolved reference: the text that replaces it, or
	/// none to leave it as written.
	using tour_reference_form = std::function<std::optional<std::string>(const tour_text_reference&)>;

	struct tour_reference_rewrite
	{
		std::string Text;
		/// References left as written because they name no file of the pull
		/// request, or several, as written; in order, each once.
		std::vector<std::string> Unresolved;
		auto                     operator==(const tour_reference_rewrite&) const -> bool = default;
	};

	/// \brief `aMarkdown` with each reference that resolves to one file of
	/// `aFiles` written by `aForm`. Fenced code, other code spans and existing
	/// links are left as written; a bare "line 12" is not a reference here.
	[[nodiscard]] auto rewrite_tour_references(std::string_view aMarkdown, const agent_reference_files& aFiles, const tour_reference_form& aForm) -> tour_reference_rewrite;

	/// \brief The references of `aMarkdown` that will not link, as written.
	[[nodiscard]] auto unresolved_tour_references(std::string_view aMarkdown, const agent_reference_files& aFiles) -> std::vector<std::string>;

	/// \brief The last line a snippet of `aReference` shows: at most
	/// `tour_reference_snippet_lines` from its first.
	[[nodiscard]] auto tour_reference_snippet_last(const tour_text_reference& aReference) noexcept -> std::uint32_t;
} // namespace bpr::core

#endif
