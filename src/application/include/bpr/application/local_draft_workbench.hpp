#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_WORKBENCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_WORKBENCH_HPP

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/local_draft_discovery.hpp"
#include "bpr/application/local_draft_store.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_material_bridge.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/review_cycle_views.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief The desktop's local PR draft surface host (RFC-BPR-0108,
/// [[BPR-2021]]): Prepare pull request, In preparation, the draft's
/// Changes and notes, refresh, discard, and the harness handoff.
/// \details Every store, workspace and Git call runs on the effect lane;
/// every projection is published on the decision lane. A late completion for
/// a draft that is no longer open is dropped. No account, agent or network
/// is needed for anything here.
namespace bpr::application {
	/// \brief What opening a worktree did.
	enum class local_draft_open_outcome : std::uint8_t {
		/// The draft for this worktree and branch was resumed.
		resumed,
		/// A new draft was created with its first candidate.
		created,
		/// The target is not settled; the author chooses it.
		choose_target,
		/// The worktree has a draft that moved here: an explicit rebind.
		moved,
		/// Nothing can be prepared yet; `recovery` says why.
		recovery,
	};
	[[nodiscard]] auto name(local_draft_open_outcome aOutcome) noexcept -> std::string_view;

	/// \brief Why a worktree cannot be prepared yet. The specific task, not a
	/// general failure.
	enum class local_draft_recovery : std::uint8_t {
		none,
		/// A folder or file without Git: Review Cycles reviews it.
		not_git,
		/// The location could not be read.
		unreadable,
		/// The repository uses a format these readers decline.
		repository,
		detached_head,
		unborn_branch,
		/// The remotes do not reach a hosted repository; see the chain refusal.
		remote_chain,
		/// The target branch's tip is not known here or not in the objects.
		target_unavailable,
		no_common_base,
		several_bases,
		/// Git could not answer a read.
		git,
		/// The store refused; see the store error.
		store,
	};
	[[nodiscard]] auto name(local_draft_recovery aRecovery) noexcept -> std::string_view;

	/// \brief One open of a worktree, shared by the desktop and `bpr draft open`.
	struct local_draft_open_result
	{
		local_draft_open_outcome                 Outcome  = local_draft_open_outcome::recovery;
		local_draft_recovery                     Recovery = local_draft_recovery::none;
		std::optional<core::local_draft>         Draft;
		local_draft_discovery                    Discovery;
		std::optional<local_draft_store_error>   Store;
		std::optional<local_draft_capture_error> Capture;
	};

	/// \brief Opens a worktree: resumes its draft by Git-directory identity
	/// and branch, or creates one with the committed change as its first
	/// candidate. `aContext`, from a harness, becomes a note. Nothing is
	/// published and no second draft is made for the same worktree and branch.
	/// \param aNewId A fresh 32-hex identity, used only if a draft is created.
	[[nodiscard]] auto open_local_draft(local_draft_store& aStore, workspace_source_port& aSource, local_draft_git_port& aGit, std::string_view aLocation,
										const std::optional<std::string>& aContext, core::local_draft_id aNewId, std::int64_t aNow, std::stop_token aStop)
		-> std::expected<local_draft_open_result, local_draft_open_error>;

	/// \brief Home's rows: In preparation, and the marks on pull request rows.
	struct local_draft_home_row
	{
		core::local_draft_id Id;
		std::string          Title;
		/// `OWNER/NAME`.
		std::string  Repository;
		std::string  Branch;
		std::int64_t Updated = 0;
		auto         operator<=>(const local_draft_home_row&) const = default;
	};
	struct local_draft_mark
	{
		core::local_draft_id Id;
		std::string          Host;
		std::string          Owner;
		std::string          Name;
		std::uint64_t        Number = 0;
		auto                 operator<=>(const local_draft_mark&) const = default;
	};
	struct local_draft_home_projection
	{
		std::vector<local_draft_home_row> InPreparation;
		std::vector<local_draft_mark>     Marks;
		/// The store could not be read.
		bool Unavailable = false;
		auto operator<=>(const local_draft_home_projection&) const = default;
	};

	enum class local_draft_stage : std::uint8_t {
		closed,
		/// Reading the worktree.
		opening,
		/// A draft (or a recovery for the chosen worktree) is shown.
		open,
	};

	/// \brief The file the Changes area shows.
	struct local_draft_file_view
	{
		std::string                         Path;
		core::draft_change                  Change = core::draft_change::modified;
		review_material_status              Status = review_material_status::failure;
		std::vector<core::cycle_diff_row>   Rows;
		core::diff_reason_code              Reason = core::diff_reason_code::none;
		auto                                operator<=>(const local_draft_file_view&) const = default;
	};

