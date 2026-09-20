#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_COMPARISON_CONTROL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_COMPARISON_CONTROL_HPP

#include "bpr/application/epoch_projection.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/epoch_line.hpp"

#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>

class QAction;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QToolButton;

namespace bpr::qt {
	/// \brief Describes the epoch comparison control statistics record.
	struct epoch_comparison_control_statistics
	{
		std::size_t PublishedUpdates  = 0U;
		std::size_t Announcements     = 0U;
		std::size_t AppearanceUpdates = 0U;
	};

	/// \brief Provides the epoch comparison control contract.
	class epoch_comparison_control final
		: public QWidget,
		  public application::epoch_projection_port
	{
	public:
		explicit epoch_comparison_control(
			application::epoch_intent_dispatch aDispatch = { },
			QWidget*                           aParent   = nullptr);

		void publish(
			const application::epoch_presentation_update& aUpdate) override;
		void set_dispatch(application::epoch_intent_dispatch aDispatch);
		void set_maintenance_scope(core::pull_request_authority aAuthority, QString aLabel);

		[[nodiscard]] auto history_model() const noexcept -> epoch_history_model*;
		/// \brief RFC-BPR-0028's epoch line, the control's one always-shown row.
		[[nodiscard]] auto line() const noexcept -> epoch_line*;
		/// \brief "Comparing ..." with the named comparisons and history actions.
		[[nodiscard]] auto comparing_button() const noexcept -> QToolButton*;
		/// \brief The exact anchors, OIDs, status and maintenance, shown on request.
		[[nodiscard]] auto details() const noexcept -> QWidget*;
		void               set_details_visible(bool aVisible);
		[[nodiscard]] auto details_visible() const -> bool;
		[[nodiscard]] auto left_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto right_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto left_oid_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto right_oid_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto whole_pr_action() const noexcept -> QAction*;
		[[nodiscard]] auto previous_upload_action() const noexcept -> QAction*;
		[[nodiscard]] auto last_reviewed_action() const noexcept -> QAction*;
		[[nodiscard]] auto load_history_action() const noexcept -> QAction*;
		[[nodiscard]] auto return_to_newest_action() const noexcept -> QAction*;
		[[nodiscard]] auto refresh_action() const noexcept -> QAction*;
		[[nodiscard]] auto cancel_action() const noexcept -> QAction*;
		[[nodiscard]] auto prune_action() const noexcept -> QAction*;
		[[nodiscard]] auto delete_cache_action() const noexcept -> QAction*;
		[[nodiscard]] auto cancel_maintenance_action() const noexcept -> QAction*;
		/// \brief [[BPR-0676]]. What is being compared, in words, on the line.
		[[nodiscard]] auto line_summary() const noexcept -> QLabel*;
		[[nodiscard]] auto status_text() const -> QString;
		/// \brief [[BPR-0677]]. The state behind the wording, for a caller that
		/// must tell work in progress from work that failed.
		[[nodiscard]] auto status() const noexcept -> core::epoch_status;
		/// \brief Whether the accepted comparison can serve a selected diff.
		[[nodiscard]] auto comparison_available() const noexcept -> bool;
		[[nodiscard]] auto comparison_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> epoch_comparison_control_statistics;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void dispatch_alias(core::named_comparison aAlias);
		void dispatch_anchor(core::comparison_side aSide, int aRow);
		void dispatch_identity(
			core::comparison_side                   aSide,
			const core::comparison_anchor_identity& aIdentity);
		void               update_line();
		void               dispatch_older_history_page();
		void               dispatch_newest_history_page();
		[[nodiscard]] auto maintenance_scope_text() const -> QString;
		void               dispatch_prune();
		void               dispatch_delete_cache();
		void               apply_catalog(
			const application::epoch_catalog_projection& aProjection);
		void apply_transition(
			const application::epoch_transition_projection& aProjection);
		void update_controls();
		void update_selection_controls();
		void update_status_text();
		void update_action_state();
		void refresh_native_appearance();
		void announce_update();

		[[nodiscard]] auto summary_text() const -> QString;
		[[nodiscard]] auto command(core::epoch_command_id aId) const
			-> std::optional<core::epoch_command_projection>;

		std::optional<core::pull_request_authority> ScopeAuthority;
		QString                                     ScopeLabel;
		application::epoch_intent_dispatch          Dispatch;
		epoch_history_model*                        HistoryModel            = nullptr;
		epoch_line*                                 Line                    = nullptr;
		QToolButton*                                ComparingButton         = nullptr;
		QWidget*                                    Details                 = nullptr;
		QComboBox*                                  LeftSelector            = nullptr;
		QComboBox*                                  RightSelector           = nullptr;
		QLineEdit*                                  LeftOid                 = nullptr;
		QLineEdit*                                  RightOid                = nullptr;
		QLabel*                                     StatusLabel             = nullptr;
		QLabel*                                     ComparisonLabel         = nullptr;
		QAction*                                    WholePrAction           = nullptr;
		QAction*                                    PreviousUploadAction    = nullptr;
		QAction*                                    LastReviewedAction      = nullptr;
		QAction*                                    LoadHistoryAction       = nullptr;
		QAction*                                    ReturnNewestAction      = nullptr;
		QAction*                                    RefreshAction           = nullptr;
		QAction*                                    CancelAction            = nullptr;
		QAction*                                    PruneAction             = nullptr;
		QAction*                                    DeleteCacheAction       = nullptr;
		QAction*                                    CancelMaintenanceAction = nullptr;
		QToolButton*                                LoadHistoryButton       = nullptr;
		QToolButton*                                ReturnNewestButton      = nullptr;
		QToolButton*                                RefreshButton           = nullptr;
		QToolButton*                                CancelButton            = nullptr;
		QToolButton*                                MaintenanceButton       = nullptr;

		std::optional<core::epoch_occurrence_history>           OccurrenceHistory;
		bool                                                    AllowOccurrenceImport = false;
		std::optional<application::epoch_row_projection>        CurrentEpoch;
		std::optional<core::pull_request_authority>             Authority;
		std::optional<core::git_commit_oid>                     LastReviewCommit;
		std::optional<core::comparison_selection>               Selection;
		std::optional<application::epoch_comparison_projection> Comparison;
		std::optional<core::epoch_history_cursor>               Continuation;
		std::optional<core::epoch_history_navigation>           HistoryNavigation;
		std::optional<core::epoch_history_page_phase>           HistoryPhase;
		core::epoch_reason_code                                 HistoryReason    = core::epoch_reason_code::none;
		core::epoch_reason_code                                 DiagnosticReason = core::epoch_reason_code::none;
		QLabel*                                                 LineSummary      = nullptr;
		core::epoch_status                                      Status           = core::epoch_status::inactive;
		std::array<core::epoch_command_projection, 6>           Commands{ };
		bool                                                    HasCommands        = false;
		bool                                                    ApplyingProjection = false;
		bool                                                    HistoryWindowStale = false;
		QString                                                 LastAnnouncement;
		epoch_comparison_control_statistics                     Statistics;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_COMPARISON_CONTROL_HPP
