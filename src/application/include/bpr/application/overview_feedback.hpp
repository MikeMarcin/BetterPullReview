#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OVERVIEW_FEEDBACK_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OVERVIEW_FEEDBACK_HPP

#include "bpr/core/feedback_label_columns.hpp"
#include "bpr/core/feedback_partition.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1089]]. The Overview's label chips, decided from the kept label
/// buckets of [[BPR-1087]] (RFC-BPR-0049 R21, R23, R24, R27; RFC-BPR-0050 R31).
/// \details The Overview asks with its chips, its mode and its sort; the
/// answer is one projection per row, read from kept tallies only, so a new
/// chip, mode or sort reads no conversation and asks no provider. Every row
/// is a set of disjoint core scopes: a file row is its file; a folder row is
/// the listed files directly in it, so it is the sum of the rows beneath it;
/// the "Not on a file" row is the conversations with no exact file plus any on
/// a file the pull request no longer lists, so the rows together count every
/// conversation once. A drill-down names a row and one of its numbers, and
/// lists exactly the conversations that number counted, from the same
/// population, columns and mode.
namespace bpr::application {
	class feedback_counter_model;

	/// \brief What one Overview shows: the reviewer's chips, from Settings, and
	/// the view's own mode and sort.
	struct overview_feedback_request
	{
		core::feedback_label_columns   Columns{ };
		core::feedback_resolution_mode Mode = core::default_feedback_resolution_mode;
		/// The number rows sort by; none keeps the file order.
		std::optional<core::feedback_count_target> Sort;

		auto operator==(const overview_feedback_request&) const -> bool = default;
	};

	/// \brief Which Overview row a number belongs to.
	enum class overview_feedback_row_kind : std::uint8_t {
		/// One listed file, by path.
		file,
		/// The listed files directly in one folder, by the folder's path; the
		/// repository root is the empty path.
		folder,
		/// "Not on a file": conversations with no exact file, and any on a
		/// file the pull request does not list.
		remainder,
		/// The whole pull request.
		pull_request,
	};

	/// \brief One row's counts. `Path` is the file's or the folder's path,
	/// empty for the remainder and the pull request.
	struct overview_feedback_row
	{
		overview_feedback_row_kind        Kind = overview_feedback_row_kind::file;
		std::string                       Path;
		core::feedback_counter_projection Counts;
	};

	/// \brief Every row's counts for one request.
	struct overview_feedback_projection
	{
		overview_feedback_request Request;
		/// The counter model's generation the counts were read at.
		std::uint64_t Generation = 0U;
		/// One per listed file, in the order the files were given.
		std::vector<overview_feedback_row> Files;
		/// One per folder that holds a listed file, in path order.
		std::vector<overview_feedback_row> Folders;
		/// Kind `remainder`.
		overview_feedback_row Remainder;
		/// Kind `pull_request`: what the ? chip beside the switch counts.
		overview_feedback_row PullRequest;
		/// The listed file paths in display order when `Request.Sort` is set:
		/// folders by their own number, then each folder's files by theirs,
		/// ties in the given order. Empty in file order.
		std::vector<std::string> Order;
	};

	/// \brief One number of one row: what a chip, its menu entry or the ?
	/// chip activates.
	struct overview_drill_down
	{
		overview_feedback_row_kind  Row = overview_feedback_row_kind::pull_request;
		std::string                 Path;
		core::feedback_count_target Target;

		auto operator==(const overview_drill_down&) const -> bool = default;
	};

	/// \brief The conversations one number counted.
	struct overview_drill_down_listing
	{
		overview_drill_down            DrillDown;
		core::feedback_resolution_mode Mode = core::default_feedback_resolution_mode;
		/// Conversation identities, each once, in identity order.
		std::vector<std::string> Conversations;
		/// The number the row showed, from the same tallies.
		std::size_t                             Count = 0U;
		std::vector<core::feedback_count_limit> Limits;
		/// The accepted annotation epoch the list and the number were read at.
		std::uint64_t Epoch = 0U;
	};

	/// \brief Every row's counts for \p aFiles, the pull request's listed file
	/// paths, from the model's kept tallies. Nothing before the model has
	/// admitted a snapshot, or for a request whose columns are not a valid
	/// selection.
	[[nodiscard]] auto project_overview_feedback(const feedback_counter_model& aModel, std::span<const std::string> aFiles,
												 const overview_feedback_request& aRequest) -> std::optional<overview_feedback_projection>;

	/// \brief The exact conversations behind one number of one row, for the
	/// same listed files and request the projection was made with. Nothing for
	/// a row the files do not hold, an unselected label, or before the model
	/// has admitted a snapshot.
	[[nodiscard]] auto list_overview_drill_down(const feedback_counter_model& aModel, std::span<const std::string> aFiles,
												const overview_feedback_request& aRequest, const overview_drill_down& aDrillDown)
		-> std::optional<overview_drill_down_listing>;
}

#endif
