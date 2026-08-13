#include "bpr/qt/workbench_window.hpp"

#include "github_presentation_text.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/qt/menu_tool_button.hpp"

#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"

#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace bpr::qt {
	namespace {
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
		static_assert(
			static_cast<std::size_t>(application::file_receipt_action_id::seen)
			== 0U);
		static_assert(
			static_cast<std::size_t>(application::file_receipt_action_id::excluded)
			== 3U);

		/// \brief Recolours one native icon silhouette for the live widget palette.
		///
		/// \details Some styles return fixed black arrow pixels even when a widget
		/// has adopted a dark local palette. Reusing the style's normal pixmap as an
		/// alpha mask preserves its platform-specific arrow geometry while making
		/// the foreground follow the palette. Disabled mode is populated explicitly
		/// so Qt does not synthesize another black or unreadably faded variant.
		[[nodiscard]] auto palette_aware_icon(
			QStyle&                      aStyle,
			const QStyle::StandardPixmap aPixmap,
			const QPalette&              aPalette,
			QWidget* const               aWidget) -> QIcon
		{
			auto source = aStyle.standardIcon(aPixmap, nullptr, aWidget);
			if (source.isNull()) {
				return source;
			}

			auto           result       = QIcon{ };
			constexpr auto logicalSizes = std::array{ 16, 20, 24, 32 };
			constexpr auto scales       = std::array{ 1.0, 1.25, 2.0 };
			constexpr auto modes        = std::array{
				std::pair{ QIcon::Normal, QPalette::Active },
				std::pair{ QIcon::Active, QPalette::Active },
				std::pair{ QIcon::Selected, QPalette::Active },
				std::pair{ QIcon::Disabled, QPalette::Disabled },
			};
			for (const auto& [mode, group] : modes) {
				for (const auto logicalSize : logicalSizes) {
					for (const auto scale : scales) {
						auto pixels = source.pixmap(
							QSize{ logicalSize, logicalSize },
							scale,
							QIcon::Normal,
							QIcon::Off);
						if (pixels.isNull()) {
							continue;
						}
						{
							auto painter = QPainter{ &pixels };
							painter.setCompositionMode(
								QPainter::CompositionMode_SourceIn);
							painter.fillRect(
								pixels.rect(),
								aPalette.color(group, QPalette::ButtonText));
						}
						result.addPixmap(pixels, mode, QIcon::Off);
					}
				}
			}
			return result.isNull() ? source : result;
		}

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
			case application::workbench_host_failure::selected_file_binary:
				return QStringLiteral("The selected file is binary.");
			case application::workbench_host_failure::selected_file_missing_left:
				return QStringLiteral("The selected file is missing from the left revision.");
			case application::workbench_host_failure::selected_file_missing_right:
				return QStringLiteral("The selected file is missing from the right revision.");
			case application::workbench_host_failure::selected_file_missing_both:
				return QStringLiteral("The selected file is absent from both revisions.");
			case application::workbench_host_failure::selected_file_ambiguous:
				return QStringLiteral(
					"The selected rename matches both paths in one revision.");
			case application::workbench_host_failure::selected_file_cancelled:
				return QStringLiteral("Loading the selected file was cancelled.");
			case application::workbench_host_failure::selected_file_stale:
				return QStringLiteral("The selected file changed before it could be loaded.");
			case application::workbench_host_failure::host_failure:
				return QStringLiteral("The workbench could not be updated.");
			}
			return QStringLiteral("The workbench could not be updated.");
		}

		[[nodiscard]] auto receipt_failure_text(
			const application::receipt_failure_publication& aFailure) -> QString
		{
			if (aFailure.Store.has_value()) {
				switch (*aFailure.Store) {
				case application::ledger_store_failure::unavailable:
					return QStringLiteral("Private review progress is unavailable.");
				case application::ledger_store_failure::conflict:
					return QStringLiteral("Private review progress changed elsewhere.");
				case application::ledger_store_failure::integrity_failure:
					return QStringLiteral("Private review progress failed its integrity check.");
				case application::ledger_store_failure::quota_exceeded:
					return QStringLiteral("Private review progress reached its storage limit.");
				case application::ledger_store_failure::cancelled:
					return QStringLiteral("The private progress change was cancelled.");
				case application::ledger_store_failure::invalid_record:
					return QStringLiteral("The private progress change was invalid.");
				}
			}
			switch (aFailure.Rejection) {
			case core::file_receipt_rejection::none:
				break;
			case core::file_receipt_rejection::unknown_file:
				return QStringLiteral("The selected file is no longer current.");
			case core::file_receipt_rejection::material_mismatch:
				return QStringLiteral("The selected file material changed.");
			case core::file_receipt_rejection::excluded_file_cannot_be_accepted:
				return QStringLiteral("Include this file before accepting it.");
			case core::file_receipt_rejection::accepted_file_cannot_be_excluded:
				return QStringLiteral("Revoke acceptance before excluding this file.");
			}
			return QStringLiteral("Private review progress could not be updated.");
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
		workbench_intent_dispatch                       aDispatch,
		workbench_change_dispatch                       aChangeDispatch,
		workbench_drawer_width_dispatch                 aWidthDispatch,
		std::vector<application::github_profile_option> aProfiles,
		application::github_intent_dispatch             aGithubDispatch,
		QWidget* const                                  aParent)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, ChangeDispatch{ std::move(aChangeDispatch) }
		, WidthDispatch{ std::move(aWidthDispatch) }
		, GithubProfiles{ std::move(aProfiles) }
		, GithubDispatch{ std::move(aGithubDispatch) }
	{
		setObjectName(QStringLiteral("workbenchWindow"));
		setWindowTitle(QStringLiteral("BetterPullReview"));
		// The six primary icon controls remain top-level at this supported floor;
		// text never consumes their persistent row width.
		setMinimumSize(480, 360);
		resize(1'280, 820);

		build_primary_toolbar();
		build_central_surface();
		build_github_header();

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

		configure_tool_buttons();
		refresh_toolbar_icons();
		apply_focus_order();
		update_action_state();
		update_status_text();
	}

	workbench_window::~workbench_window() = default;

	void workbench_window::build_primary_toolbar()
	{
		PrimaryToolbar = addToolBar(QStringLiteral("Review navigation"));
		PrimaryToolbar->setObjectName(QStringLiteral("workbenchPrimaryToolbar"));
		PrimaryToolbar->setAccessibleName(QStringLiteral("Focused review navigation"));
		PrimaryToolbar->setAccessibleDescription(QStringLiteral(
			"Files drawer, file and change navigation, secondary review controls, and conversations drawer"));
		PrimaryToolbar->setMovable(false);
		PrimaryToolbar->setFloatable(false);
		PrimaryToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

		const auto makeDrawer = [this](const QString&      aText,
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
			return {
				.Toggle     = action,
				.Name       = aName,
				.Accessible = aAccessible,
				.Collapse   = aCollapse,
				.Expand     = aExpand,
			};
		};

		// Alt+1 through Alt+3 belong to the composed review-history control's
		// named comparisons, so drawer gestures retain their unambiguous Alt+Shift
		// bindings from the existing semantic action path.
		Drawers[0] = makeDrawer(
			QStringLiteral("&Files drawer"),
			QStringLiteral("Files drawer"),
			QStringLiteral("Files drawer toggle"),
			QStringLiteral("toggleWorkbenchFilesDrawerAction"),
			QStringLiteral("Hide the file drawer and give its width to the comparison."),
			QStringLiteral("Show the file drawer at its saved width."),
			QKeySequence{ QStringLiteral("Alt+Shift+1") });
		Drawers[1] = makeDrawer(
			QStringLiteral("&Conversations drawer"),
			QStringLiteral("Conversations drawer"),
			QStringLiteral("Conversations drawer toggle"),
			QStringLiteral("toggleWorkbenchConversationsDrawerAction"),
			QStringLiteral("Hide the conversation drawer and give its width to the comparison."),
			QStringLiteral("Show the conversation drawer at its saved width."),
			QKeySequence{ QStringLiteral("Alt+Shift+2") });

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
			// including while focus is inside either drawer or comparison pane.
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
		Navigation[2] = make(QStringLiteral("Pre&vious change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("previousWorkbenchChangeAction"),
							 QStringLiteral("Select the previous semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgUp") });
		Navigation[3] = make(QStringLiteral("Ne&xt change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("nextWorkbenchChangeAction"),
							 QStringLiteral("Select the next semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgDown") });

		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			connect(Navigation[index].Action,
					&QAction::triggered,
					this,
					[this, index] { dispatch_navigation(index); });
		}

		for (std::size_t index = 0U; index < Drawers.size(); ++index) {
			connect(Drawers[index].Toggle,
					&QAction::triggered,
					this,
					[this, index] { dispatch_drawer_toggle(index); });
		}

		ReviewDetailsAction = new QAction{ QStringLiteral("Review &details"), this };
		ReviewDetailsAction->setObjectName(
			QStringLiteral("toggleWorkbenchReviewDetailsAction"));
		ReviewDetailsAction->setCheckable(true);
		ReviewDetailsAction->setChecked(true);
		ReviewDetailsAction->setShortcut(
			QKeySequence{ QStringLiteral("Alt+Shift+3") });
		ReviewDetailsAction->setShortcutContext(Qt::WindowShortcut);
		ReviewDetailsAction->setToolTip(QStringLiteral(
			"Hide secondary review details and give their height to the comparison."));
		ReviewDetailsAction->setStatusTip(ReviewDetailsAction->toolTip());
		addAction(ReviewDetailsAction);
		connect(
			ReviewDetailsAction,
			&QAction::toggled,
			this,
			[this](const bool aVisible) {
				if (HeaderScroll != nullptr) {
					HeaderScroll->setVisible(aVisible);
				}
				if (EpochSelectors != nullptr) {
					EpochSelectors->setVisible(aVisible);
				}
				if (ComparisonSurface != nullptr
					&& ComparisonSurface->detail_pane() != nullptr) {
					ComparisonSurface->detail_pane()->setVisible(aVisible);
				}
				const auto description = aVisible
					? QStringLiteral(
						  "Hide secondary review details and give their height to the comparison.")
					: QStringLiteral(
						  "Show session, queue, pull-request, epoch, appearance, and selected-change details.");
				ReviewDetailsAction->setToolTip(description);
				ReviewDetailsAction->setStatusTip(description);
				if (auto* const button = qobject_cast<QToolButton*>(
						PrimaryToolbar->widgetForAction(ReviewDetailsAction));
					button != nullptr) {
					button->setAccessibleDescription(description);
				}
			});

		ReviewControlsMenu = new QMenu{ this };
		ReviewControlsMenu->setObjectName(QStringLiteral("workbenchReviewControlsMenu"));
		ReviewControlsMenu->setAccessibleName(QStringLiteral("Private progress actions"));
		ReviewControlsMenu->setAccessibleDescription(QStringLiteral(
			"Seen, read, acceptance, and exclusion actions for the selected exact file material"));
		const auto receiptAction = [this](
									   const std::size_t aIndex,
									   const QString&    aText,
									   const QString&    aObjectName,
									   const QString&    aAccessible) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setCheckable(true);
			action->setIconText(aText);
			action->setEnabled(false);
			addAction(action);
			ReviewControlsMenu->addAction(action);
			ReceiptActions[aIndex] = action;
			connect(action,
					&QAction::triggered,
					this,
					[this, aIndex] { dispatch_receipt(aIndex); });
			action->setToolTip(aAccessible);
			action->setStatusTip(aAccessible);
		};
		receiptAction(0U,
					  QStringLiteral("&Seen"),
					  QStringLiteral("markSelectedFileSeenAction"),
					  QStringLiteral("Mark the selected exact file material seen."));
		receiptAction(1U,
					  QStringLiteral("&Read"),
					  QStringLiteral("toggleSelectedFileReadAction"),
					  QStringLiteral("Change the selected exact file material's read state."));
		receiptAction(2U,
					  QStringLiteral("&Accept"),
					  QStringLiteral("toggleSelectedFileAcceptanceAction"),
					  QStringLiteral("Change private acceptance for the selected exact file material."));
		receiptAction(3U,
					  QStringLiteral("E&xclude"),
					  QStringLiteral("toggleSelectedFileExclusionAction"),
					  QStringLiteral("Change private exclusion for the selected file."));

		ReviewControlsButton = new menu_tool_button{ this };
		ReviewControlsButton->setObjectName(QStringLiteral("workbenchReviewControlsButton"));
		ReviewControlsButton->setText(QStringLiteral("Private &progress"));
		ReviewControlsButton->setShortcut(QKeySequence{ QStringLiteral("Alt+Shift+4") });
		ReviewControlsButton->setAccessibleName(QStringLiteral("Private progress"));
		ReviewControlsButton->setAccessibleDescription(QStringLiteral(
			"Open private seen, read, acceptance, and exclusion actions"));
		ReviewControlsButton->setToolTip(QStringLiteral("Private progress actions"));
		ReviewControlsButton->setStatusTip(ReviewControlsButton->toolTip());
		ReviewControlsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		ReviewControlsButton->setPopupMode(QToolButton::InstantPopup);
		ReviewControlsButton->setFocusPolicy(Qt::StrongFocus);
		ReviewControlsButton->setMenu(ReviewControlsMenu);

		// Exact persistent order: the two semantic drawer edges enclose all four
		// navigation actions. Secondary review controls stay bounded, and the
		// expanding widget is the only item allowed to consume surplus width.
		PrimaryToolbar->addAction(Drawers[0].Toggle);
		for (const auto& entry : Navigation) {
			PrimaryToolbar->addAction(entry.Action);
		}
		PrimaryToolbar->addSeparator();
		PrimaryToolbar->addAction(ReviewDetailsAction);
		auto* const reviewControlsAction = PrimaryToolbar->addWidget(ReviewControlsButton);
		reviewControlsAction->setObjectName(QStringLiteral("workbenchReviewControlsAction"));
		ToolbarSpacer = new QWidget{ PrimaryToolbar };
		ToolbarSpacer->setObjectName(QStringLiteral("workbenchNavigationSpacer"));
		ToolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		auto* const spacerAction = PrimaryToolbar->addWidget(ToolbarSpacer);
		spacerAction->setObjectName(QStringLiteral("workbenchNavigationSpacerAction"));
		PrimaryToolbar->addAction(Drawers[1].Toggle);
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
		// host persists cannot coexist with, and the window must stay usable at
		// the supported compact icon-row width.
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

		// Route 2 of BPR-0191. The conversations drawer shows RFC-BPR-0002's
		// threaded review conversation, which is a forest and cannot be a list,
		// so the drawer keeps its collapse, width and toggle behaviour and hosts
		// the tree instead of its own list model. The files drawer is unchanged:
		// a file list is a list.
		ConversationPanel = new github_conversation_panel{ Drawers[1].Panel };
		ReviewEditor      = new review_draft_editor{
			[this](core::review_draft_command aCommand) {
				if (ReviewDraftDispatch) {
					ReviewDraftDispatch(std::move(aCommand));
				}
			},
			[this](core::file_receipt_command aCommand) {
				if (ReceiptDispatch) {
					ReceiptDispatch(aCommand);
				}
			},
			Drawers[1].Panel,
		};
		ReviewEditor->setObjectName(QStringLiteral("workbenchReviewDraftEditor"));
		ReviewEditor->setMaximumHeight(220);
		DefectCard = new defect_card_view{
			[this](core::defect_command aCommand) {
				++Statistics.DefectIntents;
				if (DefectDispatch) {
					DefectDispatch(std::move(aCommand));
				}
			},
			Drawers[1].Panel,
		};
		DefectCard->setObjectName(QStringLiteral("workbenchDefectCard"));
		ClassifyDefect = new QAction{
			tr("Classify selected conversation as defect"), this
		};
		ClassifyDefect->setObjectName(QStringLiteral("classifySelectedDefect"));
		PreviousDefect = new QAction{ tr("Previous defect"), this };
		PreviousDefect->setObjectName(QStringLiteral("previousDefect"));
		NextDefect = new QAction{ tr("Next defect"), this };
		NextDefect->setObjectName(QStringLiteral("nextDefect"));
		connect(ClassifyDefect, &QAction::triggered, this, [this] {
			classify_selected_conversation();
		});
		connect(PreviousDefect, &QAction::triggered, this, [this] {
			navigate_defect(-1);
		});
		connect(NextDefect, &QAction::triggered, this, [this] {
			navigate_defect(1);
		});
		auto* const conversationContents = new QWidget{ Drawers[1].Panel };
		auto* const conversationLayout   = new QVBoxLayout{ conversationContents };
		conversationLayout->setContentsMargins(0, 0, 0, 0);
		conversationLayout->addWidget(ConversationPanel, 1);
		conversationLayout->addWidget(ReviewEditor);
		AgentPanel = new agent_launch_panel{ { }, conversationContents };
		AgentPanel->setMaximumHeight(300);
		conversationLayout->addWidget(AgentPanel);
		auto* const defectNavigation = new QHBoxLayout{ };
		auto* const classifyButton   = new QToolButton{ conversationContents };
		classifyButton->setDefaultAction(ClassifyDefect);
		defectNavigation->addWidget(classifyButton);
		DefectCountLabel = new QLabel{ QStringLiteral("Defects 0/0"), conversationContents };
		DefectCountLabel->setObjectName(QStringLiteral("defectCount"));
		DefectCountLabel->setAccessibleName(QStringLiteral("Defect queue position"));
		defectNavigation->addWidget(DefectCountLabel, 1);
		for (auto* const action : { PreviousDefect, NextDefect }) {
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			defectNavigation->addWidget(button);
		}
		conversationLayout->addLayout(defectNavigation);
		conversationLayout->addWidget(DefectCard);
		auto* const defectActions = new QHBoxLayout{ };
		for (auto* const action : { DefectCard->verify_action(),
									DefectCard->reopen_action(), DefectCard->withdraw_action(),
									DefectCard->accept_as_is_action(), DefectCard->reclassify_action() }) {
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			defectActions->addWidget(button);
		}
		conversationLayout->addLayout(defectActions);
		Drawers[1].Panel->set_content_widget(
			conversationContents, ConversationPanel->view());
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
		connect(
			Drawers[0].Panel->contents_view()->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex& aCurrent) {
				dispatch_file_selection(aCurrent);
			});
		connect(
			ConversationPanel->view()->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex&) { update_defect_surface(); });
		update_defect_surface();
	}

	void workbench_window::build_github_header()
	{
		// RFC-BPR-0002's session chrome and pull-request metadata sit above the
		// workbench rather than in a window of their own. BPR-0188 decided this
		// window is the review surface; a second window owning the same
		// comparison is the defect that decision closes.
		// Object names, mnemonics, shortcuts and contexts carried over from
		// `github_window` unchanged, so the assertions that moved with them
		// still measure the same thing.
		GithubConnectAction = new QAction{ QStringLiteral("&Connect"), this };
		GithubConnectAction->setObjectName(QStringLiteral("connectGithubAction"));
		GithubConnectAction->setShortcut(
			QKeySequence{ QStringLiteral("Ctrl+Shift+C") });
		GithubConnectAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubConnectAction);
		GithubOpenAction = new QAction{
			QStringLiteral("&Open pull request"), this
		};
		GithubOpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
		GithubOpenAction->setShortcut(QKeySequence::Open);
		GithubOpenAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubOpenAction);
		GithubRefreshAction = new QAction{ QStringLiteral("&Refresh"), this };
		GithubRefreshAction->setObjectName(
			QStringLiteral("refreshPullRequestAction"));
		GithubRefreshAction->setShortcut(QKeySequence::Refresh);
		GithubRefreshAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubRefreshAction);
		ShowReviewThreadsAction = new QAction{
			QStringLiteral("Show review &threads"), this
		};
		ShowReviewThreadsAction->setObjectName(
			QStringLiteral("showReviewThreadsAction"));

		SessionPanel = new github_session_panel{
			GithubProfiles,
			GithubDispatch,
			GithubConnectAction,
			GithubOpenAction,
			GithubRefreshAction,
			github_session_environment{
				.HasPullRequest = [this] { return HasGithubPullRequest; },
				// The panel reports that something it owns changed the
				// presentation -- a status, an identity, a diagnostic. Stubbing
				// this out left the empty state frozen on whatever it said when
				// the window was built.
				.PresentationChanged = [this] { update_github_presentation_state(); },
			},
			nullptr,
		};
		PullRequestPanel = new github_pull_request_panel{ };

		auto* const header = new QWidget;
		header->setObjectName(QStringLiteral("workbenchReviewHeader"));
		header->setAccessibleName(QStringLiteral("Pull request"));
		auto* const column = new QVBoxLayout{ header };
		column->setContentsMargins(0, 0, 0, 0);
		column->addWidget(SessionPanel);
		build_queue_landing(header, *column);

		// The empty state is not decoration. Without it a reviewer with no pull
		// request open sees a comparison of nothing and no indication of what to
		// do, which is what the shell shipped for exactly one commit before this
		// restored it. The session chrome stays visible either way, because
		// connecting is the action the empty state is asking for.
		PullRequestStack = new QStackedWidget{ header };
		PullRequestStack->setObjectName(QStringLiteral("githubViewState"));
		auto* const emptyState = new QWidget{ PullRequestStack };
		emptyState->setObjectName(QStringLiteral("githubEmptyState"));
		auto* const emptyLayout = new QVBoxLayout{ emptyState };
		EmptyStateTitleLabel    = new QLabel{ emptyState };
		EmptyStateTitleLabel->setObjectName(QStringLiteral("githubEmptyStateTitle"));
		EmptyStateTitleLabel->setAccessibleName(QStringLiteral("GitHub view state"));
		EmptyStateTitleLabel->setAlignment(Qt::AlignCenter);
		EmptyStateTitleLabel->setWordWrap(true);
		detail::set_plain_text_label(EmptyStateTitleLabel);
		EmptyStateBodyLabel = new QLabel{ emptyState };
		EmptyStateBodyLabel->setObjectName(QStringLiteral("githubEmptyStateBody"));
		EmptyStateBodyLabel->setAccessibleName(QStringLiteral("GitHub view guidance"));
		EmptyStateBodyLabel->setAlignment(Qt::AlignCenter);
		EmptyStateBodyLabel->setWordWrap(true);
		detail::set_plain_text_label(EmptyStateBodyLabel);
		emptyLayout->addStretch(1);
		emptyLayout->addWidget(EmptyStateTitleLabel);
		emptyLayout->addWidget(EmptyStateBodyLabel);
		emptyLayout->addStretch(2);
		PullRequestStack->addWidget(emptyState);

		auto* const content = new QWidget{ PullRequestStack };
		content->setObjectName(QStringLiteral("githubPullRequestContent"));
		auto* const contentLayout = new QVBoxLayout{ content };
		contentLayout->setContentsMargins(0, 0, 0, 0);
		contentLayout->addWidget(PullRequestPanel);
		PullRequestStack->addWidget(content);
		column->addWidget(PullRequestStack);
		// The panel builds the description but deliberately does not lay it out:
		// where a description belongs is a decision for whoever assembles the
		// window, and this one puts it under the metadata it describes. Bounded
		// rather than stretching, because a long description must not be able to
		// take the height the comparison needs -- the comparison is the review.
		PullRequestPanel->body_editor()->setMaximumHeight(120);
		column->addWidget(PullRequestPanel->body_editor());
		column->addStretch(0);

		// The settings surface lives with the presentation it governs rather than
		// behind a modal dialog, so a reviewer sees a font or mode change take
		// effect on the very comparison they are reading.
		AppearanceSettings = new appearance_settings_panel{ { } };
		AppearanceSettings->setObjectName(
			QStringLiteral("appearanceSettingsPanel"));
		column->addWidget(AppearanceSettings);

		// Scrolled and bounded rather than free to grow. The comparison is what a
		// review is looking at, and a long description or a wide locator must not
		// be able to take the height it needs -- at 820 high with the header
		// unbounded the comparison became too short to hold a correspondence at
		// all, which the composed link map assertion caught.
		HeaderScroll = new QScrollArea{ this };
		HeaderScroll->setObjectName(QStringLiteral("workbenchReviewHeaderScroll"));
		HeaderScroll->setWidgetResizable(true);
		HeaderScroll->setFrameShape(QFrame::NoFrame);
		HeaderScroll->setMaximumHeight(220);
		HeaderScroll->setFocusPolicy(Qt::NoFocus);
		HeaderScroll->setWidget(header);

		auto* const host = new QWidget{ this };
		host->setObjectName(QStringLiteral("workbenchReviewHost"));
		auto* const stack = new QVBoxLayout{ host };
		stack->setContentsMargins(0, 0, 0, 0);
		stack->addWidget(HeaderScroll);
		stack->addWidget(CentralSplitter, 1);
		setCentralWidget(host);

		ConversationPanel->set_reveal_action(ShowReviewThreadsAction);
		connect(ShowReviewThreadsAction, &QAction::triggered, this, [this] {
			focus_review_threads();
		});
		SessionPanel->apply_capabilities({ });
		SessionPanel->apply_rate_limits({ });
		clear_github_pull_request();
		SessionPanel->update_selected_profile();
		SessionPanel->update_connect_enabled();
		// Seeded, exactly as the window it replaces seeded them. A command's
		// enablement and its disabled reason arrive from projections, and until
		// one does every one of these stops would carry an empty accessible
		// description -- which the shell's own "every interactive element is
		// named and described" assertion catches.
		SessionPanel->apply_command({
			.Id             = core::github_command_id::open_pull_request,
			.Enabled        = false,
			.DisabledReason = core::github_reason_code::session_required,
		});
		SessionPanel->apply_command({
			.Id             = core::github_command_id::refresh_pull_request,
			.Enabled        = false,
			.DisabledReason = core::github_reason_code::no_active_pull_request,
		});
		detail::apply_action_state(
			ShowReviewThreadsAction,
			ConversationPanel->reveal_button(),
			false,
			core::github_reason_code::no_active_pull_request,
			QStringLiteral("Focus the review-thread conversations"));
		PullRequestPanel->refresh_native_appearance();
		update_github_presentation_state();
		apply_focus_order();
	}

	void workbench_window::build_queue_landing(
		QWidget* const aParent, QVBoxLayout& aLayout)
	{
		auto* const surface = new QWidget{ aParent };
		surface->setObjectName(QStringLiteral("reviewQueueLanding"));
		surface->setAccessibleName(QStringLiteral("Local review queue"));
		surface->setAccessibleDescription(QStringLiteral(
			"Local observer rules and the pull requests they enrolled; rules never change GitHub assignments."));
		auto* const layout = new QVBoxLayout{ surface };
		layout->setContentsMargins(0, 0, 0, 0);

		QueueStatus = new QLabel{ QStringLiteral("Restoring local observer rules…"), surface };
		QueueStatus->setObjectName(QStringLiteral("reviewQueueStatus"));
		QueueStatus->setAccessibleName(QStringLiteral("Review queue status"));
		QueueStatus->setWordWrap(true);
		layout->addWidget(QueueStatus);

		QueueLanding     = new review_landing_model{ surface };
		QueueFilterModel = new review_landing_filter_model{ *QueueLanding, surface };
		QueueView        = new QListView{ surface };
		QueueView->setObjectName(QStringLiteral("reviewQueueItems"));
		QueueView->setAccessibleName(QStringLiteral("Review queue"));
		QueueView->setAccessibleDescription(QStringLiteral(
			"Pull requests enrolled by a GitHub role or a local observer rule"));
		QueueView->setModel(QueueFilterModel);
		QueueView->setMaximumHeight(84);
		layout->addWidget(QueueView);

		auto* const primaryFilters = new QHBoxLayout{ };
		QueueRoleFilter            = new QComboBox{ surface };
		QueueRoleFilter->setObjectName(QStringLiteral("queueRoleFilter"));
		QueueRoleFilter->setAccessibleName(QStringLiteral("Queue role filter"));
		QueueRoleFilter->setAccessibleDescription(QStringLiteral(
			"Show any GitHub role or one exact RFC review role"));
		QueueRoleFilter->addItem(tr("Role: any"), -1);
		for (const auto role : { core::github_role::none,
								 core::github_role::participant,
								 core::github_role::reviewer,
								 core::github_role::requested_reviewer,
								 core::github_role::author }) {
			QueueRoleFilter->addItem(
				tr("Role: %1").arg(QString::fromUtf8(core::name(role).data())),
				static_cast<int>(role));
		}
		primaryFilters->addWidget(QueueRoleFilter);

		QueueObserverFilter = new QComboBox{ surface };
		QueueObserverFilter->setObjectName(QStringLiteral("queueObserverFilter"));
		QueueObserverFilter->setAccessibleName(
			QStringLiteral("Queue observer reason filter"));
		QueueObserverFilter->setAccessibleDescription(QStringLiteral(
			"Show any observer reason or the exact rule kind that enrolled it"));
		QueueObserverFilter->addItem(tr("Observer: any reason"), -1);
		for (const auto kind : { core::observer_rule_kind::repository,
								 core::observer_rule_kind::author,
								 core::observer_rule_kind::path }) {
			QueueObserverFilter->addItem(
				tr("Observer: %1 rule")
					.arg(QString::fromUtf8(core::name(kind).data())),
				static_cast<int>(kind));
		}
		primaryFilters->addWidget(QueueObserverFilter);

		QueueBucketFilter = new QComboBox{ surface };
		QueueBucketFilter->setObjectName(QStringLiteral("queueBucketFilter"));
		QueueBucketFilter->setAccessibleName(
			QStringLiteral("Queue attention bucket filter"));
		QueueBucketFilter->setAccessibleDescription(QStringLiteral(
			"Show any attention bucket or one exact core-decided reason"));
		QueueBucketFilter->addItem(tr("Bucket: any"), -1);
		for (const auto bucket : { core::attention_bucket::initial_review_requested,
								   core::attention_bucket::changed_since_my_review,
								   core::attention_bucket::new_comments_or_author_replies,
								   core::attention_bucket::fixes_awaiting_verification,
								   core::attention_bucket::authored_awaiting_reviewer_action,
								   core::attention_bucket::observed_with_new_activity,
								   core::attention_bucket::quiet }) {
			QueueBucketFilter->addItem(
				tr("Bucket: %1").arg(QString::fromUtf8(core::name(bucket).data())),
				static_cast<int>(bucket));
		}
		primaryFilters->addWidget(QueueBucketFilter);
		layout->addLayout(primaryFilters);

		auto* const secondaryFilters = new QHBoxLayout{ };
		QueueRepositoryFilter        = new QComboBox{ surface };
		QueueRepositoryFilter->setObjectName(
			QStringLiteral("queueRepositoryFilter"));
		QueueRepositoryFilter->setAccessibleName(
			QStringLiteral("Queue repository filter"));
		QueueRepositoryFilter->setAccessibleDescription(QStringLiteral(
			"Show every repository or one exact repository in this queue"));
		QueueRepositoryFilter->addItem(tr("Repository: any"), QString{ });
		secondaryFilters->addWidget(QueueRepositoryFilter);

		QueueQuietFilter = new QComboBox{ surface };
		QueueQuietFilter->setObjectName(QStringLiteral("queueQuietFilter"));
		QueueQuietFilter->setAccessibleName(
			QStringLiteral("Queue quiet or current filter"));
		QueueQuietFilter->setAccessibleDescription(QStringLiteral(
			"Show every review, only attention, or only quiet and current reviews"));
		QueueQuietFilter->addItem(
			tr("State: any"),
			static_cast<int>(application::queue_quiet_filter::all));
		QueueQuietFilter->addItem(
			tr("State: needs attention"),
			static_cast<int>(application::queue_quiet_filter::attention));
		QueueQuietFilter->addItem(
			tr("State: quiet/current"),
			static_cast<int>(application::queue_quiet_filter::quiet));
		secondaryFilters->addWidget(QueueQuietFilter);

		QueueSort = new QComboBox{ surface };
		QueueSort->setObjectName(QStringLiteral("queueSortSelector"));
		QueueSort->setAccessibleName(QStringLiteral("Queue sort order"));
		QueueSort->setAccessibleDescription(QStringLiteral(
			"Core priority is the default; alternate orders apply only when selected"));
		QueueSort->addItem(
			tr("Sort: core priority"),
			static_cast<int>(application::queue_sort_order::core_priority));
		QueueSort->addItem(
			tr("Sort: repository, then PR"),
			static_cast<int>(application::queue_sort_order::repository));
		QueueSort->addItem(
			tr("Sort: PR number, then identity"),
			static_cast<int>(application::queue_sort_order::pull_request_number));
		secondaryFilters->addWidget(QueueSort);
		layout->addLayout(secondaryFilters);

		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			connect(
				selector,
				qOverload<int>(&QComboBox::currentIndexChanged),
				this,
				[this] { dispatch_queue_preferences(); });
		}

		auto* const controls = new QHBoxLayout{ };
		QueueRuleSelector    = new QComboBox{ surface };
		QueueRuleSelector->setObjectName(QStringLiteral("observerRuleSelector"));
		QueueRuleSelector->setAccessibleName(QStringLiteral("Saved observer rules"));
		QueueRuleSelector->setAccessibleDescription(QStringLiteral(
			"Select an exact local rule to remove"));
		controls->addWidget(QueueRuleSelector);
		QueueRuleKind = new QComboBox{ surface };
		QueueRuleKind->setObjectName(QStringLiteral("observerRuleKind"));
		QueueRuleKind->setAccessibleName(QStringLiteral("Observer rule kind"));
		QueueRuleKind->setAccessibleDescription(QStringLiteral(
			"Choose repository, author, or changed path"));
		for (const auto kind : { core::observer_rule_kind::repository,
								 core::observer_rule_kind::author,
								 core::observer_rule_kind::path }) {
			QueueRuleKind->addItem(
				QString::fromUtf8(core::name(kind).data()),
				static_cast<int>(kind));
		}
		controls->addWidget(QueueRuleKind);
		QueueRulePattern = new QLineEdit{ surface };
		QueueRulePattern->setObjectName(QStringLiteral("observerRulePattern"));
		QueueRulePattern->setAccessibleName(QStringLiteral("Observer rule pattern"));
		QueueRulePattern->setAccessibleDescription(QStringLiteral(
			"Repository owner/name, author login, or path prefix"));
		QueueRulePattern->setPlaceholderText(QStringLiteral("owner/name, login, or path"));
		QueueRulePattern->setMaxLength(
			static_cast<int>(core::maximum_observer_rule_pattern_bytes));
		controls->addWidget(QueueRulePattern, 1);
		QueueRuleExcluding = new QCheckBox{ tr("Exclude"), surface };
		QueueRuleExcluding->setObjectName(QStringLiteral("observerRuleExcluding"));
		QueueRuleExcluding->setAccessibleName(QStringLiteral("Exclude matching rule"));
		QueueRuleExcluding->setAccessibleDescription(QStringLiteral(
			"Exclude a matching local observation; GitHub roles remain visible"));
		controls->addWidget(QueueRuleExcluding);

		AddQueueRule = new QAction{ tr("Add local rule"), this };
		AddQueueRule->setObjectName(QStringLiteral("addObserverRule"));
		RemoveQueueRule = new QAction{ tr("Remove selected rule"), this };
		RemoveQueueRule->setObjectName(QStringLiteral("removeObserverRule"));
		addAction(AddQueueRule);
		AddQueueRuleButton = new QToolButton{ surface };
		AddQueueRuleButton->setDefaultAction(AddQueueRule);
		AddQueueRuleButton->setObjectName(
			QStringLiteral("addObserverRuleButton"));
		AddQueueRuleButton->setAccessibleName(
			QStringLiteral("Add local observer rule"));
		AddQueueRuleButton->setAccessibleDescription(QStringLiteral(
			"Persist the configured observer rule locally without changing GitHub"));
		controls->addWidget(AddQueueRuleButton);

		addAction(RemoveQueueRule);
		RemoveQueueRuleButton = new QToolButton{ surface };
		RemoveQueueRuleButton->setDefaultAction(RemoveQueueRule);
		RemoveQueueRuleButton->setObjectName(
			QStringLiteral("removeObserverRuleButton"));
		RemoveQueueRuleButton->setAccessibleName(
			QStringLiteral("Remove selected observer rule"));
		RemoveQueueRuleButton->setAccessibleDescription(QStringLiteral(
			"Delete the selected persisted local observer rule"));
		controls->addWidget(RemoveQueueRuleButton);
		connect(AddQueueRule, &QAction::triggered, this, [this] {
			dispatch_queue_rule(application::queue_rule_action::add);
		});
		connect(RemoveQueueRule, &QAction::triggered, this, [this] {
			dispatch_queue_rule(application::queue_rule_action::remove);
		});
		connect(QueueRulePattern, &QLineEdit::textChanged, this, [this] {
			update_queue_rule_actions();
		});
		connect(QueueRuleSelector,
				qOverload<int>(&QComboBox::currentIndexChanged),
				this,
				[this] { update_queue_rule_actions(); });
		layout->addLayout(controls);
		aLayout.addWidget(surface);
		update_queue_rule_actions();
	}

	void workbench_window::set_queue_rule_dispatch(
		workbench_queue_rule_dispatch aDispatch)
	{
		QueueRuleDispatch = std::move(aDispatch);
		update_queue_rule_actions();
	}

	void workbench_window::set_queue_preferences_dispatch(
		workbench_queue_preferences_dispatch aDispatch)
	{
		QueuePreferencesDispatch = std::move(aDispatch);
		const auto enabled       = QueueReady
			&& static_cast<bool>(QueuePreferencesDispatch);
		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			selector->setEnabled(enabled);
		}
	}

	void workbench_window::dispatch_queue_preferences()
	{
		if (ApplyingQueuePreferences || !QueueReady
			|| !QueuePreferencesDispatch) {
			return;
		}
		const auto optionalRole = [this]() -> std::optional<core::github_role> {
			const auto value = QueueRoleFilter->currentData().toInt();
			return value < 0
				? std::nullopt
				: std::optional{ static_cast<core::github_role>(value) };
		};
		const auto optionalObserver = [this]()
			-> std::optional<core::observer_rule_kind> {
			const auto value = QueueObserverFilter->currentData().toInt();
			return value < 0
				? std::nullopt
				: std::optional{ static_cast<core::observer_rule_kind>(value) };
		};
		const auto optionalBucket = [this]()
			-> std::optional<core::attention_bucket> {
			const auto value = QueueBucketFilter->currentData().toInt();
			return value < 0
				? std::nullopt
				: std::optional{ static_cast<core::attention_bucket>(value) };
		};
		auto preferences = application::queue_presentation_preferences{
			.Role           = optionalRole(),
			.ObserverReason = optionalObserver(),
			.Bucket         = optionalBucket(),
			.Repository     = QueueRepositoryFilter->currentData().toString().toStdString(),
			.Quiet          = static_cast<application::queue_quiet_filter>(
				QueueQuietFilter->currentData().toInt()),
			.Sort = static_cast<application::queue_sort_order>(
				QueueSort->currentData().toInt()),
		};
		if (!application::valid(preferences)) {
			return;
		}
		QueueFilterModel->apply(preferences);
		QueueProjection.Preferences = preferences;
		update_queue_status(QueueProjection);
		QueuePreferencesDispatch(application::queue_preferences_intent{
			.Preferences = std::move(preferences),
		});
	}

	void workbench_window::apply_queue_preferences(
		const application::queue_presentation_preferences& aPreferences)
	{
		ApplyingQueuePreferences = true;
		const auto select        = [](QComboBox* const aSelector, const int aValue) {
			const auto index = aSelector->findData(aValue);
			aSelector->setCurrentIndex(index >= 0 ? index : 0);
		};
		select(
			QueueRoleFilter,
			aPreferences.Role.has_value()
				? static_cast<int>(*aPreferences.Role)
				: -1);
		select(
			QueueObserverFilter,
			aPreferences.ObserverReason.has_value()
				? static_cast<int>(*aPreferences.ObserverReason)
				: -1);
		select(
			QueueBucketFilter,
			aPreferences.Bucket.has_value()
				? static_cast<int>(*aPreferences.Bucket)
				: -1);
		const auto repository      = QString::fromStdString(aPreferences.Repository);
		const auto repositoryIndex = QueueRepositoryFilter->findData(repository);
		QueueRepositoryFilter->setCurrentIndex(
			repositoryIndex >= 0 ? repositoryIndex : 0);
		select(QueueQuietFilter, static_cast<int>(aPreferences.Quiet));
		select(QueueSort, static_cast<int>(aPreferences.Sort));
		QueueFilterModel->apply(aPreferences);
		ApplyingQueuePreferences = false;
	}

	void workbench_window::rebuild_queue_repositories(
		const std::span<const core::queue_item> aItems,
		const std::string&                      aSelectedRepository)
	{
		auto repositories = std::vector<std::string>{ };
		repositories.reserve(aItems.size());
		for (const auto& item : aItems) {
			repositories.push_back(item.Ref.Repository);
		}
		std::ranges::sort(repositories);
		const auto unique = std::ranges::unique(repositories);
		repositories.erase(unique.begin(), unique.end());
		if (!aSelectedRepository.empty()
			&& !std::ranges::contains(repositories, aSelectedRepository)) {
			repositories.push_back(aSelectedRepository);
			std::ranges::sort(repositories);
		}
		ApplyingQueuePreferences = true;
		QueueRepositoryFilter->clear();
		QueueRepositoryFilter->addItem(tr("Repository: any"), QString{ });
		for (const auto& repository : repositories) {
			const auto text = QString::fromStdString(repository);
			QueueRepositoryFilter->addItem(
				tr("Repository: %1").arg(text), text);
		}
		ApplyingQueuePreferences = false;
	}

	void workbench_window::update_queue_status(
		const application::queue_landing_projection& aProjection)
	{
		switch (aProjection.Status) {
		case application::queue_landing_status::restoring:
			QueueStatus->setText(tr("Restoring local observer rules and queue preferences…"));
			break;
		case application::queue_landing_status::unavailable:
			QueueStatus->setText(tr("Local observer rules and queue preferences are unavailable."));
			break;
		case application::queue_landing_status::ready:
			if (aProjection.Items.empty()) {
				QueueStatus->setText(tr(
					"Review queue is empty. Local rules never change GitHub assignments."));
			} else if (QueueFilterModel->rowCount() == 0
					   && QueueFilterModel->has_active_filters()) {
				QueueStatus->setText(tr(
										 "No reviews match the active queue filters. %1 review(s) remain in the queue.")
										 .arg(aProjection.Items.size()));
			} else {
				QueueStatus->setText(
					tr("%1 of %2 review(s) shown. %3")
						.arg(QueueFilterModel->rowCount())
						.arg(aProjection.Items.size())
						.arg(aProjection.Online
								 ? tr("Refresh is online.")
								 : tr("Refresh is offline.")));
			}
			break;
		}
	}

	void workbench_window::dispatch_queue_rule(
		const application::queue_rule_action aAction)
	{
		if (!QueueRuleDispatch || !QueueReady) {
			return;
		}
		auto rule = core::observer_rule{ };
		if (aAction == application::queue_rule_action::add) {
			rule = {
				.Kind = static_cast<core::observer_rule_kind>(
					QueueRuleKind->currentData().toInt()),
				.Pattern   = QueueRulePattern->text().trimmed().toStdString(),
				.Excluding = QueueRuleExcluding->isChecked(),
			};
		} else {
			const auto index = QueueRuleSelector->currentIndex();
			if (index < 0 || static_cast<std::size_t>(index) >= QueueRules.size()) {
				return;
			}
			rule = QueueRules[static_cast<std::size_t>(index)];
		}
		if (!core::valid(rule)) {
			return;
		}
		++Statistics.QueueRuleIntents;
		QueueRuleDispatch(application::queue_rule_intent{
			.Action = aAction,
			.Rule   = std::move(rule),
		});
	}

	void workbench_window::update_queue_rule_actions()
	{
		if (AddQueueRule == nullptr || RemoveQueueRule == nullptr) {
			return;
		}
		const auto pattern = QueueRulePattern != nullptr
			? QueueRulePattern->text().trimmed().toStdString()
			: std::string{ };
		AddQueueRule->setEnabled(
			QueueReady && static_cast<bool>(QueueRuleDispatch)
			&& core::valid(core::observer_rule{ .Pattern = pattern }));
		RemoveQueueRule->setEnabled(
			QueueReady && static_cast<bool>(QueueRuleDispatch)
			&& QueueRuleSelector != nullptr
			&& QueueRuleSelector->currentIndex() >= 0);
		AddQueueRule->setToolTip(QStringLiteral(
			"Persist a local observer rule; this never changes GitHub."));
		RemoveQueueRule->setToolTip(QStringLiteral(
			"Delete the selected local observer rule."));
	}

	void workbench_window::publish(
		const application::queue_landing_projection& aProjection)
	{
		auto selectedReview = std::optional<core::pull_request_ref>{ };
		if (QueueView->currentIndex().isValid()) {
			if (const auto item = QueueFilterModel->item_at(QueueView->currentIndex());
				item.has_value()) {
				selectedReview = item->Ref;
			}
		}
		auto       selectedRule  = std::optional<core::observer_rule>{ };
		const auto selectedIndex = QueueRuleSelector->currentIndex();
		if (selectedIndex >= 0
			&& static_cast<std::size_t>(selectedIndex) < QueueRules.size()) {
			selectedRule = QueueRules[static_cast<std::size_t>(selectedIndex)];
		}
		QueueReady                = aProjection.Status == application::queue_landing_status::ready;
		const auto filtersEnabled = QueueReady
			&& static_cast<bool>(QueuePreferencesDispatch);
		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			selector->setEnabled(filtersEnabled);
		}
		QueueProjection = aProjection;
		QueueRules      = aProjection.Rules;
		QueueLanding->apply(aProjection.Items);
		rebuild_queue_repositories(
			aProjection.Items, aProjection.Preferences.Repository);
		apply_queue_preferences(aProjection.Preferences);
		if (selectedReview.has_value()) {
			for (auto row = 0; row < QueueFilterModel->rowCount(); ++row) {
				const auto index = QueueFilterModel->index(row, 0);
				const auto item  = QueueFilterModel->item_at(index);
				if (item.has_value() && item->Ref == *selectedReview) {
					QueueView->setCurrentIndex(index);
					break;
				}
			}
		}
		QueueRuleSelector->clear();
		auto restoredIndex = -1;
		for (std::size_t index = 0U; index < QueueRules.size(); ++index) {
			const auto& rule = QueueRules[index];
			QueueRuleSelector->addItem(
				QStringLiteral("%1 %2%3")
					.arg(QString::fromUtf8(core::name(rule.Kind).data()))
					.arg(rule.Excluding ? QStringLiteral("not ") : QString{ })
					.arg(QString::fromStdString(rule.Pattern)));
			if (selectedRule == std::optional{ rule }) {
				restoredIndex = static_cast<int>(index);
			}
		}
		if (restoredIndex >= 0) {
			QueueRuleSelector->setCurrentIndex(restoredIndex);
		}
		update_queue_status(aProjection);
		++Statistics.QueueUpdates;
		update_queue_rule_actions();
		apply_focus_order();
	}

	void workbench_window::publish(
		const application::workbench_projection& aProjection)
	{
		if (aProjection.Commands) {
			Commands    = *aProjection.Commands;
			HasCommands = true;
			++Statistics.CommandUpdates;
		}
		if (aProjection.Files) {
			apply_file_rows(*aProjection.Files);
		}
		if (aProjection.Selection) {
			Cursor = aProjection.Selection->Cursor;
			apply_file_selection();
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

	void workbench_window::apply_file_rows(
		const std::vector<application::workbench_file_projection>& aFiles)
	{
		auto rows = std::vector<workbench_drawer_row>{ };
		rows.reserve(aFiles.size());
		for (const auto& file : aFiles) {
			auto identity = QString{ };
			identity.reserve(
				static_cast<qsizetype>(file.File.bytes().size()) * qsizetype{ 2 });
			for (const auto byte : file.File.bytes()) {
				identity.append(QString::number(byte, 16).rightJustified(2, QLatin1Char{ '0' }));
			}
			const auto path = QString::fromUtf8(file.Path.data(), static_cast<qsizetype>(file.Path.size()));
			rows.push_back(workbench_drawer_row{
				.Identity = std::move(identity),
				.Title    = path,
				.Detail   = path,
				.File     = file.File,
			});
		}
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents()->apply(rows);
		ApplyingFileSelection = false;
		++Statistics.FileRowUpdates;
		apply_file_selection();
	}

	void workbench_window::apply_file_selection()
	{
		if (Drawers[0].Panel->contents() == nullptr
			|| Drawers[0].Panel->contents_view() == nullptr) {
			return;
		}
		const auto index      = Cursor && Cursor->File
			? Drawers[0].Panel->contents()->index_for_file(*Cursor->File)
			: QModelIndex{ };
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents_view()->setCurrentIndex(index);
		ApplyingFileSelection = false;
	}

	void workbench_window::dispatch_file_selection(const QModelIndex& aIndex)
	{
		if (ApplyingFileSelection || !Dispatch || Drawers[0].Panel->contents() == nullptr) {
			return;
		}
		const auto file = Drawers[0].Panel->contents()->file_at(aIndex);
		if (!file.has_value()) {
			return;
		}
		++Statistics.FileSelections;
		Dispatch(core::select_workbench_file_intent{ .File = *file });
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

	void workbench_window::publish(
		const application::file_receipt_action_projection& aProjection)
	{
		Receipt = aProjection;
		if (ReviewEditor != nullptr) {
			ReviewEditor->present(aProjection.Receipt);
		}
		ReceiptFailure.reset();
		LedgerUnavailable = false;
		++Statistics.ReceiptUpdates;
		update_receipt_actions();
		update_status_text();
		announce_update();
	}

	void workbench_window::fail(
		const application::receipt_failure_publication& aFailure)
	{
		ReceiptFailure = aFailure;
		if (aFailure.Store == application::ledger_store_failure::unavailable) {
			LedgerUnavailable = true;
		}
		++Statistics.ReceiptFailures;
		update_receipt_actions();
		update_status_text();
		announce_update();
	}

	void workbench_window::clear_review_ledger(const bool aUnavailable)
	{
		Receipt.reset();
		ReceiptFailure.reset();
		LedgerUnavailable = aUnavailable;
		update_receipt_actions();
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

	void workbench_window::set_receipt_dispatch(
		workbench_receipt_dispatch aReceiptDispatch)
	{
		ReceiptDispatch = std::move(aReceiptDispatch);
	}

	void workbench_window::set_review_draft_dispatch(
		workbench_review_draft_dispatch aDraftDispatch)
	{
		ReviewDraftDispatch = std::move(aDraftDispatch);
	}

	void workbench_window::set_defect_dispatch(
		workbench_defect_dispatch aDefectDispatch)
	{
		DefectDispatch = std::move(aDefectDispatch);
	}

	void workbench_window::set_agent_dispatch(agent_intent_dispatch aAgentDispatch)
	{
		AgentPanel->set_dispatch(std::move(aAgentDispatch));
	}

	void workbench_window::set_defect_classification_dispatch(
		workbench_defect_classification_dispatch aClassificationDispatch)
	{
		DefectClassificationDispatch = std::move(aClassificationDispatch);
		update_defect_surface();
	}

	void workbench_window::present_defects(
		std::vector<core::defect_history> aDefects)
	{
		std::ranges::sort(aDefects, { }, [](const core::defect_history& aDefect) {
			return aDefect.Defect.Id.value();
		});
		auto retained = std::optional<core::defect_id>{ };
		if (ShownDefectIndex.has_value() && *ShownDefectIndex < Defects.size()) {
			retained = Defects[*ShownDefectIndex].Defect.Id;
		}
		Defects = std::move(aDefects);
		ShownDefectIndex.reset();
		if (retained.has_value()) {
			const auto found = std::ranges::find(
				Defects, *retained, [](const core::defect_history& aHistory) {
					return aHistory.Defect.Id;
				});
			if (found != Defects.end()) {
				ShownDefectIndex = static_cast<std::size_t>(
					std::distance(Defects.begin(), found));
			}
		}
		if (!ShownDefectIndex.has_value() && !Defects.empty()) {
			ShownDefectIndex = 0U;
		}
		++Statistics.DefectUpdates;
		update_defect_surface();
	}

	void workbench_window::clear_defects()
	{
		Defects.clear();
		ShownDefectIndex.reset();
		++Statistics.DefectUpdates;
		update_defect_surface();
	}

	void workbench_window::classify_selected_conversation()
	{
		if (!DefectClassificationDispatch || ConversationPanel == nullptr
			|| ConversationPanel->view() == nullptr
			|| ConversationPanel->model() == nullptr) {
			return;
		}
		const auto current = ConversationPanel->view()->currentIndex();
		if (current.data(github_conversation_model::kind_role).toInt()
				!= static_cast<int>(
					application::github_conversation_kind::review_thread)
			|| !current.data(github_conversation_model::line_role).isValid()) {
			return;
		}
		const auto identity = ConversationPanel->model()->id_for_index(current);
		if (identity.empty()) {
			return;
		}
		++Statistics.DefectClassifications;
		DefectClassificationDispatch(identity);
	}

	void workbench_window::navigate_defect(const int aDirection)
	{
		if (!ShownDefectIndex.has_value() || Defects.empty()) {
			return;
		}
		const auto current = static_cast<std::ptrdiff_t>(*ShownDefectIndex);
		const auto next    = current + aDirection;
		if (next < 0 || next >= static_cast<std::ptrdiff_t>(Defects.size())) {
			return;
		}
		ShownDefectIndex = static_cast<std::size_t>(next);
		++Statistics.DefectNavigation;
		update_defect_surface();
	}

	void workbench_window::update_defect_surface()
	{
		const auto current     = ConversationPanel != nullptr
				&& ConversationPanel->view() != nullptr
			? ConversationPanel->view()->currentIndex()
			: QModelIndex{ };
		const auto canClassify = current.isValid()
			&& current.data(github_conversation_model::kind_role).toInt()
				== static_cast<int>(
					application::github_conversation_kind::review_thread)
			&& current.data(github_conversation_model::line_role).isValid();
		ClassifyDefect->setEnabled(
			canClassify && static_cast<bool>(DefectClassificationDispatch));
		const auto validIndex = ShownDefectIndex.has_value()
			&& *ShownDefectIndex < Defects.size();
		PreviousDefect->setEnabled(validIndex && *ShownDefectIndex > 0U);
		NextDefect->setEnabled(
			validIndex && *ShownDefectIndex + 1U < Defects.size());
		DefectCountLabel->setText(
			validIndex
				? tr("Defects %1/%2")
					  .arg(*ShownDefectIndex + 1U)
					  .arg(Defects.size())
				: tr("Defects 0/0"));
		if (validIndex) {
			DefectCard->present(Defects[*ShownDefectIndex]);
		} else {
			DefectCard->setVisible(false);
			return;
		}
		DefectCard->setVisible(true);
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

	auto workbench_window::seen_action() const noexcept -> QAction*
	{
		return ReceiptActions[0];
	}

	auto workbench_window::read_action() const noexcept -> QAction*
	{
		return ReceiptActions[1];
	}

	auto workbench_window::accept_action() const noexcept -> QAction*
	{
		return ReceiptActions[2];
	}

	auto workbench_window::exclude_action() const noexcept -> QAction*
	{
		return ReceiptActions[3];
	}

	auto workbench_window::review_controls_button() const noexcept
		-> QToolButton*
	{
		return ReviewControlsButton;
	}

	auto workbench_window::review_controls_menu() const noexcept -> QMenu*
	{
		return ReviewControlsMenu;
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

	void workbench_window::publish(
		const application::github_presentation_update& aUpdate)
	{
		if (aUpdate.Transition.has_value()) {
			apply_github_transition(*aUpdate.Transition);
		}
		if (aUpdate.Session.has_value()) {
			SessionPanel->apply_session(*aUpdate.Session);
		}
		if (aUpdate.PullRequest.has_value()) {
			apply_github_pull_request(*aUpdate.PullRequest);
		}
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			apply_github_host_diagnostic(diagnostic);
		}
		announce_github_presentation_update();
	}

	void workbench_window::announce_github_presentation_update()
	{
		// One polite announcement per material change, not one per projection
		// field. A screen reader that is told the same thing twice has been told
		// nothing the second time.
		if (SessionPanel == nullptr || SessionPanel->status_banner() == nullptr) {
			return;
		}
		const auto announcement = SessionPanel->summary_text();
		if (announcement.isEmpty() || announcement == LastGithubAnnouncement) {
			return;
		}
		LastGithubAnnouncement = announcement;
		QAccessibleAnnouncementEvent event{ SessionPanel->status_banner(),
											announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	void workbench_window::apply_github_transition(
		const application::github_transition_projection& aTransition)
	{
		for (const auto& command : aTransition.Commands) {
			if (!SessionPanel->apply_command(command)) {
				apply_github_content_command(command);
			}
		}
		if (aTransition.Identity.has_value()) {
			SessionPanel->apply_identity(*aTransition.Identity);
			++Statistics.GithubIdentityUpdates;
		}
		if (aTransition.Capabilities.has_value()) {
			SessionPanel->apply_capabilities(
				aTransition.Capabilities->Capabilities);
		}
		if (aTransition.PullRequest.has_value()
			&& !aTransition.PullRequest->Summary.has_value()) {
			clear_github_pull_request();
		}
		if (aTransition.Status.has_value()) {
			SessionPanel->apply_status(aTransition.Status->Status);
		}
		for (const auto& diagnostic : aTransition.Diagnostics) {
			SessionPanel->apply_diagnostic(diagnostic.Code);
			++Statistics.GithubDiagnosticUpdates;
		}
	}

	void workbench_window::apply_github_pull_request(
		const application::github_pull_request_projection& aPullRequest)
	{
		PullRequestPanel->adopt(aPullRequest);
		// The gap BPR-0191 recorded closes here. In the two-window arrangement
		// the anchor could only follow a conversation projection, because nothing
		// observed a comparison context change. The shell composes both surfaces,
		// so it sets the anchor from the file the reviewer is actually reading.
		ConversationPanel->set_anchor_file(
			ComparisonSurface != nullptr
					&& ComparisonSurface->current_context().has_value()
				? std::optional<core::file_identity>{
					  ComparisonSurface->current_context()->Key.File }
				: std::nullopt);
		ConversationPanel->adopt(
			aPullRequest.Conversations,
			detail::review_thread_availability_text(
				aPullRequest.ReviewThreadsAvailable,
				aPullRequest.ReviewThreadCount,
				aPullRequest.Capabilities.ReviewThreads));
		HasGithubPullRequest = true;
		SessionPanel->apply_capabilities(aPullRequest.Capabilities);
		SessionPanel->apply_rate_limits(aPullRequest.RateLimits);
		update_github_presentation_state();
	}

	void workbench_window::apply_github_content_command(
		const core::github_command_projection& aCommand)
	{
		if (aCommand.Id != core::github_command_id::show_review_threads) {
			return;
		}
		detail::apply_action_state(
			ShowReviewThreadsAction,
			ConversationPanel->reveal_button(),
			aCommand.Enabled,
			aCommand.DisabledReason,
			QStringLiteral("Focus the review-thread conversations"));
	}

	void workbench_window::apply_github_host_diagnostic(
		const application::github_host_diagnostic_projection& aDiagnostic)
	{
		SessionPanel->apply_rate_limits(aDiagnostic.RateLimits);
		SessionPanel->apply_diagnostic(
			aDiagnostic.Reason,
			aDiagnostic.RetryAfterSeconds,
			aDiagnostic.RateLimitCause,
			true);
	}

	void workbench_window::clear_github_pull_request()
	{
		HasGithubPullRequest = false;
		PullRequestPanel->clear();
		ConversationPanel->clear(QStringLiteral(
			"Review threads are unavailable until a pull request is open."));
		update_github_presentation_state();
	}

	void workbench_window::update_github_presentation_state()
	{
		// The session panel reports a presentation change from inside its own
		// constructor, before the member holding it has been assigned, so this
		// can run with nothing to read. Guarding here rather than deferring the
		// callback keeps the panel free to report whenever it likes.
		if (SessionPanel == nullptr || PullRequestStack == nullptr) {
			return;
		}
		PullRequestStack->setCurrentIndex(HasGithubPullRequest ? 1 : 0);

		QString state;
		if (HasGithubPullRequest) {
			switch (SessionPanel->status()) {
			case core::github_status::loading_pull_request:
				state = QStringLiteral("refreshing");
				break;
			case core::github_status::pull_request_failed:
				state = QStringLiteral("loaded-error");
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("loaded-cancelled");
				break;
			default:
				state = QStringLiteral("loaded");
				break;
			}
		} else {
			switch (SessionPanel->status()) {
			case core::github_status::no_profile:
				state = QStringLiteral("disconnected");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connect to GitHub"));
				EmptyStateBodyLabel->setText(
					GithubProfiles.empty()
						? QStringLiteral(
							  "No GitHub host profiles are configured.")
						: QStringLiteral(
							  "Choose a host profile above, then connect."));
				break;
			case core::github_status::establishing_session:
				state = QStringLiteral("connecting");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connecting to GitHub…"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The selected host is establishing an authenticated "
						"session."));
				break;
			case core::github_status::session_ready:
			case core::github_status::pull_request_ready:
				state = QStringLiteral("empty");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Open a pull request"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"Enter an owner, repository, and pull request number "
						"above."));
				break;
			case core::github_status::loading_pull_request:
				state = QStringLiteral("loading");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Loading pull request…"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"BetterPullReview is reading the selected GitHub "
						"snapshot."));
				break;
			case core::github_status::session_failed:
				state = QStringLiteral("error");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connection unavailable"));
				EmptyStateBodyLabel->setText(
					!SessionPanel->showing_diagnostic()
						? QStringLiteral(
							  "Review the connection details and try again.")
						: QString::fromStdString(SessionPanel->diagnostic_text()));
				break;
			case core::github_status::session_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connection canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"Choose Connect when you are ready to try again."));
				break;
			case core::github_status::pull_request_failed:
				state = QStringLiteral("error");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request unavailable"));
				EmptyStateBodyLabel->setText(
					!SessionPanel->showing_diagnostic()
						? QStringLiteral(
							  "Check the locator and try opening it again.")
						: QString::fromStdString(SessionPanel->diagnostic_text()));
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request loading canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The locator is unchanged and can be opened again."));
				break;
			}
		}
		PullRequestStack->setProperty("githubPresentationState", state);

		if (auto* const banner = SessionPanel->status_banner();
			banner != nullptr) {
			banner->setAccessibleDescription(SessionPanel->summary_text());
		}
	}

	void workbench_window::focus_review_threads()
	{
		for (auto row = 0; row < ConversationPanel->model()->rowCount(); ++row) {
			const auto modelIndex = ConversationPanel->model()->index(row, 0);
			if (
				modelIndex.data(github_conversation_model::kind_role).toInt()
				== static_cast<int>(
					application::github_conversation_kind::review_thread)) {
				ConversationPanel->view()->expand(modelIndex);
				ConversationPanel->view()->setCurrentIndex(modelIndex);
				ConversationPanel->view()->scrollTo(
					modelIndex, QAbstractItemView::PositionAtCenter);
				break;
			}
		}
		ConversationPanel->view()->setFocus(Qt::ShortcutFocusReason);
	}

	auto workbench_window::appearance_settings() const noexcept
		-> appearance_settings_panel*
	{
		return AppearanceSettings;
	}

	auto workbench_window::github_status_text() const -> std::string
	{
		return SessionPanel->status_text();
	}

	auto workbench_window::github_diagnostic_text() const -> std::string
	{
		return SessionPanel->diagnostic_text();
	}

	auto workbench_window::active_host_text() const -> std::string
	{
		return SessionPanel->active_host_text();
	}

	auto workbench_window::active_account_text() const -> std::string
	{
		return SessionPanel->active_account_text();
	}

	auto workbench_window::conversation_view() const noexcept -> QTreeView*
	{
		return ConversationPanel->view();
	}

	auto workbench_window::conversation_model() const noexcept
		-> github_conversation_model*
	{
		return ConversationPanel->model();
	}

	auto workbench_window::profile_selector() const noexcept -> QComboBox*
	{
		return SessionPanel->profile_selector();
	}

	auto workbench_window::session_panel() const noexcept -> github_session_panel*
	{
		return SessionPanel;
	}

	auto workbench_window::pull_request_panel() const noexcept
		-> github_pull_request_panel*
	{
		return PullRequestPanel;
	}

	auto workbench_window::github_connect_action() const noexcept -> QAction*
	{
		return GithubConnectAction;
	}

	auto workbench_window::github_open_action() const noexcept -> QAction*
	{
		return GithubOpenAction;
	}

	auto workbench_window::github_refresh_action() const noexcept -> QAction*
	{
		return GithubRefreshAction;
	}

	auto workbench_window::show_review_threads_action() const noexcept -> QAction*
	{
		return ShowReviewThreadsAction;
	}

	auto workbench_window::conversation_panel() const noexcept
		-> github_conversation_panel*
	{
		return ConversationPanel;
	}

	auto workbench_window::agent_panel() const noexcept -> agent_launch_panel*
	{
		return AgentPanel;
	}

	auto workbench_window::review_editor() const noexcept -> review_draft_editor*
	{
		return ReviewEditor;
	}

	auto workbench_window::defect_card() const noexcept -> defect_card_view*
	{
		return DefectCard;
	}

	auto workbench_window::classify_defect_action() const noexcept -> QAction*
	{
		return ClassifyDefect;
	}

	auto workbench_window::previous_defect_action() const noexcept -> QAction*
	{
		return PreviousDefect;
	}

	auto workbench_window::next_defect_action() const noexcept -> QAction*
	{
		return NextDefect;
	}

	auto workbench_window::defect_count_label() const noexcept -> QLabel*
	{
		return DefectCountLabel;
	}

	auto workbench_window::shown_defect_index() const noexcept
		-> std::optional<std::size_t>
	{
		return ShownDefectIndex;
	}

	auto workbench_window::defect_count() const noexcept -> std::size_t
	{
		return Defects.size();
	}

	auto workbench_window::queue_landing() const noexcept -> review_landing_model*
	{
		return QueueLanding;
	}

	auto workbench_window::queue_filter_model() const noexcept
		-> review_landing_filter_model*
	{
		return QueueFilterModel;
	}

	auto workbench_window::queue_view() const noexcept -> QListView*
	{
		return QueueView;
	}

	auto workbench_window::select_queue_item(
		const core::pull_request_ref& aRef) -> bool
	{
		for (auto row = 0; row < QueueFilterModel->rowCount(); ++row) {
			const auto index = QueueFilterModel->index(row, 0);
			const auto item  = QueueFilterModel->item_at(index);
			if (item.has_value() && item->Ref == aRef) {
				QueueView->setCurrentIndex(index);
				QueueView->scrollTo(index, QAbstractItemView::PositionAtCenter);
				QueueView->setFocus(Qt::OtherFocusReason);
				update_queue_status(QueueProjection);
				return true;
			}
		}

		const auto present = [this, &aRef] {
			for (auto row = 0; row < QueueLanding->rowCount(); ++row) {
				const auto item = QueueLanding->item_at(QueueLanding->index(row, 0));
				if (item.has_value() && item->Ref == aRef) {
					return true;
				}
			}
			return false;
		}();
		QueueStatus->setText(
			present
				? tr("The notified review is hidden by the active queue filters.")
				: tr("The notified review is no longer in the local review queue."));
		return false;
	}

	auto workbench_window::queue_role_filter() const noexcept -> QComboBox*
	{
		return QueueRoleFilter;
	}

	auto workbench_window::queue_observer_filter() const noexcept -> QComboBox*
	{
		return QueueObserverFilter;
	}

	auto workbench_window::queue_bucket_filter() const noexcept -> QComboBox*
	{
		return QueueBucketFilter;
	}

	auto workbench_window::queue_repository_filter() const noexcept -> QComboBox*
	{
		return QueueRepositoryFilter;
	}

	auto workbench_window::queue_quiet_filter() const noexcept -> QComboBox*
	{
		return QueueQuietFilter;
	}

	auto workbench_window::queue_sort_selector() const noexcept -> QComboBox*
	{
		return QueueSort;
	}

	auto workbench_window::queue_rule_kind_selector() const noexcept -> QComboBox*
	{
		return QueueRuleKind;
	}

	auto workbench_window::queue_rule_pattern_editor() const noexcept -> QLineEdit*
	{
		return QueueRulePattern;
	}

	auto workbench_window::queue_rule_exclusion() const noexcept -> QCheckBox*
	{
		return QueueRuleExcluding;
	}

	auto workbench_window::add_queue_rule_action() const noexcept -> QAction*
	{
		return AddQueueRule;
	}

	auto workbench_window::remove_queue_rule_action() const noexcept -> QAction*
	{
		return RemoveQueueRule;
	}

	auto workbench_window::queue_status_label() const noexcept -> QLabel*
	{
		return QueueStatus;
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

	auto workbench_window::review_details_toggle_action() const noexcept -> QAction*
	{
		return ReviewDetailsAction;
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

		append(qobject_cast<QToolButton*>(
			PrimaryToolbar->widgetForAction(Drawers[0].Toggle)));
		for (const auto& entry : Navigation) {
			append(qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Action)));
		}
		append(qobject_cast<QToolButton*>(
			PrimaryToolbar->widgetForAction(ReviewDetailsAction)));
		append(ReviewControlsButton);
		append(qobject_cast<QToolButton*>(
			PrimaryToolbar->widgetForAction(Drawers[1].Toggle)));
		// The review header sits under the toolbars and above the workbench, so
		// its stops are declared there. Reading order and focus order agree.
		if (SessionPanel != nullptr) {
			append(SessionPanel->profile_selector());
			append(SessionPanel->connect_button());
			append(SessionPanel->connection_details_button());
			append(SessionPanel->owner_editor());
			append(SessionPanel->repository_editor());
			append(SessionPanel->pull_request_editor());
			append(SessionPanel->open_button());
			append(SessionPanel->refresh_button());
		}
		append(QueueView);
		append(QueueRoleFilter);
		append(QueueObserverFilter);
		append(QueueBucketFilter);
		append(QueueRepositoryFilter);
		append(QueueQuietFilter);
		append(QueueSort);
		append(QueueRuleSelector);
		append(QueueRuleKind);
		append(QueueRulePattern);
		append(QueueRuleExcluding);
		append(AddQueueRuleButton);
		append(RemoveQueueRuleButton);
		if (PullRequestPanel != nullptr) {
			append(PullRequestPanel->technical_details_button());
			append(PullRequestPanel->body_editor());
		}
		append(Drawers[0].Panel->focus_stop());
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
		append(Drawers[1].Panel->focus_stop());
		if (ReviewEditor != nullptr) {
			append(ReviewEditor->body_editor());
		}
		if (DefectCard != nullptr) {
			append(DefectCard->severity_selector());
			append(DefectCard->category_selector());
		}
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
			refresh_toolbar_icons();
			// The pull-request title and identity carry derived emphasis fonts,
			// which stop tracking the application font unless they are re-derived
			// here. `github_window` did this; the shell must too, or a reviewer
			// who enlarges the system font gets a title that does not grow.
			if (PullRequestPanel != nullptr) {
				PullRequestPanel->refresh_native_appearance();
			}
			if (ConversationPanel != nullptr) {
				ConversationPanel->relayout();
			}
			break;
		default:
			break;
		}
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

	void workbench_window::dispatch_receipt(const std::size_t aIndex)
	{
		update_receipt_actions();
		if (!ReceiptDispatch || !Receipt.has_value()
			|| aIndex >= Receipt->Actions.size()
			|| !Receipt->Actions[aIndex].Enabled) {
			return;
		}
		++Statistics.ReceiptGestures;
		ReceiptDispatch(Receipt->Actions[aIndex].Command);
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
				PrimaryToolbar->widgetForAction(entry.Toggle));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Collapse);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(ReviewDetailsAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Review details toggle"));
			button->setAccessibleDescription(ReviewDetailsAction->toolTip());
			button->setFocusPolicy(Qt::StrongFocus);
		}
	}

	void workbench_window::refresh_toolbar_icons()
	{
		auto* const nativeStyle = style();
		if (nativeStyle == nullptr) {
			return;
		}
		Drawers[0].Toggle->setIcon(nativeStyle->standardIcon(
			QStyle::SP_DirOpenIcon, nullptr, this));
		Navigation[0].Action->setIcon(palette_aware_icon(
			*nativeStyle, QStyle::SP_ArrowUp, palette(), this));
		Navigation[1].Action->setIcon(palette_aware_icon(
			*nativeStyle, QStyle::SP_ArrowDown, palette(), this));
		Navigation[2].Action->setIcon(palette_aware_icon(
			*nativeStyle, QStyle::SP_ArrowBack, palette(), this));
		Navigation[3].Action->setIcon(palette_aware_icon(
			*nativeStyle, QStyle::SP_ArrowForward, palette(), this));
		ReviewDetailsAction->setIcon(nativeStyle->standardIcon(
			QStyle::SP_FileDialogInfoView, nullptr, this));
		ReviewControlsButton->setIcon(nativeStyle->standardIcon(
			QStyle::SP_DialogApplyButton, nullptr, this));
		Drawers[1].Toggle->setIcon(nativeStyle->standardIcon(
			QStyle::SP_FileDialogListView, nullptr, this));

		constexpr auto receiptIcons = std::array{
			QStyle::SP_DialogYesButton,
			QStyle::SP_DialogApplyButton,
			QStyle::SP_DialogOkButton,
			QStyle::SP_DialogCancelButton,
		};
		for (std::size_t index = 0U; index < ReceiptActions.size(); ++index) {
			ReceiptActions[index]->setIcon(nativeStyle->standardIcon(
				receiptIcons[index], nullptr, this));
		}
	}

	void workbench_window::apply_focus_order()
	{
		const auto order = focus_order();
		if (order.isEmpty()) {
			return;
		}
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
		update_receipt_actions();
	}

	void workbench_window::update_receipt_actions()
	{
		const auto disabled = LedgerUnavailable
			? QStringLiteral("Private review progress is unavailable.")
			: QStringLiteral("Select an available text comparison first.");
		for (std::size_t index = 0U; index < ReceiptActions.size(); ++index) {
			auto* const action = ReceiptActions[index];
			if (action == nullptr) {
				continue;
			}
			const auto projected = Receipt.has_value()
					&& index < Receipt->Actions.size()
				? &Receipt->Actions[index]
				: nullptr;
			const auto kind      = projected != nullptr
				? projected->Command.Kind
				: index == 0U
				? core::file_receipt_command_kind::mark_seen
				: index == 1U
				? core::file_receipt_command_kind::mark_read
				: index == 2U
				? core::file_receipt_command_kind::accept
				: core::file_receipt_command_kind::exclude;
			const auto label     = [&] {
				switch (kind) {
				case core::file_receipt_command_kind::mark_seen:
					return QStringLiteral("&Seen");
				case core::file_receipt_command_kind::mark_read:
					return QStringLiteral("&Read");
				case core::file_receipt_command_kind::mark_unread:
					return QStringLiteral("Mark &unread");
				case core::file_receipt_command_kind::accept:
					return QStringLiteral("&Accept");
				case core::file_receipt_command_kind::revoke_acceptance:
					return QStringLiteral("Revoke &acceptance");
				case core::file_receipt_command_kind::exclude:
					return QStringLiteral("E&xclude");
				case core::file_receipt_command_kind::include:
					return QStringLiteral("&Include");
				}
				return QStringLiteral("Private progress");
			}();
			action->setText(label);
			action->setIconText(label);
			const auto checked = Receipt.has_value()
				&& (index == 0U
						? Receipt->Receipt.Read != core::file_read_state::unread
						: index == 1U
						? Receipt->Receipt.Read == core::file_read_state::read
						: index == 2U ? Receipt->Receipt.Accepted
									  : Receipt->Receipt.Excluded);
			action->setChecked(checked);
			describe_action(
				action,
				projected != nullptr && projected->Enabled && !LedgerUnavailable,
				QStringLiteral("Update private progress for the selected exact file material."),
				disabled);
		}
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
		const auto enabledText = expanded ? entry.Collapse : entry.Expand;
		describe_action(
			entry.Toggle,
			projection && projection->Enabled,
			enabledText,
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Toggle));
			button != nullptr) {
			button->setAccessibleDescription(entry.Toggle->toolTip());
		}
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
		if (LedgerUnavailable) {
			status.append(QStringLiteral(" Private review progress is unavailable."));
		} else if (ReceiptFailure.has_value()) {
			status.append(QStringLiteral(" %1").arg(receipt_failure_text(*ReceiptFailure)));
		} else if (Receipt.has_value()) {
			status.append(QStringLiteral(" Private progress: %1, %2%3.")
							  .arg(Receipt->Receipt.Read == core::file_read_state::read
									   ? QStringLiteral("read")
									   : Receipt->Receipt.Read == core::file_read_state::seen
									   ? QStringLiteral("seen")
									   : QStringLiteral("unread"))
							  .arg(Receipt->Receipt.Accepted
									   ? QStringLiteral("accepted")
									   : QStringLiteral("not accepted"))
							  .arg(Receipt->Receipt.Excluded
									   ? QStringLiteral(", excluded")
									   : QString{ }));
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
