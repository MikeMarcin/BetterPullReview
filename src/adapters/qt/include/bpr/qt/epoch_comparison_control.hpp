#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_COMPARISON_CONTROL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_COMPARISON_CONTROL_HPP

#include "bpr/application/epoch_projection.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/epoch_line.hpp"

#include <QPointer>
#include <QToolButton>
#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>

class QAction;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;

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
		///
		/// \details [[BPR-1129]]. Its text names the comparison in force and,
		/// while both compared updates have a date, the dates: "Comparing
		/// since my last review, Feb 12 → Feb 15".
		[[nodiscard]] auto comparing_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1129]]. Hands the "Comparing" button to the review bar.
		///
		/// \details The line keeps its own row; its "Updates" name goes, since
		/// the bar names Updates before the button. The control keeps the
		/// button's text, menu and state current wherever it is placed, and
		/// the caller places it. A second call returns null.
		[[nodiscard]] auto release_comparing_button() -> QToolButton*;
		/// \brief [[BPR-1129]]. Whether the button names the compared dates;
		/// the review bar drops them first as it narrows.
		void set_comparing_dates_shown(bool aShown);
		/// \brief Places readiness in the open track or the folded comparison summary.
		void               set_readiness(QString aText, bool aTrackOpen);
		[[nodiscard]] auto readiness_text() const -> QString;
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
		void update_line();
		/// \brief [[BPR-1129]]. The "Comparing" button's words: the comparison in
		/// force and, while shown, its dates.
		void               update_comparing_text();
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
		epoch_history_model*                        HistoryModel = nullptr;
		epoch_line*                                 Line         = nullptr;
		/// [[BPR-1129]]. Guarded: once released it belongs to the review bar,
		/// which may be destroyed before this control.
		QPointer<QToolButton> ComparingButton;
		QLabel*               LineName            = nullptr;
		bool                  ComparingReleased   = false;
		bool                  ComparingDatesShown = true;
		QString               Readiness;
		bool                  TrackOpen               = true;
		QLabel*               ReadinessLabel          = nullptr;
		QWidget*              Details                 = nullptr;
		QComboBox*            LeftSelector            = nullptr;
		QComboBox*            RightSelector           = nullptr;
		QLineEdit*            LeftOid                 = nullptr;
		QLineEdit*            RightOid                = nullptr;
		QLabel*               StatusLabel             = nullptr;
		QLabel*               ComparisonLabel         = nullptr;
		QAction*              WholePrAction           = nullptr;
		QAction*              PreviousUploadAction    = nullptr;
		QAction*              LastReviewedAction      = nullptr;
		QAction*              LoadHistoryAction       = nullptr;
		QAction*              ReturnNewestAction      = nullptr;
		QAction*              RefreshAction           = nullptr;
		QAction*              CancelAction            = nullptr;
		QAction*              PruneAction             = nullptr;
		QAction*              DeleteCacheAction       = nullptr;
		QAction*              CancelMaintenanceAction = nullptr;
		QToolButton*          LoadHistoryButton       = nullptr;
		QToolButton*          ReturnNewestButton      = nullptr;
		QToolButton*          RefreshButton           = nullptr;
		QToolButton*          CancelButton            = nullptr;
		QToolButton*          MaintenanceButton       = nullptr;

		std::optional<core::epoch_occurrence_history>           OccurrenceHistory;
		bool                                                    AllowOccurrenceImport = false;
		std::optional<application::epoch_row_projection>        CurrentEpoch;
		std::optional<core::pull_request_authority>             Authority;
		std::optional<core::git_commit_oid>                     LastReviewCommit;
		std::optional<core::comparison_selection>               Selection;
		std::optional<application::epoch_comparison_projection> Comparison;
		/// [[BPR-1557]]. The pull request's base from the latest resolution,
		/// kept while a new selection resolves so the line does not lose it.
		std::optional<core::comparison_anchor>        PullRequestBase;
		std::optional<core::epoch_history_cursor>     Continuation;
		std::optional<core::epoch_history_navigation> HistoryNavigation;
		std::optional<core::epoch_history_page_phase> HistoryPhase;
		core::epoch_reason_code                       HistoryReason    = core::epoch_reason_code::none;
		core::epoch_reason_code                       DiagnosticReason = core::epoch_reason_code::none;
		QLabel*                                       LineSummary      = nullptr;
		core::epoch_status                            Status           = core::epoch_status::inactive;
		std::array<core::epoch_command_projection, 6> Commands{ };
		bool                                          HasCommands        = false;
		bool                                          ApplyingProjection = false;
		bool                                          HistoryWindowStale = false;
		QString                                       LastAnnouncement;
		epoch_comparison_control_statistics           Statistics;

		/// [[BPR-1340]]. Where focus went when the readiness line took the
		/// focused Updates line's place, so the line can have it back.
		QPointer<QWidget> LineFocusParkedOn;
		bool              LineFocusParked = false;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_COMPARISON_CONTROL_HPP
