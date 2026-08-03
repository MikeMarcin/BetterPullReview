#include "bpr/qt/workbench_window.hpp"

#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"

#include <QAccessible>
#include <QAction>
#include <QComboBox>
#include <QFontMetrics>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace bpr::qt {
	namespace {
		// The narrowest code column the central review surface is worth keeping.
		// Primary navigation moves into the compact overflow rather than eating
		// into it, which is what "narrow width" means for this window.
		constexpr int reserved_central_columns = 80;

		// The toolbar's first entries are the file actions and the rest are the
		// change actions, in the core's own command order.
		constexpr std::size_t file_navigation_entries = 2U;

		// The navigation entries are exactly the core's navigation commands in the
		// core's order, so entry `i` reads `workbench_command_id{ i }` and the
		// window needs no second mapping between the two.
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::previous_file) == 0U);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::next_file) == 1U);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::previous_change)
			== file_navigation_entries);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::next_change) == 3U);

		[[nodiscard]] auto reason_text(const core::workbench_reason_code aReason)
			-> QString
		{
			switch (aReason) {
			case core::workbench_reason_code::none:
				return { };
			case core::workbench_reason_code::no_active_workbench:
				return QStringLiteral("Open a pull request to review its files.");
			case core::workbench_reason_code::no_actionable_files:
				return QStringLiteral("This pull request has no actionable files.");
			case core::workbench_reason_code::at_first_file:
				return QStringLiteral("This is the first actionable file.");
			case core::workbench_reason_code::at_last_file:
				return QStringLiteral("This is the last actionable file.");
			case core::workbench_reason_code::invalid_file_identity:
				return QStringLiteral("The selected file identity is invalid.");
			case core::workbench_reason_code::invalid_navigation_cursor:
				return QStringLiteral("The file navigation position is invalid.");
			case core::workbench_reason_code::no_open_comparison:
				return QStringLiteral("No file comparison is open.");
			case core::workbench_reason_code::no_changes:
				return QStringLiteral(
					"This comparison has no changes to navigate.");
			case core::workbench_reason_code::at_first_change:
				return QStringLiteral(
					"This is the first change in this comparison.");
			case core::workbench_reason_code::at_last_change:
				return QStringLiteral(
					"This is the last change in this comparison.");
			case core::workbench_reason_code::invalid_change_selection:
				return QStringLiteral("The selected change is no longer available.");
			case core::workbench_reason_code::invalid_change_cursor:
				return QStringLiteral("The change navigation position is invalid.");
			}
			// Deliberately outside the switch and with no default label, so a
			// reason the core adds later is a compile error here rather than a
			// silently generic sentence in the toolbar.
			return QStringLiteral("Review navigation is unavailable.");
		}

		// The drawer commands share the workbench reason vocabulary, so only the
		// reason a drawer can actually be refused for needs drawer wording.
		[[nodiscard]] auto drawer_reason_text(
			const core::workbench_reason_code aReason) -> QString
		{
			if (aReason == core::workbench_reason_code::no_active_workbench) {
				return QStringLiteral(
					"Open a pull request to use its review drawers.");
			}
			return reason_text(aReason);
		}

		// The change commands share the workbench reason vocabulary, so only the
		// reason whose shared wording is file-flavoured needs a change sentence.
		[[nodiscard]] auto change_reason_text(
			const core::workbench_reason_code aReason) -> QString
		{
			if (aReason == core::workbench_reason_code::no_active_workbench) {
				return QStringLiteral(
					"Open a pull request to review its changes.");
			}
			return reason_text(aReason);
		}

		[[nodiscard]] auto failure_text(
			const application::workbench_host_failure aFailure) -> QString
		{
			switch (aFailure) {
			case application::workbench_host_failure::observation_unavailable:
				return QStringLiteral("The actionable file list is unavailable.");
			case application::workbench_host_failure::observation_limit:
				return QStringLiteral(
					"This pull request has more actionable files than the workbench "
					"retains.");
			case application::workbench_host_failure::settings_unavailable:
				return QStringLiteral("The saved workbench layout is unavailable.");
			case application::workbench_host_failure::host_failure:
				return QStringLiteral("The workbench could not be updated.");
			}
			return QStringLiteral("The workbench could not be updated.");
		}

		void describe_action(
			QAction* const aAction,
			const bool     aEnabled,
			const QString& aEnabledText,
			const QString& aDisabledText)
		{
			aAction->setEnabled(aEnabled);
			const auto description = aEnabled ? aEnabledText : aDisabledText;
			aAction->setToolTip(description);
			aAction->setStatusTip(description);
		}
	}

	workbench_window::workbench_window(
		workbench_intent_dispatch       aDispatch,
		workbench_change_dispatch       aChangeDispatch,
		workbench_drawer_width_dispatch aWidthDispatch,
		QWidget* const                  aParent)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, ChangeDispatch{ std::move(aChangeDispatch) }
		, WidthDispatch{ std::move(aWidthDispatch) }
	{
		setObjectName(QStringLiteral("workbenchWindow"));
		setWindowTitle(QStringLiteral("BetterPullReview"));
		// Deliberately narrower than compact_width_threshold(). A supported width
		// below the natural navigation width is exactly what the explicit compact
		// overflow exists for.
		setMinimumSize(480, 360);
		resize(1'280, 820);

		build_primary_toolbar();
		build_drawer_toolbar();
		build_central_surface();

		StatusLabel = new QLabel{ this };
		StatusLabel->setObjectName(QStringLiteral("workbenchStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("Workbench status"));
		StatusLabel->setAccessibleDescription(QStringLiteral(
			"Selected file position, host failures, and review-history state in "
			"text"));
		StatusLabel->setWordWrap(true);
		StatusLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard
											 | Qt::TextSelectableByMouse);
		statusBar()->addWidget(StatusLabel, 1);

		apply_layout_mode();
		update_action_state();
		update_status_text();
	}

	workbench_window::~workbench_window() = default;

	void workbench_window::build_primary_toolbar()
	{
		PrimaryToolbar = addToolBar(QStringLiteral("Review navigation"));
		PrimaryToolbar->setObjectName(QStringLiteral("workbenchPrimaryToolbar"));
		PrimaryToolbar->setAccessibleName(
			QStringLiteral("Primary review navigation"));
		PrimaryToolbar->setAccessibleDescription(QStringLiteral(
			"Move between actionable pull-request files and semantic changes"));
		PrimaryToolbar->setMovable(false);
		PrimaryToolbar->setFloatable(false);
		PrimaryToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

		const auto make = [this](const QString&      aText,
								 const QString&      aName,
								 const QString&      aAccessible,
								 const QString&      aObjectName,
								 const QString&      aPurpose,
								 const QKeySequence& aShortcut) -> navigation_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setShortcut(aShortcut);
			// Window context keeps the shortcut live from anywhere in the window,
			// including while the action lives in the compact overflow.
			action->setShortcutContext(Qt::WindowShortcut);
			action->setIconText(aName);
			addAction(action);
			return {
				.Action     = action,
				.Name       = aName,
				.Accessible = aAccessible,
				.Purpose    = aPurpose,
			};
		};

		Navigation[0] = make(QStringLiteral("&Previous file"),
							 QStringLiteral("Previous file"),
							 QStringLiteral("Previous file"),
							 QStringLiteral("previousWorkbenchFileAction"),
							 QStringLiteral("Select the previous actionable file."),
							 QKeySequence{ QStringLiteral("Alt+Up") });
		Navigation[1] = make(QStringLiteral("&Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("nextWorkbenchFileAction"),
							 QStringLiteral("Select the next actionable file."),
							 QKeySequence{ QStringLiteral("Alt+Down") });
		// The composed comparison surface offers its own in-surface previous and
		// next change buttons, which already answer to "Previous change" and
		// "Next change". Two focusable widgets answering to one assistive name in
		// one window is an accessibility defect, so the top-level toolbar states
		// which pair it is.
		Navigation[2] = make(QStringLiteral("Pre&vious change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("Previous review change"),
							 QStringLiteral("previousWorkbenchChangeAction"),
							 QStringLiteral("Select the previous semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgUp") });
		Navigation[3] = make(QStringLiteral("Ne&xt change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("Next review change"),
							 QStringLiteral("nextWorkbenchChangeAction"),
							 QStringLiteral("Select the next semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgDown") });

		PrimaryToolbar->addAction(Navigation[0].Action);
		PrimaryToolbar->addAction(Navigation[1].Action);
		SeparatorAction = PrimaryToolbar->addSeparator();
		PrimaryToolbar->addAction(Navigation[2].Action);
		PrimaryToolbar->addAction(Navigation[3].Action);

		OverflowMenu = new QMenu{ this };
		OverflowMenu->setObjectName(
			QStringLiteral("workbenchNavigationOverflowMenu"));
		// Named apart from the button that opens it: the button and its popup are
		// two widgets, and one assistive name cannot mean both.
		OverflowMenu->setAccessibleName(
			QStringLiteral("Review navigation overflow"));
		OverflowMenu->setAccessibleDescription(QStringLiteral(
			"Primary review navigation moved into the compact overflow"));
		OverflowButton = new QToolButton{ this };
		OverflowButton->setObjectName(
			QStringLiteral("workbenchNavigationOverflowButton"));
		OverflowButton->setText(QStringLiteral("&Navigation"));
		// setText derives a mnemonic, and platforms that do not use Alt mnemonics
		// derive an empty one. Set the shortcut explicitly afterwards so the
		// compact overflow is keyboard-openable everywhere.
		OverflowButton->setShortcut(QKeySequence{ QStringLiteral("Alt+N") });
		OverflowButton->setAccessibleName(QStringLiteral("Review navigation"));
		OverflowButton->setAccessibleDescription(QStringLiteral(
			"Open previous and next file and change navigation"));
		OverflowButton->setToolTip(
			QStringLiteral("Primary review navigation for narrow windows."));
		OverflowButton->setStatusTip(OverflowButton->toolTip());
		OverflowButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		OverflowButton->setPopupMode(QToolButton::InstantPopup);
		OverflowButton->setFocusPolicy(Qt::StrongFocus);
		OverflowButton->setMenu(OverflowMenu);
		OverflowAction = PrimaryToolbar->addWidget(OverflowButton);
		OverflowAction->setObjectName(
			QStringLiteral("workbenchNavigationOverflowAction"));
		OverflowAction->setVisible(false);

		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			connect(Navigation[index].Action,
					&QAction::triggered,
					this,
					[this, index] { dispatch_navigation(index); });
		}
	}

	void workbench_window::build_drawer_toolbar()
	{
		// A separate row. The drawer toggles are permanently top-level, so putting
		// them beside primary navigation would let them push navigation past the
		// width compact_width_threshold() promises it fits in.
		addToolBarBreak(Qt::TopToolBarArea);
		DrawerToolbar = addToolBar(QStringLiteral("Review drawers"));
		DrawerToolbar->setObjectName(QStringLiteral("workbenchDrawerToolbar"));
		DrawerToolbar->setAccessibleName(QStringLiteral("Review drawers"));
		DrawerToolbar->setAccessibleDescription(QStringLiteral(
			"Show or hide the file and conversation drawers"));
		DrawerToolbar->setMovable(false);
		DrawerToolbar->setFloatable(false);
		DrawerToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

		const auto make = [this](const QString&      aText,
								 const QString&      aName,
								 const QString&      aAccessible,
								 const QString&      aObjectName,
								 const QString&      aCollapse,
								 const QString&      aExpand,
								 const QKeySequence& aShortcut) -> drawer_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setCheckable(true);
			action->setShortcut(aShortcut);
			action->setShortcutContext(Qt::WindowShortcut);
			action->setIconText(aName);
			addAction(action);
			DrawerToolbar->addAction(action);
			return {
				.Toggle     = action,
				.Name       = aName,
				.Accessible = aAccessible,
				.Collapse   = aCollapse,
				.Expand     = aExpand,
			};
		};

		// Alt+1 through Alt+3 belong to the composed review-history control's
		// named comparisons, which are widget-context shortcuts. A window-context
		// duplicate would be ambiguous exactly while that control has focus, and
		// Qt answers an ambiguous overload by triggering neither action.
		Drawers[0] = make(
			QStringLiteral("&Files drawer"),
			QStringLiteral("Files drawer"),
			QStringLiteral("Files drawer toggle"),
			QStringLiteral("toggleWorkbenchFilesDrawerAction"),
			QStringLiteral("Hide the file drawer and give its width to the "
						   "comparison."),
			QStringLiteral("Show the file drawer at its saved width."),
			QKeySequence{ QStringLiteral("Alt+Shift+1") });
		Drawers[1] = make(
			QStringLiteral("&Conversations drawer"),
			QStringLiteral("Conversations drawer"),
			QStringLiteral("Conversations drawer toggle"),
			QStringLiteral("toggleWorkbenchConversationsDrawerAction"),
			QStringLiteral("Hide the conversation drawer and give its width to "
						   "the comparison."),
			QStringLiteral("Show the conversation drawer at its saved width."),
			QKeySequence{ QStringLiteral("Alt+Shift+2") });

		for (std::size_t index = 0U; index < Drawers.size(); ++index) {
			connect(Drawers[index].Toggle,
					&QAction::triggered,
					this,
					[this, index] { dispatch_drawer_toggle(index); });
		}
	}

	void workbench_window::build_central_surface()
	{
		CentralSplitter = new QSplitter{ Qt::Horizontal, this };
		CentralSplitter->setObjectName(QStringLiteral("workbenchCentralSplitter"));
		CentralSplitter->setAccessibleName(QStringLiteral("Review workbench"));
		CentralSplitter->setAccessibleDescription(QStringLiteral(
			"Resizable file drawer, comparison centre, and conversation drawer"));
		// A drawer leaves through the core's drawer state, never through a drag
		// to zero width that presentation decided by itself.
		CentralSplitter->setChildrenCollapsible(false);

		CentreSurface = new QWidget;
		CentreSurface->setObjectName(QStringLiteral("workbenchCentralSurface"));
		CentreSurface->setAccessibleName(QStringLiteral("Review comparison"));
		CentreSurface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		// RFC-BPR-0003's review-history control is composed unchanged above the
		// panes. It already owns the independent left and right selectors over
		// arbitrary retained epochs, the complete copyable commit OIDs, and the
		// review-history loading, failure, cancellation, and empty wording, so
		// the shell adds no second selector, OID presentation, or vocabulary.
		EpochSelectors = new epoch_comparison_control{ { }, CentreSurface };
		// An ignored horizontal hint, exactly as the drawer heading uses. The
		// centre must never impose a minimum width that the drawer widths the
		// host persists cannot coexist with, and the window must stay able to
		// reach the supported widths the compact overflow exists for.
		EpochSelectors->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		// RFC-BPR-0004's workbench is composed unchanged. It keeps the single
		// shared vertical scrollbar, the single link map, and the natural-layout
		// documents, so the centre gains no second mapping, no regrouping, no
		// follower-scroll coupling, and no synthetic alignment rows.
		ComparisonSurface  = new diff_workbench{ CentreSurface };
		auto* const centre = new QVBoxLayout{ CentreSurface };
		centre->setContentsMargins(0, 0, 0, 0);
		centre->addWidget(EpochSelectors);
		centre->addWidget(ComparisonSurface, 1);

		Drawers[0].Panel = new workbench_drawer_panel{ core::workbench_drawer::files };
		Drawers[1].Panel = new workbench_drawer_panel{
			core::workbench_drawer::conversations
		};
		CentralSplitter->addWidget(Drawers[0].Panel);
		CentralSplitter->addWidget(CentreSurface);
		CentralSplitter->addWidget(Drawers[1].Panel);
		// Only the centre absorbs a window resize, so a drawer keeps the width the
		// host persisted until the user moves the handle.
		CentralSplitter->setStretchFactor(0, 0);
		CentralSplitter->setStretchFactor(1, 1);
		CentralSplitter->setStretchFactor(2, 0);

		for (auto& entry : Drawers) {
			entry.Panel->set_width_dispatch(
				[this](const core::workbench_drawer aDrawer,
					   const std::uint32_t          aWidth) {
					observe_drawer_width(aDrawer, aWidth);
				});
		}
		setCentralWidget(CentralSplitter);
	}

	void workbench_window::publish(
		const application::workbench_projection& aProjection)
	{
		if (aProjection.Commands) {
			Commands    = *aProjection.Commands;
			HasCommands = true;
			++Statistics.CommandUpdates;
		}
		if (aProjection.Selection) {
			Cursor = aProjection.Selection->Cursor;
		}
		if (aProjection.Drawers) {
			DrawerStates = *aProjection.Drawers;
			++Statistics.DrawerUpdates;
		}
		if (aProjection.Layout) {
			LayoutWidths = *aProjection.Layout;
			++Statistics.LayoutUpdates;
		}
		if (aProjection.Drawers || aProjection.Layout) {
			apply_drawer_layout();
		}
		Failure.reset();
		++Statistics.Publications;
		update_action_state();
		update_status_text();
		announce_update();
	}

	void workbench_window::publish(
		const application::epoch_presentation_update& aUpdate)
	{
		// The composed control owns every review-history decision, so the update
		// reaches it unchanged and the shell only quotes the wording it produced.
		EpochSelectors->publish(aUpdate);
		HasEpochState = true;
		++Statistics.EpochUpdates;
		update_status_text();
		// Deliberately silent. The composed control already announced this exact
		// change, and a second announcement of the same fact is the duplicated
		// chatter a combined status update exists to prevent.
	}

	void workbench_window::fail(
		const application::workbench_failure_publication& aFailure)
	{
		// A failed effect changes no command legality. The host publishes a command
		// delta when legality actually changed, so the window reports the failure in
		// text and leaves enabled state exactly where the last projection put it.
		Failure = aFailure.Failure;
		++Statistics.Failures;
		update_status_text();
		announce_update();
	}

	void workbench_window::set_dispatch(workbench_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void workbench_window::set_change_dispatch(
		workbench_change_dispatch aChangeDispatch)
	{
		ChangeDispatch = std::move(aChangeDispatch);
	}

	void workbench_window::set_width_dispatch(
		workbench_drawer_width_dispatch aWidthDispatch)
	{
		WidthDispatch = std::move(aWidthDispatch);
	}

	auto workbench_window::primary_toolbar() const noexcept -> QToolBar*
	{
		return PrimaryToolbar;
	}

	auto workbench_window::previous_file_action() const noexcept -> QAction*
	{
		return Navigation[0].Action;
	}

	auto workbench_window::next_file_action() const noexcept -> QAction*
	{
		return Navigation[1].Action;
	}

	auto workbench_window::previous_change_action() const noexcept -> QAction*
	{
		return Navigation[2].Action;
	}

	auto workbench_window::next_change_action() const noexcept -> QAction*
	{
		return Navigation[3].Action;
	}

	auto workbench_window::navigation_overflow_button() const noexcept
		-> QToolButton*
	{
		return OverflowButton;
	}

	auto workbench_window::navigation_overflow_menu() const noexcept -> QMenu*
	{
		return OverflowMenu;
	}

	auto workbench_window::toolbar_mode() const noexcept -> workbench_toolbar_mode
	{
		return Mode;
	}

	auto workbench_window::compact_width_threshold() const -> int
	{
		const auto  metrics      = PrimaryToolbar->fontMetrics();
		auto* const toolbarStyle = PrimaryToolbar->style();
		const auto  margin       = toolbarStyle->pixelMetric(
			QStyle::PM_ButtonMargin, nullptr, PrimaryToolbar);
		const auto spacing = toolbarStyle->pixelMetric(
			QStyle::PM_ToolBarItemSpacing, nullptr, PrimaryToolbar);
		const auto item = toolbarStyle->pixelMetric(
			QStyle::PM_ToolBarItemMargin, nullptr, PrimaryToolbar);
		auto width = 2
			* toolbarStyle->pixelMetric(
				QStyle::PM_ToolBarFrameWidth, nullptr, PrimaryToolbar);
		for (const auto& entry : Navigation) {
			width += metrics.horizontalAdvance(entry.Name) + (2 * margin)
				+ (2 * item) + spacing;
		}
		return width
			+ (reserved_central_columns
			   * metrics.horizontalAdvance(QLatin1Char('0')));
	}

	auto workbench_window::drawer_toolbar() const noexcept -> QToolBar*
	{
		return DrawerToolbar;
	}

	auto workbench_window::files_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[0].Panel;
	}

	auto workbench_window::conversations_drawer() const noexcept
		-> workbench_drawer_panel*
	{
		return Drawers[1].Panel;
	}

	auto workbench_window::files_drawer_toggle_action() const noexcept -> QAction*
	{
		return Drawers[0].Toggle;
	}

	auto workbench_window::conversations_drawer_toggle_action() const noexcept
		-> QAction*
	{
		return Drawers[1].Toggle;
	}

	auto workbench_window::epoch_selectors() const noexcept
		-> epoch_comparison_control*
	{
		return EpochSelectors;
	}

	auto workbench_window::focus_order() const -> QList<QWidget*>
	{
		auto       order  = QList<QWidget*>{ };
		const auto append = [&order](QWidget* const aWidget) {
			// A stop that cannot take focus is not an interactive stop. Skipping
			// it keeps the declared order equal to the order a keyboard user can
			// actually walk.
			if (aWidget != nullptr && aWidget->focusPolicy() != Qt::NoFocus) {
				order.push_back(aWidget);
			}
		};

		if (Mode == workbench_toolbar_mode::compact) {
			append(OverflowButton);
		} else {
			for (const auto& entry : Navigation) {
				append(qobject_cast<QToolButton*>(
					PrimaryToolbar->widgetForAction(entry.Action)));
			}
		}
		for (const auto& entry : Drawers) {
			append(qobject_cast<QToolButton*>(
				DrawerToolbar->widgetForAction(entry.Toggle)));
		}
		append(Drawers[0].Panel->contents_view());
		// The two selectors sit on one grid row above their two exact OIDs, so
		// the declared order is the row order the composed control already walks.
		append(EpochSelectors->left_selector());
		append(EpochSelectors->right_selector());
		append(EpochSelectors->left_oid_editor());
		append(EpochSelectors->right_oid_editor());
		// One stop stands for the whole composed comparison surface: its last.
		// Everything before it inside that surface is RFC-BPR-0004's declared
		// order, which the shell reports through that card rather than restating
		// here.
		append(ComparisonSurface->detail_pane());
		append(Drawers[1].Panel->contents_view());
		append(StatusLabel);
		return order;
	}

	auto workbench_window::comparison_surface() const noexcept -> diff_workbench*
	{
		return ComparisonSurface;
	}

	auto workbench_window::detail_pane() const noexcept -> QPlainTextEdit*
	{
		return ComparisonSurface->detail_pane();
	}

	auto workbench_window::status_text() const -> QString
	{
		return StatusLabel->text();
	}

	auto workbench_window::statistics() const noexcept
		-> workbench_window_statistics
	{
		return Statistics;
	}

	void workbench_window::changeEvent(QEvent* const aEvent)
	{
		QMainWindow::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::ApplicationPaletteChange:
		case QEvent::PaletteChange:
		case QEvent::ApplicationFontChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
			// The threshold is style- and font-derived, so a native appearance
			// change can legitimately move the window across it.
			apply_layout_mode();
			break;
		default:
			break;
		}
	}

	void workbench_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QMainWindow::resizeEvent(aEvent);
		apply_layout_mode();
	}

	void workbench_window::dispatch_navigation(const std::size_t aIndex)
	{
		if (aIndex < file_navigation_entries) {
			if (Dispatch) {
				Dispatch(core::navigate_workbench_file_intent{
					.Direction = aIndex == 0U
						? core::change_navigation_direction::previous
						: core::change_navigation_direction::next,
				});
			}
			return;
		}
		if (!ChangeDispatch) {
			return;
		}
		// The gesture repeats the policy the projection offered it, so the
		// emitted intent and the enablement that offered it cannot disagree about
		// stop versus wrap.
		ChangeDispatch(core::navigate_diff_change_intent{
			.Direction = aIndex == file_navigation_entries
				? core::change_navigation_direction::previous
				: core::change_navigation_direction::next,
			.Policy    = Commands.Policy,
		});
	}

	void workbench_window::dispatch_drawer_toggle(const std::size_t aIndex)
	{
		// A checkable action flips itself the moment it is triggered. The core owns
		// drawer state, so the last decided state is re-asserted immediately and
		// only the gesture leaves; the window never shows a state it decided.
		update_action_state();
		if (!Dispatch) {
			return;
		}
		Dispatch(core::toggle_workbench_drawer_intent{
			.Drawer = aIndex == 0U ? core::workbench_drawer::files
								   : core::workbench_drawer::conversations,
		});
	}

	void workbench_window::observe_drawer_width(
		const core::workbench_drawer aDrawer,
		const std::uint32_t          aWidth)
	{
		// Geometry this window just applied is not a user resize. Reporting it
		// would let a width the window could merely grant overwrite the width the
		// host persisted.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		auto& retained = aDrawer == core::workbench_drawer::files
			? LayoutWidths->FilesWidth
			: LayoutWidths->ConversationsWidth;
		if (retained == aWidth) {
			return;
		}
		// The host owns and clamps the durable width. This local echo only keeps
		// one observed width from being reported twice before it is published back.
		retained = aWidth;
		if (WidthDispatch) {
			WidthDispatch(aDrawer, aWidth);
		}
	}

	void workbench_window::apply_drawer_layout()
	{
		ApplyingLayout = true;
		Drawers[0].Panel->adopt(DrawerStates.Files);
		Drawers[1].Panel->adopt(DrawerStates.Conversations);
		if (LayoutWidths) {
			CentralSplitter->setSizes(central_sizes());
		}
		ApplyingLayout = false;
	}

	auto workbench_window::central_sizes() const -> QList<int>
	{
		const auto granted = [](const std::uint32_t aWidth) {
			// An arithmetic guard on a host-owned value, not a second clamp: the
			// host already refuses to retain a width outside its own bound.
			return static_cast<int>(
				std::min(aWidth, application::maximum_workbench_drawer_width));
		};
		const auto files = DrawerStates.Files == core::workbench_drawer_state::expanded
			? granted(LayoutWidths->FilesWidth)
			: 0;
		const auto conversations
			= DrawerStates.Conversations == core::workbench_drawer_state::expanded
			? granted(LayoutWidths->ConversationsWidth)
			: 0;
		const auto handles = 2 * CentralSplitter->handleWidth();
		const auto centre  = std::max(
			1, CentralSplitter->width() - files - conversations - handles);
		return { files, centre, conversations };
	}

	void workbench_window::apply_layout_mode()
	{
		const auto mode = width() < compact_width_threshold()
			? workbench_toolbar_mode::compact
			: workbench_toolbar_mode::expanded;
		if (ToolbarConfigured && mode == Mode) {
			return;
		}
		Mode              = mode;
		ToolbarConfigured = true;

		for (const auto& entry : Navigation) {
			PrimaryToolbar->removeAction(entry.Action);
			OverflowMenu->removeAction(entry.Action);
		}
		const auto compact = Mode == workbench_toolbar_mode::compact;
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			auto* const action = Navigation[index].Action;
			if (compact) {
				OverflowMenu->addAction(action);
			} else {
				// File actions sit before the separator, change actions after it.
				PrimaryToolbar->insertAction(index < file_navigation_entries
												 ? SeparatorAction
												 : OverflowAction,
											 action);
			}
		}
		SeparatorAction->setVisible(!compact);
		OverflowAction->setVisible(compact);
		configure_tool_buttons();
		// A mode change destroys and recreates the toolbar buttons, so the
		// declared order has to be reasserted over the widgets that now exist.
		apply_focus_order();
		++Statistics.ToolbarModeChanges;
	}

	void workbench_window::configure_tool_buttons()
	{
		for (const auto& entry : Navigation) {
			auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Action));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Purpose);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		for (const auto& entry : Drawers) {
			auto* const button = qobject_cast<QToolButton*>(
				DrawerToolbar->widgetForAction(entry.Toggle));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Collapse);
			button->setFocusPolicy(Qt::StrongFocus);
		}
	}

	void workbench_window::apply_focus_order()
	{
		const auto order = focus_order();
		if (order.isEmpty()) {
			return;
		}
		// A toolbar mode change destroys and recreates the primary buttons, and a
		// newly created widget joins the focus chain wherever Qt puts it. The
		// compact overflow is built once and never moved, so anchoring the chain
		// to it makes the declared order survive every mode change instead of
		// drifting to wherever the last rebuild landed.
		QWidget::setTabOrder(OverflowButton, order.first());
		for (auto index = qsizetype{ 1 }; index < order.size(); ++index) {
			auto* const previous = order[index - 1];
			auto* const next     = order[index];
			// setTabOrder moves its second argument. A stop inside a composed
			// region is left exactly where RFC-BPR-0003 and RFC-BPR-0004 put it,
			// so the shell orders its own chrome around those regions rather than
			// replacing their internal order with one it decided.
			if (EpochSelectors->isAncestorOf(next)
				|| ComparisonSurface->isAncestorOf(next)) {
				continue;
			}
			QWidget::setTabOrder(previous, next);
		}
		++Statistics.FocusOrderUpdates;
	}

	void workbench_window::update_action_state()
	{
		// One projection for all four primary actions. Entry `i` of the core's
		// delta decides action `i`; the window adds no legality of its own and
		// keeps no second carrier for the change actions.
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			const auto projection
				= command(static_cast<core::workbench_command_id>(index));
			const auto reason = projection
				? projection->DisabledReason
				: core::workbench_reason_code::no_active_workbench;
			describe_action(Navigation[index].Action,
							projection && projection->Enabled,
							Navigation[index].Purpose,
							index < file_navigation_entries
								? reason_text(reason)
								: change_reason_text(reason));
		}

		update_drawer_action(0U,
							 core::workbench_command_id::toggle_files_drawer,
							 DrawerStates.Files);
		update_drawer_action(
			1U,
			core::workbench_command_id::toggle_conversations_drawer,
			DrawerStates.Conversations);
	}

	void workbench_window::update_drawer_action(
		const std::size_t                  aIndex,
		const core::workbench_command_id   aId,
		const core::workbench_drawer_state aState)
	{
		const auto& entry      = Drawers[aIndex];
		const auto  projection = command(aId);
		const auto  expanded   = aState == core::workbench_drawer_state::expanded;
		entry.Toggle->setChecked(expanded);
		describe_action(
			entry.Toggle,
			projection && projection->Enabled,
			expanded ? entry.Collapse : entry.Expand,
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
	}

	void workbench_window::update_status_text()
	{
		auto status = QString{ };
		if (!HasCommands) {
			status = QStringLiteral("Open a pull request to begin reviewing.");
		} else if (!Cursor) {
			status = QStringLiteral("No pull request is open in the workbench.");
		} else if (Cursor->ActionableCount == 0U) {
			status = QStringLiteral("This pull request has no actionable files.");
		} else if (Cursor->Ordinal == 0U) {
			status = QStringLiteral(
						 "The selected file is outside the %1 actionable file(s).")
						 .arg(Cursor->ActionableCount);
		} else {
			status = QStringLiteral("Actionable file %1 of %2.")
						 .arg(Cursor->Ordinal)
						 .arg(Cursor->ActionableCount);
		}
		if (Failure) {
			status.append(QStringLiteral(" %1").arg(failure_text(*Failure)));
		}
		if (HasEpochState) {
			// Quoted, never restated. Review-history loading, failure,
			// cancellation, and empty history are decided and worded by the
			// composed control; the shell repeats that exact sentence so its one
			// status line states the whole workbench state in text rather than in
			// enabled state or colour alone.
			const auto epochs = EpochSelectors->status_text();
			if (!epochs.isEmpty()) {
				status.append(QStringLiteral(" %1").arg(epochs));
			}
		}
		StatusLabel->setText(status);
	}

	void workbench_window::announce_update()
	{
		const auto announcement = StatusLabel->text();
		if (announcement.isEmpty() || announcement == LastAnnouncement) {
			return;
		}
		LastAnnouncement = announcement;
		++Statistics.Announcements;
		QAccessibleAnnouncementEvent event{ this, announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto workbench_window::command(const core::workbench_command_id aId) const
		-> std::optional<core::workbench_command_projection>
	{
		if (!HasCommands) {
			return std::nullopt;
		}
		const auto index = static_cast<std::size_t>(aId);
		if (index >= Commands.Commands.size()
			|| Commands.Commands[index].Id != aId) {
			return std::nullopt;
		}
		return Commands.Commands[index];
	}
}
