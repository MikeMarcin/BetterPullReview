#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_APPLICATION_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/workbench_preferences_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace bpr::application {
	/// \brief Counts retained file work on the serialized workbench lane.
	/// \details Projection capacity growth counts vector storage acquisitions,
	/// not all process allocations. Observation ingestion is separate from
	/// routine selection, so a full accepted catalogue cannot hide a hot scan.
	struct workbench_work_statistics
	{
		std::uint64_t FileLookupComparisons         = 0U;
		std::uint64_t FileProjectionRows            = 0U;
		std::uint64_t FileProjectionCapacityGrowths = 0U;
		std::uint64_t AcceptedObservationRows       = 0U;
		/// [[BPR-0972]]. Retained-receipt changes applied to the order in place,
		/// the files they moved, and those that had to observe again instead.
		std::uint64_t TargetedActionabilityUpdates = 0U;
		std::uint64_t TargetedActionabilityFiles   = 0U;
		std::uint64_t ActionabilityObservations    = 0U;
		/// Dense retained order maintenance, distinct from projecting file data.
		std::uint64_t FilePositionVisits       = 0U;
		std::uint64_t PublishedFileRowsShifted = 0U;
	};

	/// \brief Provides the workbench application host contract.
	///
	/// \details The host owns the ordered actionable file set, the retained
	/// selection identity, and the durable layout; the core owns navigation
	/// legality, drawer semantics, and command projections. The host supplies the
	/// core bounded cursor facts and executes the decisions it answers with; it
	/// never re-decides them.
	///
	/// Public operations and inspection are confined to the supplied decision
	/// lane. Effect work runs on the effect executor and posts immutable terminals
	/// back to that lane. Referenced ports and executors must stay live until
	/// is_quiescent().
	class workbench_application_host final
	{
	public:
		workbench_application_host(
			core::workbench_interaction_state        aInitialState,
			workbench_observation_port&              aObservations,
			workbench_settings_port&                 aSettings,
			effect_executor&                         aEffects,
			decision_executor&                       aDecisionLane,
			workbench_projection_port&               aProjections,
			core::application_session_id             aSession,
			workbench_host_limits                    aLimits                  = { },
			workbench_selected_diff_port*            aSelectedDiffs           = nullptr,
			workbench_selected_diff_projection_port* aSelectedDiffProjections = nullptr,
			workbench_preferences_host*              aPreferences             = nullptr,
			core::bpr_profile_id                     aProfile                 = core::default_bpr_profile_id);
		~workbench_application_host();

		workbench_application_host(const workbench_application_host&) = delete;
		auto operator=(const workbench_application_host&)
			-> workbench_application_host&                       = delete;
		workbench_application_host(workbench_application_host&&) = delete;
		auto operator=(workbench_application_host&&)
			-> workbench_application_host& = delete;

		// Reads the durable layout. Restored drawer states reach the core as soon
		// as an accepted observation makes the workbench active; restoring never
		// writes the layout back.
		void restore();
		// Supersedes any outstanding observation and advances the incarnation.
		void observe(workbench_pull_request_authority aAuthority);
		// Retires one exact authority and clears every authority-bound cursor fact.
		void invalidate(workbench_pull_request_authority aAuthority);
		// Replays the retained semantic selection after an accepted comparison
		// becomes available or changes.
		void refresh_selected_diff();
		// Reobserves receipt-derived actionability without replacing the accepted
		// authority or detaching its retained selected diff.
		void refresh_actionability();
		// [[BPR-0972]]. The retained receipt now names `aFile`, actionable or
		// not; every other file is conservatively actionable. Applied to the
		// order in place and published as a file delta when the host holds every
		// file the change needs; otherwise reobserves, as refresh_actionability.
		void retain_actionability(workbench_pull_request_authority aAuthority,
								  core::workbench_file_id          aFile,
								  bool                             aActionable);
		// Supplies the bounded change cursor observed for one exact accepted diff.
		// Stale or superseded selected-diff identities are ignored.
		void observe_change_navigation(
			workbench_selected_diff_request              aRequest,
			std::optional<core::workbench_change_cursor> aCursor);
		void apply(core::workbench_interaction_intent aIntent);
		// Host-owned drawer width, clamped to the configured bound and persisted.
		void resize(core::workbench_drawer aDrawer, std::uint32_t aWidth);
		// [[BPR-0661]]. Host-owned review-behaviour preferences, validated and
		// persisted; a refused record republishes the one held so the page
		// reverts.
		void apply_behaviour(review_behaviour_settings aSettings);
		// [[BPR-0783]]. This window's Home view, validated and persisted beside
		// its own layout; a refused view republishes the one held so Home
		// reverts. A view applied before the layout has loaded is merged into the
		// loaded record rather than written over it.
		void apply_home_view(home_view_state aState);
		// Bounded presentation observation; persisted only after this window's
		// layout has loaded, so startup geometry cannot replace an unread record.
		void observe_geometry(workbench_window_geometry aGeometry);
		/// \brief Validates and persists only this window's explicit summary resize or Reset.
		void resize_summary(std::optional<std::uint32_t> aFraction);
		/// \brief [[BPR-1334]]. Validates and persists only this window's
		/// explicit Line details height or Reset.
		void resize_line_details(std::optional<std::uint32_t> aHeight);
		void cancel();
		void shutdown();
		// Closes ingress while draining the latest accepted window layout. Ports and
		// the decision lane remain live through quiescence; shutdown() can force cancellation.
		void shutdown_orderly();

		[[nodiscard]] auto work_statistics() const noexcept -> workbench_work_statistics;

		[[nodiscard]] auto state() const noexcept
			-> const core::workbench_interaction_state&;
		[[nodiscard]] auto layout() const noexcept
			-> const workbench_layout_settings&;
		/// \brief Returns an accepted changed-file entry through the retained identity index.
		[[nodiscard]] auto file_observation(core::workbench_file_id aFile) const -> std::optional<workbench_file_observation_entry>;

		[[nodiscard]] auto contains_file(core::workbench_file_id aFile) const noexcept -> bool;
		[[nodiscard]] auto actionable() const noexcept
			-> std::span<const core::workbench_file_id>;
		[[nodiscard]] auto selected() const noexcept
			-> const std::optional<core::workbench_file_id>&;
		[[nodiscard]] auto generation() const noexcept -> core::request_generation;
		[[nodiscard]] auto outstanding_requests() const noexcept -> std::size_t;
		[[nodiscard]] auto rejected_terminals() const noexcept -> std::size_t;
		[[nodiscard]] auto last_rejection() const noexcept
			-> workbench_terminal_rejection;
		[[nodiscard]] auto last_reason() const noexcept
			-> core::workbench_reason_code;
		[[nodiscard]] auto persisted_layouts() const noexcept -> std::size_t;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_APPLICATION_HOST_HPP
