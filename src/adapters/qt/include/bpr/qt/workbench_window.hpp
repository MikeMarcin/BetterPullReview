#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP

#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"

#include <QMainWindow>
#include <QString>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QAction;
class QEvent;
class QLabel;
class QMenu;
class QResizeEvent;
class QToolBar;
class QToolButton;

namespace bpr::qt {
	/// \brief Carries one semantic workbench gesture out of the presentation layer.
	using workbench_intent_dispatch = std::function<void(core::workbench_interaction_intent)>;

	/// \brief Carries one semantic change-navigation gesture out of the toolbar.
	///
	/// \details Change navigation is an RFC-BPR-0004 diff gesture rather than a
	/// workbench-state gesture, so it leaves through its own sink instead of being
	/// folded into `core::workbench_interaction_intent`.
	using workbench_change_dispatch = std::function<void(core::navigate_diff_change_intent)>;

	/// \brief Describes one decided change-navigation command projection.
	///
	/// \details `core::navigate_change` is the single owner of change legality.
	/// This record carries that decision to the toolbar unchanged: `Enabled` is the
	/// core's answer about whether the gesture would reach another change, and
	/// `Outcome` is the exact core outcome the toolbar renders as the disabled
	/// reason. The window recomputes neither value.
	struct workbench_change_command_projection
	{
		bool                            Enabled = false;
		core::change_navigation_outcome Outcome = core::change_navigation_outcome::no_changes;

		auto operator<=>(const workbench_change_command_projection&) const = default;
	};

	/// \brief Describes the decided change-navigation delta for the toolbar.
	///
	/// \details `Policy` is the retained stop-versus-wrap policy the decision was
	/// taken under. The toolbar repeats it in the intent it emits, so the gesture
	/// and the enablement that offered it can never disagree.
	struct workbench_change_navigation_delta
	{
		workbench_change_command_projection Previous;
		workbench_change_command_projection Next;
		core::change_navigation_policy      Policy = core::change_navigation_policy::stop;

		auto operator<=>(const workbench_change_navigation_delta&) const = default;
	};

	enum class workbench_toolbar_mode : std::uint8_t {
		expanded,
		compact,
	};

	/// \brief Records directly observable workbench-window presentation activity.
	struct workbench_window_statistics
	{
		std::uint64_t Publications       = 0U;
		std::uint64_t Failures           = 0U;
		std::uint64_t CommandUpdates     = 0U;
		std::uint64_t ToolbarModeChanges = 0U;
		std::uint64_t Announcements      = 0U;

		auto operator<=>(const workbench_window_statistics&) const = default;
	};

	/// \brief Presents the native workbench shell and its primary review toolbar.
	///
	/// \details The window is an event and action adapter, never a controller. Each
	/// toolbar action translates one gesture into a semantic intent and every
	/// enabled state arrives as a decided projection; the window contains no
	/// navigation legality of its own and performs no GitHub, Git, SQLite,
	/// filesystem, network, or process work.
	///
	/// Primary navigation stays top-level while the window is at least
	/// compact_width_threshold() wide. Below that it moves into an explicit compact
	/// overflow control. The actions stay associated with the window in both modes,
	/// so their window shortcuts remain live and the overflow itself is focusable
	/// and mnemonic-addressable; navigation never silently disappears.
	///
	/// The central surface is a shell. The drawers, epoch selectors, comparison
	/// surface, and lower detail pane are named empty placeholders owned by later
	/// RFC-BPR-0005 cards.
	class workbench_window final
		: public QMainWindow,
		  public application::workbench_projection_port
	{
	public:
		explicit workbench_window(
			workbench_intent_dispatch aDispatch       = { },
			workbench_change_dispatch aChangeDispatch = { },
			QWidget*                  aParent         = nullptr);
		~workbench_window() override;

		workbench_window(const workbench_window&)                    = delete;
		auto operator=(const workbench_window&) -> workbench_window& = delete;
		workbench_window(workbench_window&&)                         = delete;
		auto operator=(workbench_window&&) -> workbench_window&      = delete;

		void publish(const application::workbench_projection& aProjection) override;
		void fail(
			const application::workbench_failure_publication& aFailure) override;

		/// \brief Adopts the decided change-navigation command states.
		void publish_change_navigation(
			const workbench_change_navigation_delta& aDelta);

		void set_dispatch(workbench_intent_dispatch aDispatch);
		void set_change_dispatch(workbench_change_dispatch aChangeDispatch);

		[[nodiscard]] auto primary_toolbar() const noexcept -> QToolBar*;
		[[nodiscard]] auto previous_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto previous_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto next_change_action() const noexcept -> QAction*;
		[[nodiscard]] auto navigation_overflow_button() const noexcept
			-> QToolButton*;
		[[nodiscard]] auto navigation_overflow_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto toolbar_mode() const noexcept -> workbench_toolbar_mode;

		/// \brief Computes the narrowest width that still fits top-level navigation.
		///
		/// \details Derived from the current style and font only, never from the
		/// current toolbar mode, so entering the compact overflow cannot change the
		/// answer that put the toolbar there.
		[[nodiscard]] auto compact_width_threshold() const -> int;

		[[nodiscard]] auto files_drawer_placeholder() const noexcept -> QWidget*;
		[[nodiscard]] auto conversations_drawer_placeholder() const noexcept
			-> QWidget*;
		[[nodiscard]] auto epoch_selector_placeholder() const noexcept -> QWidget*;
		[[nodiscard]] auto comparison_surface_placeholder() const noexcept
			-> QWidget*;
		[[nodiscard]] auto detail_pane_placeholder() const noexcept -> QWidget*;

		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> workbench_window_statistics;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief Binds one primary action to its presentation wording.
		struct navigation_entry
		{
			QAction* Action = nullptr;
			QString  Name;
			QString  Purpose;
		};

		void build_primary_toolbar();
		void build_central_placeholders();
		void dispatch_navigation(std::size_t aIndex);
		void apply_layout_mode();
		void configure_tool_buttons();
		void update_action_state();
		void update_status_text();
		void announce_update();

		[[nodiscard]] auto command(core::workbench_command_id aId) const
			-> std::optional<core::workbench_command_projection>;

		workbench_intent_dispatch Dispatch;
		workbench_change_dispatch ChangeDispatch;

		QToolBar*    PrimaryToolbar      = nullptr;
		QAction*     SeparatorAction     = nullptr;
		QAction*     OverflowAction      = nullptr;
		QToolButton* OverflowButton      = nullptr;
		QMenu*       OverflowMenu        = nullptr;
		QLabel*      StatusLabel         = nullptr;
		QWidget*     FilesDrawer         = nullptr;
		QWidget*     ConversationsDrawer = nullptr;
		QWidget*     EpochSelectors      = nullptr;
		QWidget*     ComparisonSurface   = nullptr;
		QWidget*     DetailPane          = nullptr;

		std::array<navigation_entry, 4>                    Navigation{ };
		core::workbench_command_delta                      Commands;
		std::optional<core::workbench_navigation_cursor>   Cursor;
		std::optional<workbench_change_navigation_delta>   ChangeNavigation;
		std::optional<application::workbench_host_failure> Failure;
		workbench_toolbar_mode                             Mode              = workbench_toolbar_mode::expanded;
		bool                                               HasCommands       = false;
		bool                                               ToolbarConfigured = false;
		QString                                            LastAnnouncement;
		workbench_window_statistics                        Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_WINDOW_HPP
