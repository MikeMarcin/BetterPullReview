#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_EXPLAIN_FILE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_EXPLAIN_FILE_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// \file
/// \brief RFC-BPR-0117 ([[BPR-2076]]). Explain this file: the reviewer asks the
/// agent about the file open in Review. Pure decisions: the material the one
/// tool-free call carries up front (the file's change in this pull request and
/// its text at the head, each within its bound) and the private note Keep as
/// note makes from the answer.
namespace bpr::core {
	/// \brief The file's change in the pull request, as unified diff text.
	inline constexpr std::size_t maximum_explain_change_bytes = 96U * 1024U;
	/// \brief The file's text at the head the agent is given.
	inline constexpr std::size_t maximum_explain_content_bytes = 64U * 1024U;
	/// \brief A path, as the subject's own bound has it.
	inline constexpr std::size_t maximum_explain_path_bytes = 4096U;

	/// \brief What the head holds of the file, as the request says it.
	enum class explain_content_state : std::uint8_t {
		/// The whole text is in `Content`.
		whole,
		/// The text was cut at a line end to its bound.
		cut,
		/// The head does not hold the file (the pull request deletes it).
		absent,
		/// The file is not text (not UTF-8, or holds a NUL).
		binary,
		/// The file is larger than the read takes; no text is given.
		too_large,
	};
	[[nodiscard]] auto stable_name(explain_content_state aState) noexcept -> std::string_view;

	/// \brief One Explain this file run's material, up front.
	struct supplement_explain
	{
		std::string Path;
		/// Full lowercase hexadecimal commits the change is between.
		std::string BaseCommit;
		std::string HeadCommit;
		/// The change; cut at a line end when `ChangeCut`.
		std::string           Change;
		bool                  ChangeCut = false;
		std::string           Content;
		explain_content_state ContentState = explain_content_state::whole;

		auto operator<=>(const supplement_explain&) const = default;
	};

	/// \brief What the host read for one file, before bounds.
	struct explain_material_input
	{
		std::string Path;
		std::string BaseCommit;
		std::string HeadCommit;
		/// The rendered change (`render_returning_change`).
		std::string Change;
		/// The file at the head; none when the head does not hold it.
		std::optional<std::string> Head;
		/// The read refused the file as too large.
		bool TooLarge = false;

		auto operator<=>(const explain_material_input&) const = default;
	};

	/// \brief Bounds one file's material, deterministically, or refuses it.
	///
	/// \details Refused (none) without a path, with a path past its bound, or
	/// without both commits. The change and the text are each cut at the last
	/// line end within their bound (at a character boundary when the first
	/// line alone is longer); the text's state says which of `explain_content_state`
	/// applies.
	[[nodiscard]] auto bound_explain_material(explain_material_input aInput) -> std::optional<supplement_explain>;

	/// \brief Whether a supplement is within every bound `bound_explain_material` keeps.
	[[nodiscard]] auto valid(const supplement_explain& aExplain) -> bool;

	/// \brief The text of the private note Keep as note makes.
	///
	/// \details A first line that says the note is the agent's explanation of
	/// the file ("Agent: Claude explained this file."), a blank line, then the
	/// answer, cut at a line end so the whole stays within `aMaximumBytes`
	/// with an ellipsis line where it was cut. Empty for an empty answer.
	[[nodiscard]] auto explain_note_text(std::string_view aAgent, std::string_view aAnswer, std::size_t aMaximumBytes) -> std::string;
} // namespace bpr::core

#endif
