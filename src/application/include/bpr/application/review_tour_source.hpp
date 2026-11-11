#ifndef BPR_APPLICATION_REVIEW_TOUR_SOURCE_HPP
#define BPR_APPLICATION_REVIEW_TOUR_SOURCE_HPP
#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/review_tour.hpp"
#include <expected>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
namespace bpr::application {
	class epoch_application_host;
	/// Exact pinned comparison for repair analysis, never a selected diff or a write grant.
	struct tour_source_comparison
	{
		core::diff_semantic_index   Index;
		core::tour_code_destination Left, Right;
	};
	[[nodiscard]] auto materialize_tour_source_comparison(epoch_application_host&, diff_line_matcher_port&,
														  const workbench_selected_diff_request&, std::stop_token) -> std::optional<tour_source_comparison>;
	/// Decision-lane admission of one pinned commit, including a file absent on the other PR side.
	[[nodiscard]] auto review_tour_source_context(epoch_application_host&, const workbench_pull_request_authority&, const core::tour_code_destination&) -> std::optional<workbench_selected_diff_context>;
	struct review_tour_source_material
	{
		core::tour_source_range Source;
		std::string             Text;
	};
	enum class review_tour_source_failure { unavailable,
											range_unavailable,
											too_large,
											cancelled };
	/// Runs on a source worker with exact endpoint resolution and a sealed epoch source read.
	/// Never reads a working tree or substitutes text for an unproved pinned endpoint.
	[[nodiscard]] auto materialize_review_tour_source(workbench_selected_diff_port&, epoch_application_host&,
													  const workbench_selected_diff_request&, const core::tour_code_destination&, std::stop_token)
		-> std::expected<review_tour_source_material, review_tour_source_failure>;

	/// \brief [[BPR-2014]]. The changed paths an Accept or an observed Ready
	/// resolves a stop's references against: `aSnapshot`'s, when it is the
	/// read of the pull request `aOwner` names; none when it is another's
	/// (a window that moved to another pull request supplies no list).
	[[nodiscard]] auto tour_changed_paths(const core::tour_owner& aOwner, const github_pull_request_snapshot* aSnapshot) -> std::vector<std::string>;

	/// \brief [[BPR-2014]]. One comparison of accepted code with the current
	/// head: the commit the code was accepted at, the head, and its path.
	using tour_material_key = std::tuple<core::git_commit_oid, core::git_commit_oid, std::string>;
	/// \brief True when no accepted code at `aKey`'s commit and path is left
	/// uncompared with `aKey`'s head: what a read that succeeded must leave.
	[[nodiscard]] auto tour_material_compared(const core::review_tour& aTour, const tour_material_key& aKey) -> bool;
	/// \brief [[BPR-2014]], critical review finding 7 and re-review finding 3.
	/// Which comparisons of accepted code a window may ask for now. A
	/// comparison is asked once at a time; one that failed, or that was read
	/// and left the code uncompared, is asked again on a later presentation,
	/// at most `maximum_attempts` times; one dropped unanswered (the window
	/// moved to another pull request) counts nothing and is asked again.
	class tour_material_ledger
	{
	public:
		static constexpr unsigned maximum_attempts = 3U;
		[[nodiscard]] auto        may_ask(const tour_material_key& aKey) const -> bool;
		void                      asked(tour_material_key aKey);
		/// The read of `aKey` ended; `aCompared` false counts an attempt.
		void answered(const tour_material_key& aKey, bool aCompared);
		/// Every comparison asked and not answered is forgotten, uncounted.
		void dropped() noexcept;

	private:
		std::vector<tour_material_key>                      Asking;
		std::vector<std::pair<tour_material_key, unsigned>> Failed;
	};
}
#endif