	struct local_draft_workbench_projection
	{
		local_draft_stage        Stage = local_draft_stage::closed;
		local_draft_open_outcome Outcome  = local_draft_open_outcome::recovery;
		local_draft_recovery     Recovery = local_draft_recovery::none;
		std::string              Location;
		/// The draft, when one exists (resumed, created or moved).
		std::optional<core::local_draft> Draft;
		/// The candidate shown, by ordinal.
		std::uint64_t Shown = 0;
		/// What the worktree shows now, for the header and the working edits.
		std::string                           Branch;
		core::source_state                    Source = core::source_state::unreadable;
		std::optional<core::chosen_target>    Target;
		std::vector<core::local_remote_hop>   Chain;
		std::optional<core::remote_chain_refused> ChainRefusal;
		std::optional<core::target_selection> Selection;
		std::optional<core::git_commit_oid>   Head;
		std::optional<core::git_commit_oid>   TargetTip;
		core::local_draft_comparison          Comparison;
		std::vector<core::working_file>       Working;
		/// Working files the next capture includes.
		std::vector<std::string> Included;
		bool                     ScanIncomplete     = false;
		std::uint64_t            FilteredUncompared = 0;
		/// The worktree differs from the shown candidate: Refresh records a successor.
		bool RefreshOffered = false;
		/// The worktree could not be read when the draft was resumed; the
		/// stored candidates remain readable.
		bool WorktreeMissing = false;
		/// How the worktree read now relates to the draft: anything but
		/// `same` leaves the draft as it is and records nothing.
		core::worktree_match Match = core::worktree_match::same;
		std::optional<local_draft_file_view>     File;
		std::optional<local_draft_store_error>   Store;
		std::optional<local_draft_capture_error> Capture;
		std::optional<publication_git_error>     Git;
		/// A write lost to another writer was re-read.
		bool Reread = false;
		bool Busy   = false;
		auto operator<=>(const local_draft_workbench_projection&) const = default;
	};

	class local_draft_workbench_port
	{
	public:
		virtual ~local_draft_workbench_port() = default;

		virtual void publish(const local_draft_workbench_projection& aProjection) = 0;
		virtual void publish(const local_draft_home_projection& aHome)           = 0;
	};

	/// \brief Makes the isolated Git reader on the effect lane, at first use.
	using local_draft_git_factory = std::function<publication_git_result<std::unique_ptr<local_draft_git_port>>(std::stop_token)>;

	class local_draft_workbench final
	{
	public:
		/// \param aStore Opened on the effect lane at first use; null when
		/// the profile's store cannot be opened.
		/// \param aSeed Seeds new draft identities.
		/// \param aNow Seconds since the epoch.
		local_draft_workbench(std::function<std::unique_ptr<local_draft_store>()> aStore, workspace_source_port& aSource, local_draft_git_factory aGit, effect_executor& aEffects,
							  decision_executor& aDecisions, local_draft_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed, std::function<std::int64_t()> aNow,
							  std::shared_ptr<diff_line_matcher_port> aMatcher = nullptr);
		~local_draft_workbench();
		local_draft_workbench(const local_draft_workbench&)                    = delete;
		auto operator=(const local_draft_workbench&) -> local_draft_workbench& = delete;
		local_draft_workbench(local_draft_workbench&&)                         = delete;
		auto operator=(local_draft_workbench&&) -> local_draft_workbench&      = delete;

		/// \brief Publishes Home's rows again.
		void refresh_home();
		/// \brief Prepare pull request: opens the chosen worktree.
		void open_location(std::string aLocation, std::optional<std::string> aContext = std::nullopt);
		/// \brief Resumes a draft from In preparation or a pull request's mark.
		void resume(core::local_draft_id aId);
		/// \brief The author's choice of target.
		void choose_target(core::local_draft_target aTarget);
		/// \brief Includes or leaves out one working file in the next capture.
		void include(std::string aPath, bool aIncluded);
		/// \brief Observes the worktree again and records a successor
		/// candidate with the included files when anything differs.
		void refresh();
		/// \brief Shows an earlier or later candidate; it stays as captured.
		void show_candidate(std::uint64_t aOrdinal);
		void open_file(std::string aPath);
		void add_note(std::string aText, std::string aPath);
		void remove_note(std::uint64_t aNote);
		void rename(std::string aTitle);
		/// \brief Confirms the rebind a moved worktree offered.
		void rebind();
		/// \brief Removes the open draft's private preparation. The caller
		/// has asked first. The worktree, its branches and any pull request
		/// are not touched.
		void discard();
		/// \brief Leaves the draft; nothing is lost.
		void close();

		[[nodiscard]] auto projection() const noexcept -> const local_draft_workbench_projection&;
		[[nodiscard]] auto home() const noexcept -> const local_draft_home_projection&;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
