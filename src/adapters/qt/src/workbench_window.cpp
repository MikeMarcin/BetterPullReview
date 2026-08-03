#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstddef>
#include <utility>

namespace bpr::qt {
	namespace {
		// The narrowest code column the central review surface is worth keeping.
		// Primary navigation moves into the compact overflow rather than eating
		// into it, which is what "narrow width" means for this window.
		constexpr int reserved_central_columns = 80;

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
			}
			return QStringLiteral("File navigation is unavailable.");
		}

		[[nodiscard]] auto change_reason_text(
			const core::change_navigation_outcome aOutcome) -> QString
		{
			switch (aOutcome) {
			case core::change_navigation_outcome::selected:
			case core::change_navigation_outcome::wrapped:
				return { };
			case core::change_navigation_outcome::at_start:
				return QStringLiteral("This is the first change in this comparison.");
			case core::change_navigation_outcome::at_end:
				return QStringLiteral("This is the last change in this comparison.");
			case core::change_navigation_outcome::no_changes:
				return QStringLiteral("This comparison has no changes to navigate.");
			case core::change_navigation_outcome::invalid_selection:
				return QStringLiteral("The selected change is no longer available.");
			}
			return QStringLiteral("Change navigation is unavailable.");
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

		[[nodiscard]] auto placeholder(
			QWidget* const aParent,
			const QString& aObjectName,
			const QString& aAccessibleName,
			const QString& aAccessibleDescription) -> QWidget*
		{
			auto* const surface = new QWidget{ aParent };
			surface->setObjectName(aObjectName);
			surface->setAccessibleName(aAccessibleName);
			surface->setAccessibleDescription(aAccessibleDescription);
			return surface;
		}
	}

	workbench_window::workbench_window(
		workbench_intent_dispatch aDispatch,
		workbench_change_dispatch aChangeDispatch,
		QWidget* const            aParent)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, ChangeDispatch{ std::move(aChangeDispatch) }
	{
		setObjectName(QStringLiteral("workbenchWindow"));
		setWindowTitle(QStringLiteral("BetterPullReview"));
		// Deliberately narrower than compact_width_threshold(). A supported width
		// below the natural navigation width is exactly what the explicit compact
		// overflow exists for.
		setMinimumSize(480, 360);
		resize(1'280, 820);

		build_primary_toolbar();
		build_central_placeholders();

		StatusLabel = new QLabel{ this };
		StatusLabel->setObjectName(QStringLiteral("workbenchStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("Workbench status"));
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
			return { .Action = action, .Name = aName, .Purpose = aPurpose };
		};

		Navigation[0] = make(QStringLiteral("&Previous file"),
							 QStringLiteral("Previous file"),
							 QStringLiteral("previousWorkbenchFileAction"),
							 QStringLiteral("Select the previous actionable file."),
							 QKeySequence{ QStringLiteral("Alt+Up") });
		Navigation[1] = make(QStringLiteral("&Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("nextWorkbenchFileAction"),
							 QStringLiteral("Select the next actionable file."),
							 QKeySequence{ QStringLiteral("Alt+Down") });
		Navigation[2] = make(QStringLiteral("Pre&vious change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("previousWorkbenchChangeAction"),
							 QStringLiteral("Select the previous semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgUp") });
		Navigation[3] = make(QStringLiteral("Ne&xt change"),
							 QStringLiteral("Next change"),
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
		OverflowMenu->setAccessibleName(QStringLiteral("Review navigation"));
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

	void workbench_window::build_central_placeholders()
	{
		auto* const central = new QWidget{ this };
		central->setObjectName(QStringLiteral("workbenchCentralSurface"));
		central->setAccessibleName(QStringLiteral("Review workbench"));

		FilesDrawer = placeholder(
			central,
			QStringLiteral("workbenchFilesDrawerPlaceholder"),
			QStringLiteral("Files drawer"),
			QStringLiteral("Reserved for the collapsible file drawer"));
		ConversationsDrawer = placeholder(
			central,
			QStringLiteral("workbenchConversationsDrawerPlaceholder"),
			QStringLiteral("Conversations drawer"),
			QStringLiteral("Reserved for the collapsible conversation drawer"));
		EpochSelectors = placeholder(
			central,
			QStringLiteral("workbenchEpochSelectorPlaceholder"),
			QStringLiteral("Epoch selectors"),
			QStringLiteral("Reserved for the left and right epoch selectors"));
		ComparisonSurface = placeholder(
			central,
			QStringLiteral("workbenchComparisonSurfacePlaceholder"),
			QStringLiteral("Comparison surface"),
			QStringLiteral("Reserved for the composed side-by-side comparison"));
		DetailPane = placeholder(
			central,
			QStringLiteral("workbenchDetailPanePlaceholder"),
			QStringLiteral("Selected change detail"),
			QStringLiteral("Reserved for the full-width line-over-line detail pane"));

		auto* const centre = new QVBoxLayout;
		centre->setContentsMargins(0, 0, 0, 0);
		centre->addWidget(EpochSelectors);
		centre->addWidget(ComparisonSurface, 1);
		centre->addWidget(DetailPane);

		auto* const row = new QHBoxLayout{ central };
		row->setContentsMargins(0, 0, 0, 0);
		row->addWidget(FilesDrawer);
		row->addLayout(centre, 1);
		row->addWidget(ConversationsDrawer);
		setCentralWidget(central);
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
		Failure.reset();
		++Statistics.Publications;
		update_action_state();
		update_status_text();
		announce_update();
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

	void workbench_window::publish_change_navigation(
		const workbench_change_navigation_delta& aDelta)
	{
		ChangeNavigation = aDelta;
		update_action_state();
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

	auto workbench_window::files_drawer_placeholder() const noexcept -> QWidget*
	{
		return FilesDrawer;
	}

	auto workbench_window::conversations_drawer_placeholder() const noexcept
		-> QWidget*
	{
		return ConversationsDrawer;
	}

	auto workbench_window::epoch_selector_placeholder() const noexcept -> QWidget*
	{
		return EpochSelectors;
	}

	auto workbench_window::comparison_surface_placeholder() const noexcept
		-> QWidget*
	{
		return ComparisonSurface;
	}

	auto workbench_window::detail_pane_placeholder() const noexcept -> QWidget*
	{
		return DetailPane;
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
		switch (aIndex) {
		case 0U:
		case 1U:
			if (Dispatch) {
				Dispatch(core::navigate_workbench_file_intent{
					.Direction = aIndex == 0U
						? core::change_navigation_direction::previous
						: core::change_navigation_direction::next,
				});
			}
			return;
		default:
			break;
		}
		if (!ChangeDispatch) {
			return;
		}
		ChangeDispatch(core::navigate_diff_change_intent{
			.Direction = aIndex == 2U
				? core::change_navigation_direction::previous
				: core::change_navigation_direction::next,
			.Policy    = ChangeNavigation ? ChangeNavigation->Policy
										  : core::change_navigation_policy::stop,
		});
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
				PrimaryToolbar->insertAction(
					index < 2U ? SeparatorAction : OverflowAction, action);
			}
		}
		SeparatorAction->setVisible(!compact);
		OverflowAction->setVisible(compact);
		configure_tool_buttons();
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
			button->setAccessibleName(entry.Name);
			button->setAccessibleDescription(entry.Purpose);
			button->setFocusPolicy(Qt::StrongFocus);
		}
	}

	void workbench_window::update_action_state()
	{
		const auto previousFile = command(core::workbench_command_id::previous_file);
		const auto nextFile     = command(core::workbench_command_id::next_file);
		describe_action(
			Navigation[0].Action,
			previousFile && previousFile->Enabled,
			Navigation[0].Purpose,
			reason_text(previousFile
							? previousFile->DisabledReason
							: core::workbench_reason_code::no_active_workbench));
		describe_action(
			Navigation[1].Action,
			nextFile && nextFile->Enabled,
			Navigation[1].Purpose,
			reason_text(nextFile
							? nextFile->DisabledReason
							: core::workbench_reason_code::no_active_workbench));

		const auto absent = QStringLiteral("No file comparison is open.");
		describe_action(Navigation[2].Action,
						ChangeNavigation && ChangeNavigation->Previous.Enabled,
						Navigation[2].Purpose,
						ChangeNavigation
							? change_reason_text(ChangeNavigation->Previous.Outcome)
							: absent);
		describe_action(Navigation[3].Action,
						ChangeNavigation && ChangeNavigation->Next.Enabled,
						Navigation[3].Purpose,
						ChangeNavigation
							? change_reason_text(ChangeNavigation->Next.Outcome)
							: absent);
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
