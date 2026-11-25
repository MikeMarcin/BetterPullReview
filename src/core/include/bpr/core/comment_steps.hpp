#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_COMMENT_STEPS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_COMMENT_STEPS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief One comment Comments lists on the file shown, as a stop of
	/// Previous and Next comment.
	///
	/// \details The owner, 2026-11-24: "They should just go through comments
	/// in the current file and stop when it reaches either end." The host
	/// supplies only the comments the drawer lists after its filters, so a
	/// filtered-out comment is never a stop.
	struct comment_step_stop
	{
		/// The comment's identity in the drawer.
		std::string Id;
		/// Where the comparison shows the comment's line, top to bottom: an
		/// observation of the view's own order. None when its line is not
		/// shown (outdated, a whole-file comment, or no aligned comparison).
		std::optional<std::int64_t> Shown;
		/// The one-based line it sits on; zero for the whole file.
		std::uint32_t Line = 0U;
		/// The projection's own order, which breaks ties.
		std::size_t Order = 0U;

		auto operator==(const comment_step_stop&) const -> bool = default;
	};

	/// \brief Where Previous and Next comment go from the comment open.
	struct comment_steps
	{
		/// The file's comments in the order they are stepped through.
		std::vector<std::string> Stops;
		/// The comment before the open one; none at the first. With none of
		/// the file's comments open, the file's last.
		std::optional<std::string> Previous;
		/// The comment after the open one; none at the last. With none of
		/// the file's comments open, the file's first.
		std::optional<std::string> Next;

		auto operator==(const comment_steps&) const -> bool = default;
	};

	/// \brief Decides the order of the file's comments and the step each way
	/// from \p aOpen.
	///
	/// \details Comments whose line is shown come first, top to bottom, then
	/// the rest by line; equal places keep the projection's order, then the
	/// order given. The steps never wrap and never leave the file: at either
	/// end that step is none, and with no comments both are.
	[[nodiscard]] auto decide_comment_steps(std::vector<comment_step_stop> aStops, std::string_view aOpen) -> comment_steps;
} // namespace bpr::core

#endif
