#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_PROJECTION_HPP

#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

namespace bpr::application {
	using epoch_intent = std::variant<
		core::refresh_epochs_intent,
		core::select_named_comparison_intent,
		core::select_comparison_anchors_intent,
		core::select_comparison_anchor_intent,
		core::select_epoch_occurrence_intent,
		core::request_epoch_history_page_intent,
		core::prune_review_epochs_intent,
		core::delete_repository_cache_intent,
		core::cancel_epoch_maintenance_intent,
		core::cancel_epoch_refresh_intent,
		core::cancel_comparison_resolution_intent>;
	using epoch_intent_dispatch = std::function<void(epoch_intent)>;

	// This is the presentation form of an accepted revision. The host-owned
	// source-observation identity deliberately stops before the projection edge.
	/// \brief Describes the epoch revision projection record.
	struct epoch_revision_projection
	{
		core::provider_snapshot_id                           Snapshot;
		core::github_identity_generation                     IdentityGeneration;
		core::request_generation                             PullRequestGeneration;
		core::pull_request_revision_origin                   Origin = core::pull_request_revision_origin::live;
		core::base_branch_id                                 BaseBranch;
		core::git_commit_oid                                 Base;
		core::git_commit_oid                                 Head;
		std::optional<core::current_viewer_review_candidate> CurrentViewerReview;

		auto operator<=>(const epoch_revision_projection&) const = default;
	};

	/// \brief Describes the epoch authority projection record.
	struct epoch_authority_projection
	{
		std::optional<core::pull_request_authority> Authority;
		std::optional<epoch_revision_projection>    Revision;
		core::epoch_generation                      Generation;

		auto operator<=>(const epoch_authority_projection&) const = default;
	};

	/// \brief Describes the epoch row projection record.
	struct epoch_row_projection
	{
		core::review_epoch_id                        Id;
		core::provider_snapshot_id                   Snapshot;
		core::github_identity_generation             IdentityGeneration;
		core::request_generation                     PullRequestGeneration;
		core::base_branch_id                         BaseBranch;
		core::git_commit_oid                         Base;
		core::git_commit_oid                         Head;
		std::optional<core::epoch_material_identity> Material;
		core::epoch_cause                            Cause;
		core::observed_utc_milliseconds              ObservedAt;
		core::anchor_availability                    BaseAvailability = core::anchor_availability::unavailable;
		core::anchor_availability                    HeadAvailability = core::anchor_availability::unavailable;

		auto operator<=>(const epoch_row_projection&) const = default;
	};

	enum class epoch_row_change_kind {
		insert,
		remove,
		update,
	};

	/// \brief Describes the epoch row change projection record.
	struct epoch_row_change_projection
	{
		epoch_row_change_kind               Kind  = epoch_row_change_kind::update;
		std::size_t                         Index = 0U;
		core::review_epoch_id               Id;
		std::optional<epoch_row_projection> Row;

		auto operator<=>(const epoch_row_change_projection&) const = default;
	};

	/// \brief Describes the epoch history page projection record.
	struct epoch_history_page_projection
	{
		core::epoch_history_page_phase            Phase = core::epoch_history_page_phase::loading;
		core::epoch_catalog_identity              Catalog;
		core::epoch_history_navigation            Navigation = core::epoch_history_navigation::newest;
		std::optional<core::epoch_history_cursor> RequestedCursor;
		std::uint16_t                             Limit = core::maximum_epoch_history_page_size;
		std::vector<epoch_row_projection>         Rows;
		std::optional<core::epoch_history_cursor> Continuation;
		core::epoch_reason_code                   Reason = core::epoch_reason_code::none;

		auto operator<=>(const epoch_history_page_projection&) const = default;
	};

	/// \brief Describes the epoch catalog projection record.
	struct epoch_catalog_projection
	{
		std::optional<core::epoch_catalog_identity>   Catalog;
		bool                                          Replace = false;
		std::vector<epoch_row_change_projection>      Changes;
		std::vector<core::review_epoch_id>            AvailabilityChangedEpochs;
		std::optional<core::anchor_availability>      AvailabilityChangedTo;
		std::optional<core::anchor_availability>      AllEpochAvailability;
		std::optional<epoch_history_page_projection>  HistoryPage;
		std::optional<core::epoch_occurrence_history> OccurrenceHistory;
		std::optional<epoch_row_projection>           CurrentEpoch;
		bool                                          AllowOccurrenceImport = false;

		auto operator<=>(const epoch_catalog_projection&) const = default;
	};

	/// \brief Describes the epoch comparison projection record.
	struct epoch_comparison_projection
	{
		core::comparison_selection           Selection = core::named_selection(core::named_comparison::whole_pr);
		std::optional<core::comparison_pair> Pair;
		core::comparison_generation          Generation;
		core::comparison_revision            Revision;
		bool                                 AvailableForDiff = false;
		core::epoch_reason_code              Reason           = core::epoch_reason_code::none;
		/// [[BPR-1557]]. The pull request's base, which Whole PR compares
		/// from, as the core published it with this resolution.
		std::optional<core::comparison_anchor> PullRequestBase;

		auto operator<=>(const epoch_comparison_projection&) const = default;
	};

	/// \brief Describes the epoch transition projection record.
	struct epoch_transition_projection
	{
		core::epoch_changed_domain                      Changed = core::epoch_changed_domain::none;
		std::optional<epoch_authority_projection>       Authority;
		std::optional<core::comparison_selection_delta> Selection;
		std::optional<epoch_comparison_projection>      Comparison;
		std::optional<core::epoch_status_delta>         Status;
		std::optional<core::epoch_command_delta>        Commands;
		std::vector<core::epoch_diagnostic>             Diagnostics;

		auto operator<=>(const epoch_transition_projection&) const = default;
	};

	/// \brief Describes the epoch presentation update record.
	struct epoch_presentation_update
	{
		std::optional<epoch_transition_projection> Transition;
		std::optional<epoch_catalog_projection>    Catalog;

		auto operator<=>(const epoch_presentation_update&) const = default;
	};

	/// \brief Provides the epoch projection port contract.
	class epoch_projection_port
	{
	public:
		virtual ~epoch_projection_port() = default;

		// Delivery is synchronous and observational. The host contains projection
		// exceptions; presentation cannot veto accepted core or durable state.
		// The update contains no provider, Git, SQLite, credential, transport, or
		// host-request record.
		virtual void publish(const epoch_presentation_update& aUpdate) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_PROJECTION_HPP
