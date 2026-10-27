#ifndef BPR_CORE_DESCRIPTION_EDIT_HPP
#define BPR_CORE_DESCRIPTION_EDIT_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief [[BPR-1774]]. Whether the pull request's description may be
/// edited, and whether a save may be written.
///
/// \details The owner, 2026-10-26: "We seem to be missing author/uploader
/// controls for editing the summary". Edit is offered to the pull request's
/// author, the account that opened it. Save is decided twice: once on the
/// gesture, from what the accepted read said and what was typed, and once on
/// what GitHub says just before the write, so a body changed on GitHub since
/// editing began is never overwritten silently.
namespace bpr::core {
	/// \brief GitHub's limit on a pull request's body, in characters.
	inline constexpr std::size_t maximum_description_characters = 65536U;

	/// \brief Why a description edit or save is refused.
	enum class description_edit_refusal : std::uint8_t {
		none,
		/// The viewer is not the pull request's author, or reads anonymously.
		not_author,
		/// The text is the body editing began with; there is nothing to send.
		unchanged,
		/// The text is longer than GitHub keeps.
		too_long,
		/// GitHub, asked before the write, says this viewer may not update
		/// the pull request.
		not_permitted,
		/// GitHub's body is no longer the one editing began with.
		conflict,
	};

	[[nodiscard]] auto name(description_edit_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The characters of a UTF-8 text as GitHub counts its limit: code
	/// points, each a lead byte.
	[[nodiscard]] auto description_characters(std::string_view aText) noexcept -> std::size_t;

	/// \brief Whether Edit is offered: the viewer is the pull request's
	/// author, by `same_login` (GitHub compares logins without ASCII case).
	/// An empty login is nobody's, so an anonymous read is never offered it.
	[[nodiscard]] auto description_edit_offered(std::string_view aAuthor, std::string_view aViewer) noexcept -> bool;

	/// \brief What decides a Save gesture, before anything is asked of GitHub.
	struct description_edit_input
	{
		/// The pull request's author as the accepted read gave it.
		std::string_view Author;
		/// The signed-in account's login; empty for an anonymous read.
		std::string_view Viewer;
		/// The body editing began with, byte for byte.
		std::string_view Original;
		/// The text to save.
		std::string_view Edited;
	};

	/// \brief Decides a Save gesture from its inputs alone, in order: not
	/// the author, unchanged, too long.
	[[nodiscard]] auto decide_description_edit(const description_edit_input& aInput) noexcept -> description_edit_refusal;

	/// \brief What GitHub said of the pull request just before the write.
	struct description_save_observation
	{
		/// The body editing began with.
		std::string_view Original;
		/// The text to save.
		std::string_view Edited;
		/// The body GitHub has now.
		std::string_view Current;
		/// GitHub's `viewerCanUpdate`.
		bool ViewerCanUpdate = false;
	};

	/// \brief What a save does once GitHub has been asked.
	enum class description_save_outcome : std::uint8_t {
		/// Write the text once.
		write,
		/// GitHub already has the text; nothing is sent.
		already_saved,
		/// GitHub does not let this viewer update the pull request.
		not_permitted,
		/// GitHub's body changed since editing began; nothing is sent.
		conflict,
	};

	[[nodiscard]] auto name(description_save_outcome aOutcome) noexcept -> std::string_view;

	/// \brief Decides a save from GitHub's answer, in order: already saved,
	/// not permitted, conflict, write. A body GitHub already shows as asked
	/// is the answer to a lost write, so it is settled before the rest.
	[[nodiscard]] auto decide_description_save(const description_save_observation& aObservation) noexcept -> description_save_outcome;
} // namespace bpr::core

#endif
