#include "bpr/qt/epoch_comparison_control.hpp"

#include "bpr/qt/menu_tool_button.hpp"

#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <variant>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto oid_text(const core::git_commit_oid& aOid) -> QString
		{
			const auto canonical = core::canonical_hex(aOid);
			const auto view      = canonical.view();
			return QString::fromLatin1(
				view.data(), static_cast<qsizetype>(view.size()));
		}

		[[nodiscard]] auto reason_text(const core::epoch_reason_code aReason) -> QString
		{
			switch (aReason) {
			case core::epoch_reason_code::none:
				return { };
			case core::epoch_reason_code::invalid_identity:
				return QStringLiteral("The review-history identity is invalid.");
			case core::epoch_reason_code::invalid_oid:
				return QStringLiteral("A commit identity is invalid.");
			case core::epoch_reason_code::invalid_revision:
				return QStringLiteral("The pull-request revision is invalid.");
			case core::epoch_reason_code::invalid_catalog:
				return QStringLiteral("The review-history catalog is invalid.");
			case core::epoch_reason_code::invalid_history_cursor:
				return QStringLiteral("The history-page position is invalid.");
			case core::epoch_reason_code::invalid_maintenance_target:
				return QStringLiteral("The maintenance target is invalid.");
			case core::epoch_reason_code::invalid_observation:
				return QStringLiteral("The review-history observation is invalid.");
			case core::epoch_reason_code::invalid_selection:
				return QStringLiteral("That comparison cannot be selected.");
			case core::epoch_reason_code::no_active_authority:
				return QStringLiteral("Open an authenticated pull request first.");
			case core::epoch_reason_code::no_catalog:
				return QStringLiteral("Review history is not available yet.");
			case core::epoch_reason_code::no_resolved_comparison:
				return QStringLiteral("No exact comparison is resolved.");
			case core::epoch_reason_code::catalog_revision_mismatch:
				return QStringLiteral("Review history changed; retry the selection.");
			case core::epoch_reason_code::stale_completion:
				return QStringLiteral("An obsolete review-history result was ignored.");
			case core::epoch_reason_code::duplicate_history_page:
				return QStringLiteral("That history page is already loading.");
			case core::epoch_reason_code::duplicate_maintenance:
				return QStringLiteral("Review-history maintenance is already running.");
			case core::epoch_reason_code::completion_identity_mismatch:
				return QStringLiteral("A mismatched review-history result was ignored.");
			case core::epoch_reason_code::request_counter_exhausted:
			case core::epoch_reason_code::epoch_generation_exhausted:
			case core::epoch_reason_code::comparison_generation_exhausted:
			case core::epoch_reason_code::epoch_ordinal_exhausted:
			case core::epoch_reason_code::catalog_revision_exhausted:
			case core::epoch_reason_code::comparison_revision_exhausted:
				return QStringLiteral("A review-history identity limit was reached.");
			case core::epoch_reason_code::git_tool_unsupported:
				return QStringLiteral("Configure Git 2.49 or later with exact revision cloning, then retry.");
			case core::epoch_reason_code::git_tool_probe_failed:
				return QStringLiteral("The configured Git could not be checked. Check its executable and retry.");
			case core::epoch_reason_code::gh_tool_unsupported:
				return QStringLiteral(
					"Configure GitHub CLI 2.96 or later with its api, repo clone and git credential commands, then retry.");
			case core::epoch_reason_code::gh_tool_probe_failed:
				return QStringLiteral("The configured GitHub CLI could not be checked. Check its executable and retry.");
			case core::epoch_reason_code::unavailable:
				return QStringLiteral("The exact comparison is unavailable.");
			case core::epoch_reason_code::catalog_conflict:
				return QStringLiteral("Review history changed concurrently.");
			case core::epoch_reason_code::quota_exceeded:
				return QStringLiteral("The local review-history limit was reached.");
			case core::epoch_reason_code::repair_required:
				return QStringLiteral("The local review history requires repair.");
			case core::epoch_reason_code::process_failure:
				// Deliberately says which layer, and does not try to say what
				// Git said. The text Git wrote is retained in the adapter that
				// ran it (BPR-0348); putting a subprocess message in front of a
				// reviewer here would be quoting a stranger.
				return QStringLiteral("A Git command failed while reading the review history.");
			case core::epoch_reason_code::unsupported_object:
				// Says what is wrong rather than that something failed, and
				// still does not quote Git: the message class it named is in
				// the adapter's log, not in front of the reviewer (BPR-0880).
				return QStringLiteral(
					"This pull request contains a Git object the installed Git refuses to read.");
			case core::epoch_reason_code::cancelled:
				return QStringLiteral("Review-history work was canceled.");
			case core::epoch_reason_code::cancellation_after_point_of_no_return:
				return QStringLiteral(
					"Maintenance is finishing safely and cannot be canceled.");
			}
			return QStringLiteral("Review history is unavailable.");
		}

		[[nodiscard]] auto epoch_status_text(const core::epoch_status aStatus) -> QString
		{
			switch (aStatus) {
			case core::epoch_status::inactive:
				return QStringLiteral("Review history is inactive.");
			case core::epoch_status::loading_catalog:
				return QStringLiteral("Loading review history…");
			case core::epoch_status::preparing:
				return QStringLiteral("Refreshing retained revisions…");
			case core::epoch_status::committing:
				return QStringLiteral("Saving the observed revision…");
			case core::epoch_status::resolving:
				return QStringLiteral("Resolving the exact comparison…");
			case core::epoch_status::maintaining:
				return QStringLiteral("Updating retained review history…");
			case core::epoch_status::ready:
				return QStringLiteral("Review history is ready.");
			case core::epoch_status::unavailable:
				return QStringLiteral("Review history is unavailable.");
			case core::epoch_status::failed:
				return QStringLiteral("Review history could not be updated.");
			case core::epoch_status::cancelled:
				return QStringLiteral("Review-history refresh was canceled.");
			}
			return QStringLiteral("Review history is inactive.");
		}

		[[nodiscard]] auto alias_text(const core::named_comparison aAlias) -> QString
		{
			switch (aAlias) {
			case core::named_comparison::whole_pr:
				return QStringLiteral("Whole PR");
			case core::named_comparison::previous_upload_to_latest:
				return QStringLiteral("Previous upload → latest");
			case core::named_comparison::my_last_reviewed_to_latest:
				return QStringLiteral("My last reviewed → latest");
			}
			return QStringLiteral("Comparison");
		}

		[[nodiscard]] auto availability_text(
			const core::anchor_availability aAvailability) -> QString
		{
			switch (aAvailability) {
			case core::anchor_availability::available:
				return QStringLiteral("available");
			case core::anchor_availability::unavailable:
				return QStringLiteral("unavailable");
			case core::anchor_availability::pruned:
				return QStringLiteral("pruned");
			case core::anchor_availability::cache_deleted:
				return QStringLiteral("cache deleted");
			case core::anchor_availability::corrupt:
				return QStringLiteral("corrupt");
			case core::anchor_availability::never_observed:
				return QStringLiteral("never observed");
			}
			return QStringLiteral("unavailable");
		}

		void configure_action(
			QAction* const      aAction,
			const QString&      aObjectName,
			const QKeySequence& aShortcut)
		{
			aAction->setObjectName(aObjectName);
			aAction->setShortcut(aShortcut);
			aAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		}

		void configure_oid_editor(
			QLineEdit* const aEditor,
			const QString&   aObjectName,
			const QString&   aAccessibleName)
		{
			aEditor->setObjectName(aObjectName);
			aEditor->setAccessibleName(aAccessibleName);
			aEditor->setAccessibleDescription(
				QStringLiteral("Complete lowercase commit OID; read-only and copyable"));
			aEditor->setReadOnly(true);
			aEditor->setClearButtonEnabled(false);
			aEditor->setPlaceholderText(QStringLiteral("No exact commit selected"));
		}

		[[nodiscard]] auto confirm_destructive_action(QWidget* const aParent,
													  const QString& aObjectName,
													  const QString& aTitle,
													  const QString& aText,
													  const QString& aConfirmationText) -> bool
		{
			auto* owner = aParent;
			if (auto* active = QApplication::activeWindow(); active != nullptr
				&& (active == aParent->window() || aParent->window()->isAncestorOf(active))) {
				owner = active;
			}
			QMessageBox dialog{ QMessageBox::Warning,
								aTitle,
								aText,
								QMessageBox::Cancel,
								owner };
			dialog.setObjectName(aObjectName);
			dialog.setTextFormat(Qt::PlainText);
			dialog.setAccessibleName(aTitle);
			dialog.setAccessibleDescription(aText);
			dialog.setWindowModality(Qt::WindowModal);
			auto* const confirm = dialog.addButton(
				aConfirmationText, QMessageBox::DestructiveRole);
			confirm->setObjectName(aObjectName + QStringLiteral("ConfirmButton"));
			confirm->setAccessibleName(aConfirmationText);
			auto* const cancel = qobject_cast<QPushButton*>(dialog.button(QMessageBox::Cancel));
			cancel->setObjectName(aObjectName + QStringLiteral("CancelButton"));
			dialog.setDefaultButton(cancel);
			dialog.setEscapeButton(cancel);
			static_cast<void>(dialog.exec());
			return dialog.clickedButton() == confirm;
		}
	}

	epoch_comparison_control::epoch_comparison_control(
		application::epoch_intent_dispatch aDispatch,
		QWidget* const                     aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setObjectName(QStringLiteral("epochComparisonControl"));
		setAccessibleName(QStringLiteral("Review epoch comparison"));
		setAccessibleDescription(QStringLiteral(
			"Choose independent exact left and right retained revision anchors"));

		HistoryModel = new epoch_history_model{ this };

		WholePrAction = new QAction{ QStringLiteral("&Whole PR"), this };
		configure_action(WholePrAction,
						 QStringLiteral("wholePrComparisonAction"),
						 QKeySequence{ QStringLiteral("Alt+1") });
		WholePrAction->setCheckable(true);
		PreviousUploadAction = new QAction{ QStringLiteral("Previous &upload → latest"), this };
		configure_action(PreviousUploadAction,
						 QStringLiteral("previousUploadComparisonAction"),
						 QKeySequence{ QStringLiteral("Alt+2") });
		PreviousUploadAction->setCheckable(true);
		LastReviewedAction = new QAction{ QStringLiteral("My last &reviewed → latest"), this };
		configure_action(LastReviewedAction,
						 QStringLiteral("lastReviewedComparisonAction"),
						 QKeySequence{ QStringLiteral("Alt+3") });
		LastReviewedAction->setCheckable(true);
		auto* const aliases = new QActionGroup{ this };
		aliases->setExclusive(true);
		aliases->addAction(WholePrAction);
		aliases->addAction(PreviousUploadAction);
		aliases->addAction(LastReviewedAction);
		addActions(aliases->actions());

		LoadHistoryAction = new QAction{ QStringLiteral("Load &earlier epochs"), this };
		configure_action(LoadHistoryAction,
						 QStringLiteral("loadEpochHistoryAction"),
						 QKeySequence{ QStringLiteral("Ctrl+Alt+Down") });
		ReturnNewestAction = new QAction{ QStringLiteral("Return to &newest epochs"), this };
		configure_action(ReturnNewestAction,
						 QStringLiteral("returnToNewestEpochsAction"),
						 QKeySequence{ QStringLiteral("Ctrl+Alt+Up") });
		RefreshAction = new QAction{ QStringLiteral("Refresh review &history"), this };
		configure_action(RefreshAction,
						 QStringLiteral("refreshEpochsAction"),
						 QKeySequence{ QStringLiteral("Ctrl+Alt+R") });
		CancelAction = new QAction{ QStringLiteral("&Cancel history refresh"), this };
		configure_action(CancelAction,
						 QStringLiteral("cancelEpochRefreshAction"),
						 QKeySequence{ QStringLiteral("Escape") });
		PruneAction = new QAction{ QStringLiteral("&Prune selected epochs"), this };
		configure_action(PruneAction,
						 QStringLiteral("pruneSelectedEpochsAction"),
						 QKeySequence{ QStringLiteral("Ctrl+Alt+P") });
		DeleteCacheAction = new QAction{ QStringLiteral("Delete repository &cache"), this };
		configure_action(DeleteCacheAction,
						 QStringLiteral("deleteRepositoryCacheAction"),
						 QKeySequence{ QStringLiteral("Ctrl+Alt+Shift+D") });
		CancelMaintenanceAction = new QAction{ QStringLiteral("Cancel history &maintenance"), this };
		configure_action(CancelMaintenanceAction,
						 QStringLiteral("cancelEpochMaintenanceAction"),
						 QKeySequence{ QStringLiteral("Ctrl+Alt+Escape") });
		addAction(LoadHistoryAction);
		addAction(ReturnNewestAction);
		addAction(RefreshAction);
		addAction(CancelAction);
		addAction(PruneAction);
		addAction(DeleteCacheAction);
		addAction(CancelMaintenanceAction);

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		// [[BPR-0646]]. RFC-BPR-0028's epoch line is the one row this control
		// always shows: "Updates", the line, and a menu that carries the
		// named comparisons and the history actions. Everything the control
		// showed before -- the exact anchors, their OIDs, the status and the
		// maintenance -- is the details region, shown on request.
		auto* const lineRow = new QHBoxLayout;
		lineRow->setContentsMargins(12, 0, 8, 0);
		lineRow->setSpacing(10);
		auto* const lineName = new QLabel{ QStringLiteral("Updates"), this };
		lineName->setObjectName(QStringLiteral("epochLineName"));
		lineName->setFixedWidth(52);
		LineName = lineName;
		Line     = new epoch_line{ this };
		Line->set_dispatch(
			[this](const core::comparison_side             aSide,
				   const core::comparison_anchor_identity& aIdentity) {
				dispatch_identity(aSide, aIdentity);
			});
		Line->set_occurrence_dispatch([this](core::comparison_side aSide, core::epoch_occurrence_id aOccurrence) {
			const auto catalog = HistoryModel->catalog();
			if (!ApplyingProjection && Dispatch && catalog) {
				Dispatch(core::select_epoch_occurrence_intent{ aSide, aOccurrence, *catalog });
			}
		});
		auto* const comparingMenu = new QMenu{ this };
		comparingMenu->setObjectName(QStringLiteral("epochComparingMenu"));
		comparingMenu->addAction(WholePrAction);
		comparingMenu->addAction(PreviousUploadAction);
		comparingMenu->addAction(LastReviewedAction);
		comparingMenu->addSeparator();
		comparingMenu->addAction(LoadHistoryAction);
		comparingMenu->addAction(ReturnNewestAction);
		comparingMenu->addAction(RefreshAction);
		comparingMenu->addAction(CancelAction);
		comparingMenu->addSeparator();
		comparingMenu->addAction(PruneAction);
		comparingMenu->addAction(DeleteCacheAction);
		comparingMenu->addAction(CancelMaintenanceAction);
		auto* const comparing = new menu_tool_button{ this };
		comparing->setObjectName(QStringLiteral("epochComparingButton"));
		comparing->setAccessibleName(QStringLiteral("Comparing"));
		comparing->setAccessibleDescription(QStringLiteral(
			"The named comparison in force; opens the named comparisons and "
			"the review-history actions"));
		comparing->setMenu(comparingMenu);
		comparing->setPopupMode(QToolButton::InstantPopup);
		comparing->setToolButtonStyle(Qt::ToolButtonTextOnly);
		comparing->setFocusPolicy(Qt::StrongFocus);
		comparing->setText(QStringLiteral("Comparing whole PR"));
		ComparingButton = comparing;
		// [[BPR-0676]]. The nodes say which uploads exist; they do not say
		// whether the thing is working. The owner: "The epoch line is quite
		// hard to see if it's working properly or not." This says it in
		// words, on the line, from the same facts the details region states
		// at length behind a disclosure nobody had opened.
		LineSummary = new QLabel{ this };
		LineSummary->setObjectName(QStringLiteral("epochLineSummary"));
		LineSummary->setAccessibleName(QStringLiteral("What is being compared"));
		LineSummary->setTextFormat(Qt::PlainText);
		LineSummary->setForegroundRole(QPalette::PlaceholderText);
		LineSummary->setTextInteractionFlags(Qt::TextSelectableByKeyboard
											 | Qt::TextSelectableByMouse);
		lineRow->addWidget(lineName);
		lineRow->addWidget(Line, 1);
		ReadinessLabel = new QLabel{ this };
		ReadinessLabel->setObjectName(QStringLiteral("epochReadiness"));
		ReadinessLabel->setAccessibleName(QStringLiteral("Review loading and readiness"));
		ReadinessLabel->setTextFormat(Qt::PlainText);
		ReadinessLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		ReadinessLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		ReadinessLabel->setFixedHeight(epoch_line::height_pixels);
		ReadinessLabel->hide();
		lineRow->addWidget(ReadinessLabel, 1);
		lineRow->addWidget(LineSummary);
		lineRow->addWidget(ComparingButton);
		layout->addLayout(lineRow);
		// [[BPR-1130]]. RFC-BPR-0052: open, the timeline runs alone across the
		// window. The three words stay the control's fact, and their sentence
		// is the comparison's tooltip; loading and an unreadable pair are the
		// readiness line's to say ([[BPR-0795]]).
		LineSummary->hide();

		Details = new QWidget{ this };
		Details->setObjectName(QStringLiteral("epochDetails"));
		Details->setAccessibleName(QStringLiteral("Review epoch details"));
		Details->setAccessibleDescription(QStringLiteral(
			"Exact left and right anchors, their commit OIDs, the review-history "
			"status and its maintenance"));
		auto* const detailsLayout = new QVBoxLayout{ Details };
		detailsLayout->setContentsMargins(0, 0, 0, 0);
		detailsLayout->setSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
		layout->addWidget(Details);
		Details->hide();

		auto* const aliasRow = new QHBoxLayout;
		aliasRow->setContentsMargins(0, 0, 0, 0);
		const auto makeButton = [this, aliasRow](QAction* const aAction) {
			auto* const button = new QToolButton{ this };
			button->setDefaultAction(aAction);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			aliasRow->addWidget(button);
		};
		makeButton(WholePrAction);
		makeButton(PreviousUploadAction);
		makeButton(LastReviewedAction);
		aliasRow->addStretch(1);
		detailsLayout->addLayout(aliasRow);

		auto* const selectorGrid = new QGridLayout;
		selectorGrid->setContentsMargins(0, 0, 0, 0);
		selectorGrid->setColumnStretch(1, 1);
		selectorGrid->setColumnStretch(3, 1);
		LeftSelector = new QComboBox{ this };
		LeftSelector->setObjectName(QStringLiteral("leftEpochSelector"));
		LeftSelector->setAccessibleName(QStringLiteral("Left review epoch"));
		LeftSelector->setAccessibleDescription(QStringLiteral(
			"Select the exact base or head anchor for the left side"));
		LeftSelector->setModel(HistoryModel);
		LeftSelector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		LeftSelector->setMinimumContentsLength(24);
		RightSelector = new QComboBox{ this };
		RightSelector->setObjectName(QStringLiteral("rightEpochSelector"));
		RightSelector->setAccessibleName(QStringLiteral("Right review epoch"));
		RightSelector->setAccessibleDescription(QStringLiteral(
			"Select the exact base or head anchor for the right side"));
		RightSelector->setModel(HistoryModel);
		RightSelector->setSizeAdjustPolicy(
			QComboBox::AdjustToMinimumContentsLengthWithIcon);
		RightSelector->setMinimumContentsLength(24);
		auto* const leftLabel = new QLabel{ QStringLiteral("&Left:"), this };
		leftLabel->setObjectName(QStringLiteral("leftEpochSelectorLabel"));
		leftLabel->setBuddy(LeftSelector);
		auto* const rightLabel = new QLabel{ QStringLiteral("&Right:"), this };
		rightLabel->setObjectName(QStringLiteral("rightEpochSelectorLabel"));
		rightLabel->setBuddy(RightSelector);
		selectorGrid->addWidget(leftLabel, 0, 0);
		selectorGrid->addWidget(LeftSelector, 0, 1);
		selectorGrid->addWidget(rightLabel, 0, 2);
		selectorGrid->addWidget(RightSelector, 0, 3);

		LeftOid = new QLineEdit{ this };
		configure_oid_editor(LeftOid,
							 QStringLiteral("leftEpochOid"),
							 QStringLiteral("Exact left commit OID"));
		RightOid = new QLineEdit{ this };
		configure_oid_editor(RightOid,
							 QStringLiteral("rightEpochOid"),
							 QStringLiteral("Exact right commit OID"));
		auto* const leftOidLabel = new QLabel{ QStringLiteral("Left OID:"), this };
		leftOidLabel->setBuddy(LeftOid);
		auto* const rightOidLabel = new QLabel{ QStringLiteral("Right OID:"), this };
		rightOidLabel->setBuddy(RightOid);
		selectorGrid->addWidget(leftOidLabel, 1, 0);
		selectorGrid->addWidget(LeftOid, 1, 1);
		selectorGrid->addWidget(rightOidLabel, 1, 2);
		selectorGrid->addWidget(RightOid, 1, 3);
		detailsLayout->addLayout(selectorGrid);

		auto* const footer = new QHBoxLayout;
		footer->setContentsMargins(0, 0, 0, 0);
		auto* const statusColumn = new QVBoxLayout;
		StatusLabel              = new QLabel{ this };
		StatusLabel->setObjectName(QStringLiteral("epochStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("Review-history status"));
		StatusLabel->setWordWrap(true);
		StatusLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard
											 | Qt::TextSelectableByMouse);
		ComparisonLabel = new QLabel{ this };
		ComparisonLabel->setObjectName(QStringLiteral("epochComparisonStatus"));
		ComparisonLabel->setAccessibleName(QStringLiteral("Comparison availability"));
		ComparisonLabel->setWordWrap(true);
		ComparisonLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard
												 | Qt::TextSelectableByMouse);
		statusColumn->addWidget(StatusLabel);
		statusColumn->addWidget(ComparisonLabel);
		footer->addLayout(statusColumn, 1);
		LoadHistoryButton = new QToolButton{ this };
		LoadHistoryButton->setObjectName(QStringLiteral("loadEpochHistoryButton"));
		LoadHistoryButton->setDefaultAction(LoadHistoryAction);
		LoadHistoryButton->setFocusPolicy(Qt::StrongFocus);
		ReturnNewestButton = new QToolButton{ this };
		ReturnNewestButton->setObjectName(
			QStringLiteral("returnToNewestEpochsButton"));
		ReturnNewestButton->setDefaultAction(ReturnNewestAction);
		ReturnNewestButton->setFocusPolicy(Qt::StrongFocus);
		RefreshButton = new QToolButton{ this };
		RefreshButton->setObjectName(QStringLiteral("refreshEpochsButton"));
		RefreshButton->setDefaultAction(RefreshAction);
		RefreshButton->setFocusPolicy(Qt::StrongFocus);
		CancelButton = new QToolButton{ this };
		CancelButton->setObjectName(QStringLiteral("cancelEpochRefreshButton"));
		CancelButton->setDefaultAction(CancelAction);
		CancelButton->setFocusPolicy(Qt::StrongFocus);
		auto* const maintenanceMenu = new QMenu{ this };
		maintenanceMenu->setObjectName(
			QStringLiteral("localHistoryMaintenanceMenu"));
		maintenanceMenu->addAction(PruneAction);
		maintenanceMenu->addAction(DeleteCacheAction);
		maintenanceMenu->addSeparator();
		maintenanceMenu->addAction(CancelMaintenanceAction);
		MaintenanceButton = new menu_tool_button{ this };
		MaintenanceButton->setObjectName(
			QStringLiteral("localHistoryMaintenanceButton"));
		MaintenanceButton->setText(QStringLiteral("Local history maintenance"));
		MaintenanceButton->setAccessibleName(
			QStringLiteral("Local history maintenance"));
		MaintenanceButton->setAccessibleDescription(QStringLiteral(
			"Open confirmed local retention and repository-cache operations"));
		MaintenanceButton->setMenu(maintenanceMenu);
		MaintenanceButton->setPopupMode(QToolButton::InstantPopup);
		MaintenanceButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		MaintenanceButton->setFocusPolicy(Qt::StrongFocus);
		footer->addWidget(LoadHistoryButton);
		footer->addWidget(ReturnNewestButton);
		footer->addWidget(RefreshButton);
		footer->addWidget(CancelButton);
		footer->addWidget(MaintenanceButton);
		detailsLayout->addLayout(footer);

		connect(WholePrAction, &QAction::triggered, this, [this] {
			dispatch_alias(core::named_comparison::whole_pr);
		});
		connect(PreviousUploadAction, &QAction::triggered, this, [this] {
			dispatch_alias(core::named_comparison::previous_upload_to_latest);
		});
		connect(LastReviewedAction, &QAction::triggered, this, [this] {
			dispatch_alias(core::named_comparison::my_last_reviewed_to_latest);
		});
		connect(LeftSelector,
				&QComboBox::currentIndexChanged,
				this,
				[this](const int aRow) {
					dispatch_anchor(core::comparison_side::left, aRow);
				});
		connect(RightSelector,
				&QComboBox::currentIndexChanged,
				this,
				[this](const int aRow) {
					dispatch_anchor(core::comparison_side::right, aRow);
				});
		connect(LoadHistoryAction, &QAction::triggered, this, [this] {
			dispatch_older_history_page();
		});
		connect(ReturnNewestAction, &QAction::triggered, this, [this] {
			dispatch_newest_history_page();
		});
		connect(RefreshAction, &QAction::triggered, this, [this] {
			if (Dispatch) {
				Dispatch(core::refresh_epochs_intent{ });
			}
		});
		connect(CancelAction, &QAction::triggered, this, [this] {
			if (Dispatch) {
				Dispatch(core::cancel_epoch_refresh_intent{ });
			}
		});
		connect(PruneAction, &QAction::triggered, this, [this] {
			dispatch_prune();
		});
		connect(DeleteCacheAction, &QAction::triggered, this, [this] {
			dispatch_delete_cache();
		});
		connect(CancelMaintenanceAction, &QAction::triggered, this, [this] {
			if (Dispatch) {
				Dispatch(core::cancel_epoch_maintenance_intent{ });
			}
		});

		WholePrAction->setChecked(true);
		update_controls();
		refresh_native_appearance();
	}

	void epoch_comparison_control::publish(
		const application::epoch_presentation_update& aUpdate)
	{
		ApplyingProjection = true;
		if (aUpdate.Catalog) {
			apply_catalog(*aUpdate.Catalog);
		}
		if (aUpdate.Transition) {
			apply_transition(*aUpdate.Transition);
		}
		update_controls();
		ApplyingProjection = false;
		++Statistics.PublishedUpdates;
		announce_update();
	}

	void epoch_comparison_control::set_maintenance_scope(core::pull_request_authority aAuthority, QString aLabel)
	{
		ScopeAuthority = aAuthority;
		ScopeLabel     = std::move(aLabel);
	}

	auto epoch_comparison_control::maintenance_scope_text() const -> QString
	{
		return ScopeAuthority == Authority && Authority.has_value() ? ScopeLabel : QString{ };
	}

	void epoch_comparison_control::set_dispatch(
		application::epoch_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto epoch_comparison_control::history_model() const noexcept
		-> epoch_history_model*
	{
		return HistoryModel;
	}

	auto epoch_comparison_control::line() const noexcept -> epoch_line*
	{
		return Line;
	}

	auto epoch_comparison_control::comparing_button() const noexcept -> QToolButton*
	{
		return ComparingButton;
	}

	auto epoch_comparison_control::details() const noexcept -> QWidget*
	{
		return Details;
	}

	void epoch_comparison_control::set_details_visible(const bool aVisible)
	{
		if (Details != nullptr) {
			Details->setVisible(aVisible);
			updateGeometry();
		}
	}

	auto epoch_comparison_control::details_visible() const -> bool
	{
		return Details != nullptr && !Details->isHidden();
	}

	void epoch_comparison_control::dispatch_identity(
		const core::comparison_side             aSide,
		const core::comparison_anchor_identity& aIdentity)
	{
		if (ApplyingProjection || !Dispatch) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		if (!catalog) {
			return;
		}
		Dispatch(core::select_comparison_anchor_intent{
			.Side                    = aSide,
			.Anchor                  = aIdentity,
			.ExpectedCatalogRevision = catalog->Revision,
		});
	}

	void epoch_comparison_control::update_line()
	{
		if (Line == nullptr) {
			return;
		}
		// The model keeps its pages newest first; the line reads left to
		// right in time.
		auto updates = HistoryModel->rows();
		std::ranges::reverse(updates);
		if (OccurrenceHistory) {
			Line->set_occurrences(*OccurrenceHistory, CurrentEpoch, updates, AllowOccurrenceImport);
		} else {
			Line->set_updates(std::move(updates));
		}
		if (Comparison && Comparison->Pair) {
			Line->set_selection(
				Comparison->Pair->Left.Identity, Comparison->Pair->Right.Identity);
		} else {
			Line->set_selection(std::nullopt, std::nullopt);
		}
		Line->set_last_review(LastReviewCommit);
		const auto select = command(core::epoch_command_id::select_comparison);
		Line->set_selection_offered(select && select->Enabled && !HistoryWindowStale);
		update_comparing_text();
	}

	void epoch_comparison_control::update_comparing_text()
	{
		if (Line == nullptr) {
			return;
		}
		auto comparing = QStringLiteral("Comparing exact anchors");
		if (Selection) {
			if (const auto* const named
				= std::get_if<core::named_comparison_selection>(&*Selection);
				named != nullptr) {
				switch (named->Alias) {
				case core::named_comparison::whole_pr:
					comparing = QStringLiteral("Comparing whole PR");
					break;
				case core::named_comparison::previous_upload_to_latest:
					comparing = QStringLiteral("Comparing since last upload");
					break;
				case core::named_comparison::my_last_reviewed_to_latest:
					comparing = QStringLiteral("Comparing since my last review");
					break;
				}
			}
		}
		// [[BPR-1129]]. The dates of the two compared updates follow the name.
		if (const auto dates = Line->selection_dates(); dates && ComparingDatesShown) {
			comparing.append(QStringLiteral(", %1 → %2").arg(dates->first, dates->second));
		}
		if (!TrackOpen && !Readiness.isEmpty()) {
			comparing = Readiness;
		}
		if (ComparingButton != nullptr) {
			const auto foldedReadiness = !TrackOpen && !Readiness.isEmpty();
			const auto displayed       = foldedReadiness ? ComparingButton->fontMetrics().elidedText(comparing, Qt::ElideRight, 270) : comparing;
			if (ComparingButton->text() != displayed) {
				ComparingButton->setText(displayed);
			}
			ComparingButton->setAccessibleDescription(comparing);
			ComparingButton->setMaximumWidth(foldedReadiness ? 320 : QWIDGETSIZE_MAX);
		}
		// [[BPR-1130]]. The comparison's tooltip says what the line row used to
		// say beside it: whether the pair can be read, and the history
		// statement that was the line's caption.
		if (ComparingButton != nullptr && LineSummary != nullptr) {
			auto tip = comparing;
			if (!LineSummary->text().isEmpty()) {
				tip.append(QStringLiteral(" (%1)").arg(LineSummary->text()));
			}
			if (const auto history = Line->statement(); !history.isEmpty()) {
				tip.append(QStringLiteral("\n%1").arg(history));
			}
			if (ComparingButton->toolTip() != tip) {
				ComparingButton->setToolTip(tip);
			}
		}
	}

	void epoch_comparison_control::set_readiness(QString aText, const bool aTrackOpen)
	{
		Readiness = std::move(aText);
		TrackOpen = aTrackOpen;
		ReadinessLabel->setText(Readiness);
		ReadinessLabel->setToolTip(Readiness);
		ReadinessLabel->setAccessibleDescription(Readiness);
		ReadinessLabel->setVisible(TrackOpen && !Readiness.isEmpty());
		Line->setVisible(TrackOpen && Readiness.isEmpty());
		update_comparing_text();
	}

	auto epoch_comparison_control::readiness_text() const -> QString
	{
		return Readiness;
	}

	auto epoch_comparison_control::release_comparing_button() -> QToolButton*
	{
		if (ComparingReleased || ComparingButton == nullptr) {
			return nullptr;
		}
		ComparingReleased = true;
		delete LineName;
		LineName = nullptr;
		ComparingButton->setParent(nullptr);
		return ComparingButton;
	}

	void epoch_comparison_control::set_comparing_dates_shown(const bool aShown)
	{
		if (ComparingDatesShown != aShown) {
			ComparingDatesShown = aShown;
			update_comparing_text();
		}
	}

	auto epoch_comparison_control::left_selector() const noexcept -> QComboBox*
	{
		return LeftSelector;
	}

	auto epoch_comparison_control::right_selector() const noexcept -> QComboBox*
	{
		return RightSelector;
	}

	auto epoch_comparison_control::left_oid_editor() const noexcept -> QLineEdit*
	{
		return LeftOid;
	}

	auto epoch_comparison_control::right_oid_editor() const noexcept -> QLineEdit*
	{
		return RightOid;
	}

	auto epoch_comparison_control::whole_pr_action() const noexcept -> QAction*
	{
		return WholePrAction;
	}

	auto epoch_comparison_control::previous_upload_action() const noexcept
		-> QAction*
	{
		return PreviousUploadAction;
	}

	auto epoch_comparison_control::last_reviewed_action() const noexcept -> QAction*
	{
		return LastReviewedAction;
	}

	auto epoch_comparison_control::load_history_action() const noexcept -> QAction*
	{
		return LoadHistoryAction;
	}

	auto epoch_comparison_control::return_to_newest_action() const noexcept
		-> QAction*
	{
		return ReturnNewestAction;
	}

	auto epoch_comparison_control::refresh_action() const noexcept -> QAction*
	{
		return RefreshAction;
	}

	auto epoch_comparison_control::cancel_action() const noexcept -> QAction*
	{
		return CancelAction;
	}

	auto epoch_comparison_control::prune_action() const noexcept -> QAction*
	{
		return PruneAction;
	}

	auto epoch_comparison_control::delete_cache_action() const noexcept -> QAction*
	{
		return DeleteCacheAction;
	}

	auto epoch_comparison_control::cancel_maintenance_action() const noexcept
		-> QAction*
	{
		return CancelMaintenanceAction;
	}

	auto epoch_comparison_control::status_text() const -> QString
	{
		return StatusLabel->text();
	}

	auto epoch_comparison_control::comparison_available() const noexcept -> bool
	{
		return Comparison && Comparison->AvailableForDiff;
	}

	auto epoch_comparison_control::status() const noexcept -> core::epoch_status
	{
		return Status;
	}

	auto epoch_comparison_control::line_summary() const noexcept -> QLabel*
	{
		return LineSummary;
	}

	auto epoch_comparison_control::comparison_text() const -> QString
	{
		return ComparisonLabel->text();
	}

	auto epoch_comparison_control::statistics() const noexcept
		-> epoch_comparison_control_statistics
	{
		return Statistics;
	}

	void epoch_comparison_control::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::ApplicationPaletteChange:
		case QEvent::PaletteChange:
		case QEvent::ApplicationFontChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
			refresh_native_appearance();
			break;
		default:
			break;
		}
	}

	void epoch_comparison_control::dispatch_alias(
		const core::named_comparison aAlias)
	{
		if (ApplyingProjection || !Dispatch) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		if (!catalog) {
			return;
		}
		Dispatch(core::select_named_comparison_intent{
			.Alias                   = aAlias,
			.ExpectedCatalogRevision = catalog->Revision,
		});
		update_selection_controls();
	}

	void epoch_comparison_control::dispatch_anchor(
		const core::comparison_side aSide,
		const int                   aRow)
	{
		if (ApplyingProjection || !Dispatch || aRow < 0) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		const auto anchor  = HistoryModel->anchor_at(HistoryModel->index(aRow, 0));
		if (!catalog || !anchor) {
			return;
		}
		Dispatch(core::select_comparison_anchor_intent{
			.Side                    = aSide,
			.Anchor                  = anchor->Identity,
			.ExpectedCatalogRevision = catalog->Revision,
		});
		update_selection_controls();
	}

	void epoch_comparison_control::dispatch_older_history_page()
	{
		if (!Dispatch || !Continuation) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		if (!catalog) {
			return;
		}
		Dispatch(core::request_epoch_history_page_intent{
			.Catalog    = *catalog,
			.Navigation = core::epoch_history_navigation::older,
			.Cursor     = Continuation,
			.Limit      = core::maximum_epoch_history_page_size,
		});
	}

	void epoch_comparison_control::dispatch_newest_history_page()
	{
		if (!Dispatch) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		if (!catalog) {
			return;
		}
		Dispatch(core::request_epoch_history_page_intent{
			.Catalog    = *catalog,
			.Navigation = core::epoch_history_navigation::newest,
			.Limit      = core::maximum_epoch_history_page_size,
		});
	}

	void epoch_comparison_control::dispatch_prune()
	{
		if (!Dispatch || !Authority || !Comparison || !Comparison->Pair) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		if (!catalog) {
			return;
		}

		auto       targets     = std::vector<core::review_epoch_id>{ };
		const auto appendEpoch = [&targets](const core::comparison_anchor& aAnchor) {
			if (aAnchor.Identity.Kind != core::comparison_anchor_kind::epoch_base
				&& aAnchor.Identity.Kind
					!= core::comparison_anchor_kind::epoch_head) {
				return;
			}
			targets.push_back(core::review_epoch_id{
				.Authority   = aAnchor.Identity.Authority,
				.Incarnation = aAnchor.Identity.Incarnation,
				.Ordinal     = aAnchor.Identity.Epoch,
			});
		};
		appendEpoch(Comparison->Pair->Left);
		appendEpoch(Comparison->Pair->Right);
		std::ranges::sort(targets);
		const auto duplicate = std::ranges::unique(targets);
		targets.erase(duplicate.begin(), duplicate.end());
		if (targets.empty()) {
			return;
		}

		const auto capturedCatalog = *catalog;
		const auto count           = static_cast<qulonglong>(targets.size());
		const auto text            = maintenance_scope_text() + QStringLiteral("\n\n") + QStringLiteral("Prune %1 selected review epoch(s) from this pull request. This removes "
																										"their private retention refs, so those revisions may become "
																										"unrecoverable after remote history changes. Pruned tombstones remain "
																										"visible. This does not modify GitHub or a developer worktree.")
																							 .arg(count);
		if (!confirm_destructive_action(this,
										QStringLiteral("pruneSelectedEpochsConfirmationDialog"),
										QStringLiteral("Prune selected review epochs?"),
										text,
										QStringLiteral("Prune selected epochs"))) {
			return;
		}
		Dispatch(core::prune_review_epochs_intent{
			.Catalog = capturedCatalog,
			.Epochs  = std::move(targets),
		});
	}

	void epoch_comparison_control::dispatch_delete_cache()
	{
		if (!Dispatch || !Authority) {
			return;
		}
		const auto catalog = HistoryModel->catalog();
		if (!catalog) {
			return;
		}

		const auto capturedCatalog = *catalog;
		const auto capturedCache   = Authority->Cache;
		const auto text            = maintenance_scope_text() + QStringLiteral("\n\n") + QStringLiteral("Delete the current repository cache. This affects every retained "
																										"pull request in this repository cache and removes its private "
																										"retained repository bytes. Cache-deleted tombstones remain visible. "
																										"This does not modify GitHub or a developer worktree.");
		if (!confirm_destructive_action(this,
										QStringLiteral("deleteRepositoryCacheConfirmationDialog"),
										QStringLiteral("Delete repository cache?"),
										text,
										QStringLiteral("Delete repository cache"))) {
			return;
		}
		Dispatch(core::delete_repository_cache_intent{
			.Catalog = capturedCatalog,
			.Cache   = capturedCache,
		});
	}

	void epoch_comparison_control::apply_catalog(
		const application::epoch_catalog_projection& aProjection)
	{
		const auto previousCatalog = HistoryModel->catalog();
		HistoryModel->apply(aProjection);
		AllowOccurrenceImport = aProjection.AllowOccurrenceImport;
		if (aProjection.Replace && !aProjection.OccurrenceHistory) {
			OccurrenceHistory.reset();
			CurrentEpoch.reset();
		}
		if (aProjection.OccurrenceHistory) {
			OccurrenceHistory = aProjection.OccurrenceHistory;
			CurrentEpoch      = aProjection.CurrentEpoch;
		}
		if (aProjection.Replace && !aProjection.Catalog) {
			Continuation.reset();
			HistoryNavigation.reset();
			HistoryPhase.reset();
			HistoryReason      = core::epoch_reason_code::none;
			HistoryWindowStale = false;
		} else if (aProjection.Catalog && previousCatalog
				   && *aProjection.Catalog != *previousCatalog
				   && !aProjection.HistoryPage) {
			Continuation.reset();
		}
		if (aProjection.HistoryPage) {
			HistoryPhase  = aProjection.HistoryPage->Phase;
			HistoryReason = aProjection.HistoryPage->Reason;
			if (aProjection.HistoryPage->Phase
				== core::epoch_history_page_phase::loaded) {
				HistoryNavigation  = aProjection.HistoryPage->Navigation;
				Continuation       = aProjection.HistoryPage->Continuation;
				HistoryWindowStale = false;
			} else if (aProjection.HistoryPage->Phase
					   == core::epoch_history_page_phase::invalidated) {
				Continuation.reset();
				HistoryWindowStale = true;
			}
		}
	}

	void epoch_comparison_control::apply_transition(
		const application::epoch_transition_projection& aProjection)
	{
		if (aProjection.Authority) {
			if (Authority && Authority != aProjection.Authority->Authority) {
				OccurrenceHistory.reset();
				CurrentEpoch.reset();
			}
			Authority = aProjection.Authority->Authority;
			// The head commit of my last review, for the line's amber tick.
			LastReviewCommit.reset();
			if (aProjection.Authority->Revision
				&& aProjection.Authority->Revision->CurrentViewerReview) {
				LastReviewCommit
					= aProjection.Authority->Revision->CurrentViewerReview->Commit;
			}
			if (!aProjection.Authority->Authority) {
				HistoryModel->clear();
				OccurrenceHistory.reset();
				CurrentEpoch.reset();
				Selection.reset();
				Comparison.reset();
				Continuation.reset();
				HistoryNavigation.reset();
				HistoryPhase.reset();
				HistoryWindowStale = false;
			}
		}
		if (aProjection.Selection) {
			Selection = aProjection.Selection->Selection;
			if (Comparison && Comparison->Selection != *Selection) {
				Comparison.reset();
			}
		}
		if (aProjection.Comparison) {
			Comparison = aProjection.Comparison;
			Selection  = aProjection.Comparison->Selection;
		}
		if (aProjection.Status) {
			Status = aProjection.Status->Status;
		}
		if (aProjection.Commands) {
			Commands    = aProjection.Commands->Commands;
			HasCommands = true;
		}
		if (!aProjection.Diagnostics.empty()) {
			DiagnosticReason = aProjection.Diagnostics.back().Code;
		} else if (aProjection.Comparison
				   || (aProjection.Status
					   && aProjection.Status->Status == core::epoch_status::ready)) {
			DiagnosticReason = core::epoch_reason_code::none;
		}
	}

	void epoch_comparison_control::update_controls()
	{
		update_selection_controls();
		update_status_text();
		update_action_state();
		update_line();
	}

	void epoch_comparison_control::update_selection_controls()
	{
		const QSignalBlocker leftBlocker{ LeftSelector };
		const QSignalBlocker rightBlocker{ RightSelector };
		auto                 leftIndex       = -1;
		auto                 rightIndex      = -1;
		auto                 leftOid         = QString{ };
		auto                 rightOid        = QString{ };
		auto                 leftDescription = QStringLiteral(
			"Complete lowercase commit OID; read-only and copyable");
		auto rightDescription        = leftDescription;
		auto leftSelectorDescription = QStringLiteral(
			"Select the exact base or head anchor for the left side");
		auto rightSelectorDescription = QStringLiteral(
			"Select the exact base or head anchor for the right side");

		if (Comparison && Comparison->Pair) {
			const auto& pair  = *Comparison->Pair;
			const auto  left  = HistoryModel->index_for_anchor(pair.Left.Identity);
			const auto  right = HistoryModel->index_for_anchor(pair.Right.Identity);
			if (left.isValid()) {
				leftIndex = left.row();
			} else {
				leftSelectorDescription = QStringLiteral(
					"The exact selected left anchor is outside the loaded "
					"history window; its complete OID remains available below");
			}
			if (right.isValid()) {
				rightIndex = right.row();
			} else {
				rightSelectorDescription = QStringLiteral(
					"The exact selected right anchor is outside the loaded "
					"history window; its complete OID remains available below");
			}
			leftOid          = oid_text(pair.Left.Commit);
			rightOid         = oid_text(pair.Right.Commit);
			leftDescription  = QStringLiteral("Complete lowercase left commit OID; %1")
								   .arg(availability_text(pair.Left.Availability));
			rightDescription = QStringLiteral("Complete lowercase right commit OID; %1")
								   .arg(availability_text(pair.Right.Availability));
		}
		if (LeftSelector->currentIndex() != leftIndex) {
			LeftSelector->setCurrentIndex(leftIndex);
		}
		if (RightSelector->currentIndex() != rightIndex) {
			RightSelector->setCurrentIndex(rightIndex);
		}
		if (LeftOid->text() != leftOid) {
			LeftOid->setText(leftOid);
		}
		if (RightOid->text() != rightOid) {
			RightOid->setText(rightOid);
		}
		LeftOid->setAccessibleDescription(leftDescription);
		RightOid->setAccessibleDescription(rightDescription);
		LeftSelector->setAccessibleDescription(leftSelectorDescription);
		RightSelector->setAccessibleDescription(rightSelectorDescription);

		WholePrAction->setChecked(false);
		PreviousUploadAction->setChecked(false);
		LastReviewedAction->setChecked(false);
		if (Selection) {
			if (const auto* named = std::get_if<core::named_comparison_selection>(&*Selection)) {
				switch (named->Alias) {
				case core::named_comparison::whole_pr:
					WholePrAction->setChecked(true);
					break;
				case core::named_comparison::previous_upload_to_latest:
					PreviousUploadAction->setChecked(true);
					break;
				case core::named_comparison::my_last_reviewed_to_latest:
					LastReviewedAction->setChecked(true);
					break;
				}
			}
		}
	}

	void epoch_comparison_control::update_status_text()
	{
		auto       status   = epoch_status_text(Status);
		const auto retained = HistoryModel->epoch_count() != 0U;
		if (retained && (Status == core::epoch_status::loading_catalog || Status == core::epoch_status::preparing || Status == core::epoch_status::committing || Status == core::epoch_status::resolving)) {
			status.append(QStringLiteral(" The retained comparison remains visible."));
		}
		if (HistoryPhase == core::epoch_history_page_phase::loading) {
			status = retained
				? QStringLiteral(
					  "Loading earlier epochs; retained history remains visible.")
				: QStringLiteral("Loading review history…");
		} else if (HistoryPhase == core::epoch_history_page_phase::failed) {
			status = retained
				? QStringLiteral(
					  "Earlier history could not be loaded; retained epochs "
					  "remain visible.")
				: QStringLiteral("Review history could not be loaded.");
		} else if (HistoryPhase == core::epoch_history_page_phase::cancelled) {
			status = retained
				? QStringLiteral(
					  "History loading was canceled; retained epochs remain "
					  "visible.")
				: QStringLiteral("Review-history loading was canceled.");
		} else if (HistoryPhase == core::epoch_history_page_phase::invalidated) {
			status = retained
				? QStringLiteral(
					  "Retained history changed; the visible page is stale. "
					  "Return to newest epochs to refresh it.")
				: QStringLiteral(
					  "Retained history changed; load the newest epoch page.");
		} else if (HistoryPhase == core::epoch_history_page_phase::loaded
				   && !retained && Status == core::epoch_status::ready) {
			status = QStringLiteral("No review epochs have been recorded.");
		}
		const auto historyReason = reason_text(HistoryReason);
		if (!historyReason.isEmpty()) {
			status.append(QStringLiteral(" %1").arg(historyReason));
		}
		const auto diagnostic = reason_text(DiagnosticReason);
		if (!diagnostic.isEmpty() && DiagnosticReason != HistoryReason) {
			status.append(QStringLiteral(" %1").arg(diagnostic));
		}
		StatusLabel->setText(status);
		Line->set_empty_status(status);

		QString comparison;
		if (Selection) {
			if (const auto* named = std::get_if<core::named_comparison_selection>(&*Selection)) {
				comparison = QStringLiteral("%1. ").arg(alias_text(named->Alias));
			} else {
				comparison = QStringLiteral("Exact epoch pair. ");
			}
		}
		if (Comparison && Comparison->Pair) {
			comparison.append(QStringLiteral("Left %1; right %2. ")
								  .arg(
									  availability_text(
										  Comparison->Pair->Left.Availability),
									  availability_text(
										  Comparison->Pair->Right.Availability)));
			comparison.append(Comparison->AvailableForDiff
								  ? QStringLiteral("Ready for diff.")
								  : QStringLiteral(
										"The exact pair is retained but unavailable for diff."));
		} else {
			comparison.append(QStringLiteral("No exact pair is resolved."));
		}
		if (Comparison) {
			const auto reason = reason_text(Comparison->Reason);
			if (!reason.isEmpty()) {
				comparison.append(QStringLiteral(" %1").arg(reason));
			}
		}
		ComparisonLabel->setText(comparison);
		if (LineSummary != nullptr) {
			// [[BPR-0676]]. Three states and no jargon: nothing chosen yet, a
			// pair that can be read, a pair that cannot. Written here, from
			// the same sentence the details region gets, so the two can never
			// disagree; the sentence itself is the tooltip.
			LineSummary->setText(!Comparison || !Comparison->Pair
									 ? QStringLiteral("no pair yet")
									 : Comparison->AvailableForDiff
									 ? QStringLiteral("pair ready")
									 : QStringLiteral("pair not readable"));
			LineSummary->setToolTip(comparison);
		}
	}

	void epoch_comparison_control::update_action_state()
	{
		const auto select            = command(core::epoch_command_id::select_comparison);
		const auto refresh           = command(core::epoch_command_id::refresh_epochs);
		const auto cancel            = command(core::epoch_command_id::cancel_epoch_refresh);
		const auto prune             = command(core::epoch_command_id::prune_selected_epochs);
		const auto deleteCache       = command(core::epoch_command_id::delete_repository_cache);
		const auto cancelMaintenance = command(core::epoch_command_id::cancel_epoch_maintenance);
		const bool selectEnabled     = select && select->Enabled;
		WholePrAction->setEnabled(selectEnabled);
		PreviousUploadAction->setEnabled(selectEnabled);
		LastReviewedAction->setEnabled(selectEnabled);
		LeftSelector->setEnabled(selectEnabled && !HistoryWindowStale
								 && HistoryModel->rowCount() != 0);
		RightSelector->setEnabled(selectEnabled && !HistoryWindowStale
								  && HistoryModel->rowCount() != 0);
		RefreshAction->setEnabled(refresh && refresh->Enabled);
		CancelAction->setEnabled(cancel && cancel->Enabled);
		PruneAction->setEnabled(prune && prune->Enabled);
		DeleteCacheAction->setEnabled(deleteCache && deleteCache->Enabled);
		CancelMaintenanceAction->setEnabled(
			cancelMaintenance && cancelMaintenance->Enabled);
		const bool pageLoading = HistoryPhase
			== core::epoch_history_page_phase::loading;
		LoadHistoryAction->setEnabled(
			HistoryModel->catalog().has_value() && !pageLoading
			&& !HistoryWindowStale && Continuation.has_value());
		ReturnNewestAction->setEnabled(
			HistoryModel->catalog().has_value() && !pageLoading
			&& (HistoryWindowStale || !HistoryNavigation.has_value()
				|| *HistoryNavigation == core::epoch_history_navigation::older));

		const auto describe =
			[](QAction* const                                       aAction,
			   const std::optional<core::epoch_command_projection>& aValue,
			   const QString&                                       aEnabledText) {
				const auto description = aValue && aValue->Enabled
					? aEnabledText
					: reason_text(
						  aValue ? aValue->DisabledReason
								 : core::epoch_reason_code::no_active_authority);
				aAction->setToolTip(description);
				aAction->setStatusTip(description);
			};
		describe(WholePrAction,
				 select,
				 QStringLiteral("Compare branch base with latest."));
		describe(PreviousUploadAction,
				 select,
				 QStringLiteral("Compare the exact previous upload with latest."));
		describe(LastReviewedAction,
				 select,
				 QStringLiteral("Compare the exact last-reviewed commit with latest."));
		describe(RefreshAction,
				 refresh,
				 QStringLiteral("Refresh retained review revisions."));
		describe(CancelAction,
				 cancel,
				 QStringLiteral("Cancel the active review-history refresh."));
		describe(PruneAction,
				 prune,
				 QStringLiteral(
					 "Prune private retention refs for the exact selected epochs."));
		describe(DeleteCacheAction,
				 deleteCache,
				 QStringLiteral(
					 "Delete every retained pull request in this repository cache."));
		describe(CancelMaintenanceAction,
				 cancelMaintenance,
				 QStringLiteral("Cancel maintenance before its point of no return."));
		LoadHistoryAction->setToolTip(LoadHistoryAction->isEnabled()
										  ? QStringLiteral("Load the next bounded page of retained epochs.")
										  : QStringLiteral("No earlier retained epoch page is available."));
		LoadHistoryAction->setStatusTip(LoadHistoryAction->toolTip());
		ReturnNewestAction->setToolTip(ReturnNewestAction->isEnabled()
										   ? QStringLiteral("Load the newest bounded page of retained epochs.")
										   : QStringLiteral("The newest retained epoch page is visible."));
		ReturnNewestAction->setStatusTip(ReturnNewestAction->toolTip());
	}

	void epoch_comparison_control::refresh_native_appearance()
	{
		if (HistoryModel == nullptr) {
			return;
		}
		HistoryModel->refresh_presentation();
		LeftSelector->updateGeometry();
		RightSelector->updateGeometry();
		updateGeometry();
		++Statistics.AppearanceUpdates;
	}

	void epoch_comparison_control::announce_update()
	{
		const auto announcement = summary_text();
		if (announcement.isEmpty() || announcement == LastAnnouncement) {
			return;
		}
		LastAnnouncement = announcement;
		++Statistics.Announcements;
		QAccessibleAnnouncementEvent event{ this, announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto epoch_comparison_control::summary_text() const -> QString
	{
		auto summary = StatusLabel->text();
		if (!ComparisonLabel->text().isEmpty()) {
			summary.append(QStringLiteral(" %1").arg(ComparisonLabel->text()));
		}
		if (summary.size() > 1'024) {
			summary.truncate(1'024);
		}
		return summary;
	}

	auto epoch_comparison_control::command(const core::epoch_command_id aId) const
		-> std::optional<core::epoch_command_projection>
	{
		if (!HasCommands) {
			return std::nullopt;
		}
		const auto found = std::ranges::find(Commands, aId,
											 &core::epoch_command_projection::Id);
		return found == Commands.end() ? std::nullopt : std::optional{ *found };
	}
}
