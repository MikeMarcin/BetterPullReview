#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/qt/diff_link_map.hpp"

#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <memory>
#include <optional>

class QEvent;
class QLabel;
class QPlainTextEdit;
class QResizeEvent;
class QScrollBar;
class QSplitter;
class QToolButton;

namespace bpr::qt {
	/// \brief Records directly observable composed-workbench activity.
	struct diff_workbench_statistics
	{
		std::uint64_t Publications       = 0U;
		std::uint64_t Failures           = 0U;
		std::uint64_t Invalidations      = 0U;
		std::uint64_t GeometryRebuilds   = 0U;
		std::uint64_t GeometryFailures   = 0U;
		std::uint64_t FrameApplications  = 0U;
		std::uint64_t SelectionChanges   = 0U;
		std::uint64_t RejectedStaleInput = 0U;

		auto operator<=>(const diff_workbench_statistics&) const = default;
	};

	/// \brief Presents one immutable computed diff through the selected Qt path.
	class diff_workbench final
		: public QWidget,
		  public application::diff_projection_port
	{
	public:
		explicit diff_workbench(QWidget* aParent = nullptr);
		~diff_workbench() override;

		diff_workbench(const diff_workbench&)                    = delete;
		auto operator=(const diff_workbench&) -> diff_workbench& = delete;
		diff_workbench(diff_workbench&&)                         = delete;
		auto operator=(diff_workbench&&) -> diff_workbench&      = delete;

		void publish(const application::diff_publication& aPublication) override;
		void fail(const application::diff_failure_publication& aFailure) override;
		void invalidate(const core::pull_request_authority& aAuthority) override;

		/// \brief Clears stale geometry and presents an exact pending context.
		void set_loading(const core::diff_request_context& aContext);
		void clear();

		[[nodiscard]] auto left_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto right_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto detail_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto previous_change_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto next_change_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto shared_scrollbar() const noexcept -> QScrollBar*;
		[[nodiscard]] auto comparison_splitter() const noexcept
			-> diff_comparison_splitter*;
		[[nodiscard]] auto link_map() const noexcept -> diff_link_map*;
		[[nodiscard]] auto scroll_controller() const noexcept
			-> comparison_scroll_controller*;
		[[nodiscard]] auto selection() const noexcept
			-> const std::optional<core::semantic_selection>&;
		[[nodiscard]] auto current_context() const noexcept
			-> const std::optional<core::diff_request_context>&;
		[[nodiscard]] auto current_result() const noexcept
			-> const std::shared_ptr<const core::computed_file_diff>&;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> diff_workbench_statistics;

	protected:
		[[nodiscard]] auto event(QEvent* aEvent) -> bool override;
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent)
			-> bool override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void schedule_geometry_rebuild(measured_geometry_change aChange);
		void rebuild_geometry();
		void apply_frame(const comparison_scroll_frame& aFrame);
		void select_line(core::diff_side aSide, std::uint32_t aLine);
		void select_link(const diff_link_map_identity& aIdentity, bool aReveal);
		void navigate(core::change_navigation_direction aDirection);
		void apply_interaction(bool aReveal, bool aAnnounceSelection);
		void update_detail();
		void update_selection_paint();
		void update_action_state();
		void announce_accessible_status(QString aText);
		void update_status(QString aText);
		void clear_projection(QString aStatus);

		QToolButton*                  PreviousChange = nullptr;
		QToolButton*                  NextChange     = nullptr;
		QLabel*                       Status         = nullptr;
		diff_comparison_splitter*     Comparison     = nullptr;
		QPlainTextEdit*               Left           = nullptr;
		QPlainTextEdit*               Right          = nullptr;
		QScrollBar*                   SharedScroll   = nullptr;
		comparison_scroll_controller* Controller     = nullptr;
		diff_link_map*                LinkMap        = nullptr;
		QSplitter*                    Vertical       = nullptr;
		QPlainTextEdit*               Detail         = nullptr;

		std::optional<core::diff_request_context>       CurrentContext;
		std::shared_ptr<const core::computed_file_diff> CurrentResult;
		core::diff_interaction_state                    Interaction;
		measured_alignment_axis_cache                   AxisCache;
		diff_workbench_statistics                       Statistics;
		QString                                         LastAnnouncement;
		bool                                            GeometryScheduled = false;
		bool                                            ApplyingFrame     = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP
