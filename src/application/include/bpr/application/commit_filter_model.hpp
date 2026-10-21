#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_MODEL_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_MODEL_HPP

#include "bpr/application/commit_filter_projection.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/commit_filter.hpp"
#include "bpr/core/generated_files.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief One file of a commit-filtered review, ready for the workbench and
	/// the Overview.
	///
	/// \details `Record` is the snapshot's own record for a file whose diff is
	/// the comparison's (no `Filter`). For an exact filtered diff it is built
	/// from the two endpoint commits' tree diffs: its change is what the file
	/// is between them, and its line counts are unknown (zero) until read.
	struct commit_filter_file
	{
		github_file_record                           Record;
		std::optional<core::filtered_diff_endpoints> Filter;
		bool                                         Split = false;
		/// For a split file: the hidden commits that change it between shown
		/// ones, oldest first.
		std::vector<core::git_commit_oid> SplitHidden;
		/// RFC-BPR-0089 M2. The file's generated decision. A file in the set is
		/// generated only when the reviewer chose Review anyway.
		core::generated_decision Generated;

		auto operator<=>(const commit_filter_file&) const = default;
	};

	/// \brief The filtered file set in review order.
	///
	/// \details Files the comparison lists keep its order; a file only the
	/// filter's diffs name (a shown commit added it and a hidden one removed
	/// it) follows them, by path.
	struct commit_filter_files
	{
		std::vector<commit_filter_file> Files;
		std::size_t                     SplitFiles = 0U;
		/// RFC-BPR-0089 M2. Whether the commit filter narrowed the set.
		bool CommitFiltered = false;
		/// Files the reviewer's file filters left out.
		std::size_t FileFiltered = 0U;
		/// Generated files set aside behind their own row, in review order.
		std::vector<commit_filter_file> SetAside;

		auto operator<=>(const commit_filter_files&) const = default;
	};

	/// \brief RFC-BPR-0089 M1. Joins the accepted snapshot's commits, the
	/// window's comparison, the chain's tree diffs and the reviewer's filter.
	///
	/// \details Holds no effect and no clock: the composition asks the epoch
	/// host for the tree diffs and hands the answer back. Every decision is the
	/// core's (`commit_filter.hpp`); this keeps the inputs and maps the answers
	/// to the workbench's file records and the Qt projection.
	class commit_filter_model final
	{
	public:
		/// \brief Adopts an accepted snapshot. The filter is kept; tree diffs
		/// are kept only when the head is unchanged.
		void adopt(const github_pull_request_snapshot& aSnapshot);

		/// \brief Forgets the pull request, its filter and its tree diffs.
		void clear();

		/// \brief Adopts the window's comparison, or none while unresolved.
		void adopt_comparison(std::optional<core::filter_comparison> aComparison);

		/// \brief Restores a persisted filter. Refused when malformed.
		[[nodiscard]] auto restore(core::commit_filter_state aState) -> bool;

		/// \brief The chain's tree diffs to ask for, oldest first, when the
		/// graph is known and they have not been asked for under this head.
		[[nodiscard]] auto touches_steps() const -> std::optional<std::vector<commit_filter_touch_step>>;

		/// \brief The accepted head the tree diffs are asked under.
		[[nodiscard]] auto head() const noexcept -> const std::optional<core::git_commit_oid>&;

		/// \brief Records that the tree diffs were asked for.
		void begin_touches();

		/// \brief Adopts the tree diffs; an answer for another head or other
		/// steps is ignored.
		void adopt_touches(const commit_filter_touches_observation& aObservation);

		/// \brief Applies one gesture; `none` when accepted. Clear all filters
		/// also forgets the file filters.
		[[nodiscard]] auto apply(const commit_filter_gesture& aGesture) -> core::commit_filter_refusal;

		/// \brief RFC-BPR-0089 M2. "Hide files these commits changed" on the
		/// selected commits' full IDs; `none` when accepted.
		[[nodiscard]] auto hide_files_changed_by(const std::vector<std::string>& aCommits) -> core::commit_filter_refusal;

		[[nodiscard]] auto file_filter() const noexcept -> const core::file_filter_state&;

		/// \brief RFC-BPR-0089 M2. Adopts the review's generated marks as the
		/// ledger holds them. Refused when malformed.
		[[nodiscard]] auto adopt_generated_marks(core::generated_file_marks aMarks) -> bool;

		[[nodiscard]] auto generated_marks() const noexcept -> const core::generated_file_marks&;

		/// \brief RFC-BPR-0089 M2. Adopts the head's `linguist-generated`
		/// rules, or none while they are not read.
		void adopt_generated_attributes(std::shared_ptr<const core::generated_attribute_rules> aRules);

		/// \brief The mark a gesture leaves on one file the review lists, for
		/// the composition to persist; none when the file is not listed, the
		/// gesture is not offered for it, or the core refuses it.
		[[nodiscard]] auto generated_mark_after(const generated_file_gesture& aGesture) const
			-> std::optional<core::generated_file_mark>;

		[[nodiscard]] auto state() const noexcept -> const core::commit_filter_state&;

		/// \brief Whether the commit filter changes the file set now: it
		/// applies to the comparison and the trees are read.
		[[nodiscard]] auto filtering() const -> bool;

		/// \brief The review's file set while anything leaves files out of it:
		/// the commit filter, a file filter, or generated files set aside.
		/// None while the set is exactly the comparison's.
		[[nodiscard]] auto files() const -> std::optional<commit_filter_files>;

		/// \brief What the Commits tab and the chip show.
		[[nodiscard]] auto projection() const -> commit_filter_projection;

	private:
		[[nodiscard]] auto span() const -> std::optional<std::expected<core::filter_commit_span, core::commit_filter_suspension>>;
		/// \brief The commit filter's own file set, while `filtering()`.
		[[nodiscard]] auto commit_filtered_files() const -> std::optional<commit_filter_files>;
		/// \brief The core's touch sets for the span's chain commits.
		[[nodiscard]] auto span_touches(const core::filter_commit_span& aSpan) const -> std::vector<core::filter_commit_touch>;
		/// \brief The generated decision of one listed path.
		[[nodiscard]] auto generated_decision_of(std::string_view aPath) const -> core::generated_decision;
		/// \brief Whether the review lists the path, as the comparison or a
		/// filtered diff names it.
		[[nodiscard]] auto listed_path(std::string_view aPath) const -> bool;
		/// \brief One file's generated state and the gestures the core offers.
		[[nodiscard]] auto generated_row(const commit_filter_file& aFile) const -> generated_file_row;
		/// \brief The set-aside list, the marked files and the menus.
		void project_generated(commit_filter_projection& aProjection) const;

		std::optional<core::git_commit_oid>                                       Head;
		std::expected<core::filter_commit_graph, core::commit_filter_unavailable> Graph = std::unexpected{ core::commit_filter_unavailable::invalid_list };
		bool                                                                      Known = false;
		std::vector<std::string>                                                  Authors;
		std::vector<github_file_record>                                           SnapshotFiles;
		std::optional<core::filter_comparison>                                    Comparison;
		core::commit_filter_state                                                 State;
		commit_filter_trees                                                       Trees = commit_filter_trees::idle;
		/// Touches for every chain commit, oldest first, once read.
		std::vector<commit_filter_commit_touch>    Touches;
		std::optional<core::commit_filter_refusal> Refusal;
		core::file_filter_state                    FileFilter;
		core::generated_file_marks                 GeneratedMarks;
		/// Shared so the model stays copyable; the rules are immutable.
		std::shared_ptr<const core::generated_attribute_rules> AttributeRules;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_MODEL_HPP
