#include "bpr/qt/workbench_window.hpp"
#include "bpr/qt/github_thread_map.hpp"

#include "github_presentation_text.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/qt/menu_tool_button.hpp"

#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/native_window_chrome.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

		/// \brief The width the conversations drawer opens at, in logical pixels.
		///
		/// \details Stated here because the alternative is what the drawer had
		/// before: whichever row was widest set the column's size hint, the
		/// splitter handed the drawer exactly that, and then the vertical scroll
		/// bar took 18 px back out of it. So the widest row was always the row
		/// that lost its last control, at whatever width the drawer opened -
		/// 540 px while the defect rows were horizontal, 366 px once BPR-0520
		/// stacked them and the agent launcher's run controls became the widest
		/// row instead. A number nobody chose, that moved whenever a label did,
		/// and that was wrong by the scroll bar's width every time.
		///
		/// Measured at 100 % with every row of the drawer stacked (BPR-0506,
		/// BPR-0520, BPR-0527): the column asks for 357 px to draw every control
		/// at its natural width, needs 251 px for its widest single control -
		/// *Classify selected conversation as defect* - below which it starts
		/// losing controls again, and the vertical scroll bar takes 18 px that
		/// the column never gets. 357 plus 18 is this. A style or system font
		/// whose metrics run wider than this machine's - the headless Linux gate
		/// uses a different style from macOS - squeezes the column toward the
		/// 251 it needs rather than clipping it, which is why the number that is
		/// stated is the one the column asks for and not the one it needs.
		///
		/// It is a floor on the column, not on the drawer. The panel keeps its
		/// own explicit `minimum_workbench_drawer_width`, so a reviewer can still
		/// drag the drawer narrower and the host still persists what they chose.
		/// What holds at those narrower widths is that every row is a column, so
		/// a row cannot put its last control out of reach; this number is only
		/// what the product opens at.
		constexpr int conversation_drawer_default_width = 375;
		/// \brief The least width the conversation takes in Discuss ([[BPR-0649]]).
		constexpr int discuss_conversation_width = 640;
		/// \brief Narrowest width a drawer is ever presented at when expanded.
		///
		/// \details [[BPR-0662]]. The host's floor is 64 pixels, the least it
		/// will retain; a persisted width beneath this one was not chosen by a
		/// reviewer but written by a squeeze (a hidden splitter laid out at
		/// its default size wrote 64 for every drawer), so the window grants
		/// the drawer's design width instead and the next drag persists a
		/// real one.
		constexpr std::uint32_t minimum_presented_drawer_width = 160U;
#if defined(__APPLE__)
		/// \brief Width the macOS window buttons occupy at the strip's left.
		constexpr int traffic_light_inset = 78;
#endif

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
#if defined(__APPLE__)
		// [[BPR-0646]]. The strip is the title row. The window's content
		// extends under a transparent title bar whose text is hidden, so the
		// traffic lights sit in the strip's left inset and the identity the
		// strip shows is the title, as Fork and Visual Studio Code do. The
		// safe-area margin Qt would otherwise add above the content is
		// declined: the strip takes that row itself.
		setWindowFlag(Qt::ExpandedClientAreaHint, true);
		setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
		setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif
		// [[BPR-0646]]. Every strip control stays top-level at this supported
		// floor: the eleven bordered buttons and the four modality segments
		// fit with nothing folded into an overflow menu. The identity and the
		// two position labels are the only strip items that yield width, and
		// they do so before the floor (see apply_strip_width).
		setMinimumSize(900, 360);
		resize(1'280, 820);

		build_menu_bar();
		build_primary_toolbar();
		build_central_surface();
		build_github_header();

		StatusLabel = new QLabel{ this };
		StatusLabel->setObjectName(QStringLiteral("workbenchStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("Workbench status"));
		StatusLabel->setAccessibleDescription(QStringLiteral(
			"Selected file position, host failures, and review-history state in "
			"text"));
		// QStatusBar temporarily hides this label for an action status tip. Keep
		// both presentations on one compressible line so that swap cannot change
		// the status bar's height and move the workbench beneath it.
		StatusLabel->setWordWrap(false);
		StatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
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
		// [[BPR-0646]]. RFC-BPR-0028's strip: one 30-pixel row in full ink
		// with bordered buttons, three regions. It stays the window's one
		// toolbar so every action keeps its presenter, shortcut and place in
		// the declared focus order.
		Strip = new workbench_strip{ QStringLiteral("Review strip"), this };
		addToolBar(Strip);
		PrimaryToolbar = Strip;
		PrimaryToolbar->setObjectName(QStringLiteral("workbenchPrimaryToolbar"));
		PrimaryToolbar->setAccessibleName(QStringLiteral("Review strip"));
		PrimaryToolbar->setAccessibleDescription(QStringLiteral(
			"Drawer toggles, pull request identity, change and file navigation, "
			"the review modality, refresh, account and settings"));

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
		// [[BPR-0651]]. The repositories drawer, left of the files. Alt+Shift+3
		// to 5 are taken by the line numbers, review controls and review
		// details, so this one is mnemonic.
		Drawers[2] = makeDrawer(
			QStringLiteral("&Repositories drawer"),
			QStringLiteral("Repositories drawer"),
			QStringLiteral("Repositories drawer toggle"),
			QStringLiteral("toggleWorkbenchRepositoriesDrawerAction"),
			QStringLiteral("Hide the repositories drawer and give its width to the comparison."),
			QStringLiteral("Show the repositories drawer at its saved width."),
			QKeySequence{ QStringLiteral("Alt+Shift+R") });

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
		// [[BPR-0646]]. Off by default: the chrome above the comparison is
		// the strip and the epoch line; the exact anchors and the selected
		// change's details are shown on request.
		ReviewDetailsAction->setChecked(false);
		ReviewDetailsAction->setShortcut(
			QKeySequence{ QStringLiteral("Alt+Shift+3") });
		ReviewDetailsAction->setShortcutContext(Qt::WindowShortcut);
		ReviewDetailsAction->setToolTip(QStringLiteral(
			"Show the exact review anchors and the selected-change details."));
		ReviewDetailsAction->setStatusTip(ReviewDetailsAction->toolTip());
		addAction(ReviewDetailsAction);
		connect(ReviewDetailsAction, &QAction::toggled, this, [this](const bool) {
			apply_review_details_visibility();
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

		AccountAction = new QAction{ QStringLiteral("&Account"), this };
		AccountAction->setObjectName(QStringLiteral("workbenchAccountAction"));
		AccountAction->setIconText(QStringLiteral("Account"));
		addAction(AccountAction);
		connect(AccountAction, &QAction::triggered, this, [this] {
			// Signed out, the one way in is the device sign-in the header
			// shows; signed in, the account lives on its settings page.
			// Signed out or in, the account lives on its settings page; while
			// signed out the sign-in panel is that page's top and takes focus.
			open_settings(settings_page::accounts_and_hosts);
			if (DevicePanel != nullptr && DevicePanel->isVisibleTo(SettingsWindow)) {
				DevicePanel->begin_button()->setFocus(Qt::ShortcutFocusReason);
			}
		});

		const auto stripLabel = [this](const QString& aObjectName,
									   const QString& aAccessible,
									   const QString& aDescription) {
			auto* const label = new QLabel{ PrimaryToolbar };
			label->setObjectName(aObjectName);
			label->setAccessibleName(aAccessible);
			label->setAccessibleDescription(aDescription);
			label->setTextFormat(Qt::PlainText);
			label->setFocusPolicy(Qt::NoFocus);
			label->setContentsMargins(2, 0, 4, 0);
			return label;
		};
		const auto stripWidget = [this](QWidget* const aWidget, const QString& aActionName) {
			auto* const action = PrimaryToolbar->addWidget(aWidget);
			action->setObjectName(aActionName);
			return action;
		};
		const auto stripSpacer = [this, &stripWidget](
									 const QString& aObjectName, const QString& aActionName) {
			auto* const spacer = new QWidget{ PrimaryToolbar };
			spacer->setObjectName(aObjectName);
			spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
			spacer->setFocusPolicy(Qt::NoFocus);
			stripWidget(spacer, aActionName);
			return spacer;
		};

		// Exact persistent order, the design's three regions left to right.
		// Left: the files drawer toggle, the identity, a rule, the change
		// navigation with its position, the file navigation with its position.
		// Centre: the modality control alone between two expanding spacers.
		// Right: the secondary review controls, refresh (inserted once the
		// session chrome has made it), the comments drawer toggle, a rule,
		// account, settings.
		// The platform inset comes first: on macOS the width the three window
		// buttons occupy plus the gap Fork and Visual Studio Code leave before
		// their first control; nothing elsewhere. A widget item rather than a
		// margin, because the toolbar layout lays its items from its own edge.
		StripInset = new QWidget{ PrimaryToolbar };
		StripInset->setObjectName(QStringLiteral("workbenchStripInset"));
		StripInset->setFocusPolicy(Qt::NoFocus);
#if defined(__APPLE__)
		StripInset->setFixedWidth(traffic_light_inset);
#else
		StripInset->setFixedWidth(0);
#endif
		stripWidget(StripInset, QStringLiteral("workbenchStripInsetAction"));
		// [[BPR-0650]]. Home first: the way back to the landing and the queue
		// from any review, checked while Home shows.
		HomeAction = new QAction{ QStringLiteral("&Home"), this };
		HomeAction->setObjectName(QStringLiteral("workbenchHomeAction"));
		HomeAction->setCheckable(true);
		HomeAction->setShortcut(QKeySequence{ QStringLiteral("Ctrl+0") });
		HomeAction->setShortcutContext(Qt::WindowShortcut);
		HomeAction->setIconText(QStringLiteral("Home"));
		HomeAction->setToolTip(QStringLiteral(
			"Show Home: sign-in or the review queue, and the way back to the review."));
		HomeAction->setStatusTip(HomeAction->toolTip());
		addAction(HomeAction);
		connect(HomeAction, &QAction::triggered, this, [this](const bool aChecked) {
			choose_home(aChecked);
		});
		PrimaryToolbar->addAction(HomeAction);
		PrimaryToolbar->addAction(Drawers[2].Toggle);
		PrimaryToolbar->addAction(Drawers[0].Toggle);
		IdentityLabel = stripLabel(
			QStringLiteral("workbenchStripIdentity"),
			QStringLiteral("Open pull request"),
			QStringLiteral("Repository and pull request the strip navigates"));
		IdentityLabel->setContentsMargins(4, 0, 4, 0);
		IdentityAction = stripWidget(
			IdentityLabel, QStringLiteral("workbenchStripIdentityAction"));
		stripWidget(
			new strip_divider{ PrimaryToolbar },
			QStringLiteral("workbenchStripLeftDividerAction"));
		PrimaryToolbar->addAction(Navigation[2].Action);
		PrimaryToolbar->addAction(Navigation[3].Action);
		ChangePositionLabel = stripLabel(
			QStringLiteral("workbenchStripChangePosition"),
			QStringLiteral("Change position"),
			QStringLiteral("The selected change's position among the comparison's changes"));
		ChangePositionAction = stripWidget(
			ChangePositionLabel,
			QStringLiteral("workbenchStripChangePositionAction"));
		PrimaryToolbar->addAction(Navigation[0].Action);
		PrimaryToolbar->addAction(Navigation[1].Action);
		FilePositionLabel = stripLabel(
			QStringLiteral("workbenchStripFilePosition"),
			QStringLiteral("File position"),
			QStringLiteral("The selected file's position among the actionable files"));
		FilePositionAction = stripWidget(
			FilePositionLabel, QStringLiteral("workbenchStripFilePositionAction"));
		// [[BPR-0656]]. Next new comment with its count: the unread threads
		// the ledger host decided, walked in file order.
		NextNewCommentAction = new QAction{ QStringLiteral("Next new &comment"), this };
		NextNewCommentAction->setObjectName(QStringLiteral("nextNewCommentAction"));
		NextNewCommentAction->setIconText(QStringLiteral("Next new comment"));
		NextNewCommentAction->setToolTip(QStringLiteral("Next new comment"));
		NextNewCommentAction->setStatusTip(QStringLiteral(
			"Reveal the next thread with a post newer than what you have read."));
		NextNewCommentAction->setShortcut(QKeySequence{ QStringLiteral("Alt+N") });
		NextNewCommentAction->setShortcutContext(Qt::WindowShortcut);
		NextNewCommentAction->setEnabled(false);
		addAction(NextNewCommentAction);
		connect(NextNewCommentAction, &QAction::triggered, this, [this] { step_to_next_new_comment(); });
		PrimaryToolbar->addAction(NextNewCommentAction);
		NewCommentCountLabel = stripLabel(
			QStringLiteral("workbenchStripNewCommentCount"),
			QStringLiteral("New comments"),
			QStringLiteral("How many threads carry a post newer than what you have read"));
		NewCommentCountAction = stripWidget(
			NewCommentCountLabel, QStringLiteral("workbenchStripNewCommentCountAction"));

		ToolbarSpacer = stripSpacer(
			QStringLiteral("workbenchNavigationSpacer"),
			QStringLiteral("workbenchNavigationSpacerAction"));
		Modality = new modality_control{ *Strip, PrimaryToolbar };
		Modality->set_dispatch(
			[this](const review_modality aModality) { choose_modality(aModality); });
		stripWidget(Modality, QStringLiteral("workbenchModalityAction"));
		ModalitySpacer = stripSpacer(
			QStringLiteral("workbenchModalitySpacer"),
			QStringLiteral("workbenchModalitySpacerAction"));

		PrimaryToolbar->addAction(ReviewDetailsAction);
		stripWidget(ReviewControlsButton, QStringLiteral("workbenchReviewControlsAction"));
		PrimaryToolbar->addAction(Drawers[1].Toggle);
		stripWidget(
			new strip_divider{ PrimaryToolbar },
			QStringLiteral("workbenchStripRightDividerAction"));
		PrimaryToolbar->addAction(AccountAction);
		PrimaryToolbar->addAction(SettingsAction);
		update_strip_positions();
		update_strip_identity();
		apply_strip_width();
	}

	void workbench_window::build_central_surface()
	{
		CentralSplitter = new workbench_central_splitter{ this };
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
		// [[BPR-0646]]. Home's landing, above the epoch line while there is
		// nothing to compare and no pull request: it funnels a reviewer to
		// the account or to a pull request rather than showing empty panes.
		// [[BPR-0646]] [[BPR-0650]]. Home is the whole centre while nothing
		// is open: the landing card and the review queue, and none of the
		// review chrome that means nothing before a pull request is chosen.
		HomePage = new QWidget;
		HomePage->setObjectName(QStringLiteral("workbenchHome"));
		HomePage->setAccessibleName(QStringLiteral("Home"));
		HomePage->setAccessibleDescription(QStringLiteral(
			"The landing while nothing is open: sign in or choose a pull "
			"request, and the local review queue"));
		auto* const home = new QVBoxLayout{ HomePage };
		home->setContentsMargins(0, 0, 0, 0);
		home->setSpacing(0);
		HomeColumn  = home;
		HomeLanding = new workbench_landing{ HomePage };
		HomeLanding->set_primary_dispatch([this] {
			if (HomeLanding->state() == landing_state::back_to_review) {
				choose_home(false);
				return;
			}
			if (HomeLanding->state() == landing_state::sign_in) {
				open_settings(settings_page::accounts_and_hosts);
				if (DevicePanel != nullptr) {
					DevicePanel->begin_button()->setFocus(Qt::OtherFocusReason);
				}
				return;
			}
			if (QueueView != nullptr) {
				QueueView->setFocus(Qt::OtherFocusReason);
			}
		});
		HomeLanding->set_secondary_dispatch([this] {
			if (HomeLanding->state() == landing_state::back_to_review) {
				if (QueueView != nullptr) {
					QueueView->setFocus(Qt::OtherFocusReason);
				}
				return;
			}
			open_settings(settings_page::accounts_and_hosts);
			if (HomeLanding->state() == landing_state::choose_pull_request
				&& SessionPanel != nullptr) {
				SessionPanel->owner_editor()->setFocus(Qt::OtherFocusReason);
			}
		});
		home->addWidget(HomeLanding);
		centre->addWidget(EpochSelectors);
		// [[BPR-0647]]. Beneath the epoch line the centre is a stack: the
		// overview for the Overview modality, the comparison for the rest.
		// The comparison keeps a page of its own with no margins, so every
		// geometry the probes pin on it is what it was.
		CentreStack = new QStackedWidget{ CentreSurface };
		CentreStack->setObjectName(QStringLiteral("workbenchCentreStack"));
		ComparisonPage = new QWidget{ CentreStack };
		ComparisonPage->setObjectName(QStringLiteral("workbenchComparisonPage"));
		auto* const comparisonColumn = new QVBoxLayout{ ComparisonPage };
		comparisonColumn->setContentsMargins(0, 0, 0, 0);
		comparisonColumn->setSpacing(0);
		comparisonColumn->addWidget(ComparisonSurface, 1);
		OverviewPage = new pull_request_overview_page{ CentreStack };
		OverviewPage->set_entry_dispatch([this](const QString& aPath) { enter_read_at(aPath); });
		OverviewPage->set_file_dispatch([this](const QString& aPath) { enter_read_at(aPath); });
		CentreStack->addWidget(ComparisonPage);
		CentreStack->addWidget(OverviewPage);
		CentreStack->setCurrentWidget(ComparisonPage);
		centre->addWidget(CentreStack, 1);

		Drawers[0].Panel = new workbench_drawer_panel{ core::workbench_drawer::files };
		Drawers[1].Panel = new workbench_drawer_panel{
			core::workbench_drawer::conversations
		};
		// [[BPR-0651]]. The repositories tree: each repository the queue names
		// with its pull requests of interest; choosing one opens it on its
		// overview through the same intent the locator sends.
		Drawers[2].Panel = new workbench_drawer_panel{
			core::workbench_drawer::repositories
		};
		RepositoriesTree = new repositories_tree_view{ Drawers[2].Panel };
		RepositoriesTree->set_saved_repository_dispatch(
			[this](application::saved_repository_intent aIntent) {
				if (SavedRepositoryDispatch && QueueReady) {
					SavedRepositoryDispatch(std::move(aIntent));
				}
			});
		RepositoriesTree->set_open_dispatch([this](const core::queue_item& aItem) {
			open_pull_request_item(aItem);
		});
		Drawers[2].Panel->set_content_widget(RepositoriesTree, RepositoriesTree);
		// [[BPR-0646]]. A rail is the collapsed drawer's one control, and
		// pressing it is the same gesture as the strip's toggle: the action
		// is triggered, and the core decides.
		for (std::size_t index = 0U; index < Drawers.size(); ++index) {
			connect(Drawers[index].Panel->rail(),
					&QAbstractButton::clicked,
					this,
					[this, index] { Drawers[index].Toggle->trigger(); });
		}

		// Route 2 of BPR-0191. The conversations drawer shows RFC-BPR-0002's
		// threaded review conversation, which is a forest and cannot be a list,
		// so the drawer keeps its collapse, width and toggle behaviour and hosts
		// the tree instead of its own list model. The files drawer is unchanged:
		// a file list is a list.
		ConversationPanel = new github_conversation_panel{ Drawers[1].Panel };
		ConversationPanel->set_current_endpoint_placement(false, false);
		// [[BPR-0648]]. The cards read one geometry from the comparison: where
		// a line is. The window relays it, and relays every applied scroll
		// frame, so the cards sit at their lines and follow the shared scroll
		// without the drawer ever reading the panes itself.
		ConversationPanel->cards()->set_line_locator(
			[this](const core::diff_side aSide, const std::uint32_t aOneBasedLine)
				-> std::optional<int> {
				if (ComparisonSurface == nullptr || aOneBasedLine == 0U
					|| ConversationPanel == nullptr) {
					return std::nullopt;
				}
				const auto rect = ComparisonSurface->line_rect(aSide, aOneBasedLine - 1U);
				if (rect.isEmpty()) {
					return std::nullopt;
				}
				const auto inWindow = ComparisonSurface->mapTo(this, rect.topLeft());
				return ConversationPanel->cards()->viewport()->mapFrom(this, inWindow).y();
			});
		ComparisonSurface->set_frame_observer([this] { refresh_cards(); });
		// [[BPR-0661]]. A file marks itself read once fully seen only when the
		// preference says so; the shared scrollbar is what says "fully".
		if (auto* const scrollbar = ComparisonSurface->shared_scrollbar(); scrollbar != nullptr) {
			connect(scrollbar, &QScrollBar::valueChanged, this, [this](int) { consider_mark_read_when_fully_seen(); });
			connect(scrollbar, &QScrollBar::rangeChanged, this, [this](int, int) { consider_mark_read_when_fully_seen(); });
		}
		ConversationPanel->cards()->set_action_dispatch(
			[this](const card_action aAction, const std::string& aIdentity) {
				switch (aAction) {
				case card_action::comment_here: {
					// A comment on the thread's line: the thread becomes
					// current, which reveals its line, the line is selected as
					// a click would, and the draft editor takes focus.
					const auto index = ConversationPanel->model()->index_for_id(aIdentity);
					if (index.isValid()) {
						ConversationPanel->view()->setCurrentIndex(index);
					}
					static_cast<void>(select_thread_line(aIdentity));
					if (ReviewEditor != nullptr) {
						ReviewEditor->body_editor()->setFocus(Qt::OtherFocusReason);
					}
					break;
				}
				case card_action::publish_draft:
					if (ReviewEditor != nullptr) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::edit_draft:
					if (ReviewEditor != nullptr) {
						ReviewEditor->body_editor()->setFocus(Qt::OtherFocusReason);
					}
					break;
				case card_action::delete_draft:
					if (ReviewEditor != nullptr) {
						ReviewEditor->discard_action()->trigger();
					}
					break;
				case card_action::mark_read:
					// [[BPR-0656]]. The ledger host advances the cursor and
					// the read state comes back through present_read_state.
					if (ConversationReadDispatch) {
						ConversationReadDispatch(aIdentity);
					}
					break;
				case card_action::resolve:
				case card_action::unresolve: {
					// [[BPR-0657]]. What the window knows travels with the
					// gesture: the thread's state as projected and the server's
					// capability for the direction asked; the composition adds
					// what only it knows and decides.
					if (!ThreadResolutionDispatch) {
						break;
					}
					const auto resolve  = aAction == card_action::resolve;
					auto       resolved = false;
					if (const auto index = ConversationPanel->model()->index_for_id(aIdentity); index.isValid()) {
						resolved = index.data(github_conversation_model::resolved_role).toBool();
					}
					ThreadResolutionDispatch(thread_resolution_gesture{
						.Identity   = aIdentity,
						.Resolve    = resolve,
						.Resolved   = resolved,
						.Capability = resolve ? Capabilities.ResolveThread : Capabilities.UnresolveThread,
					});
					break;
				}
				case card_action::reply: {
					// [[BPR-0649]], [[BPR-0657]]. The feed's Reply: the thread
					// is named to the composition first, so the draft that
					// follows the selection answers it; then the thread becomes
					// current and the draft editor takes focus.
					if (ReviewReplyDispatch) {
						ReviewReplyDispatch(aIdentity);
					}
					const auto index = ConversationPanel->model()->index_for_id(aIdentity);
					if (index.isValid()) {
						ConversationPanel->view()->setCurrentIndex(index);
					}
					static_cast<void>(select_thread_line(aIdentity));
					if (ReviewEditor != nullptr) {
						ReviewEditor->body_editor()->setFocus(Qt::OtherFocusReason);
					}
					break;
				}
				case card_action::open_in_code: {
					// [[BPR-0649]]. Back to Read at the thread's line: the
					// file the thread names is selected through the same
					// intent the files drawer sends, Read is entered, and the
					// thread made current so the line is revealed.
					const auto index = ConversationPanel->model()->index_for_id(aIdentity);
					if (index.isValid()) {
						enter_read_at(index.data(github_conversation_model::path_role).toString());
						ConversationPanel->view()->setCurrentIndex(index);
					}
					break;
				}
				}
			});
		ConversationPanel->cards()->set_excerpt_locator(
			[this](const QString& aPath, const core::diff_side aSide, const std::uint32_t aLine)
				-> std::optional<QString> {
				// Only the compared file's text is on hand: one line each
				// side of the thread's, from the pane on the thread's side.
				if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr
					|| !ComparisonSurface->current_context().has_value()
					|| ComparisonSurface->current_context()->Key.File != core::file_identity_of(aPath.toStdString())) {
					return std::nullopt;
				}
				auto* const pane = aSide == core::diff_side::left ? ComparisonSurface->left_pane()
																  : ComparisonSurface->right_pane();
				if (pane == nullptr || pane->document() == nullptr) {
					return std::nullopt;
				}
				auto lines = QStringList{ };
				for (auto number = static_cast<int>(aLine) - 2; number <= static_cast<int>(aLine); ++number) {
					if (number < 0) {
						continue;
					}
					const auto block = pane->document()->findBlockByNumber(number);
					if (block.isValid()) {
						lines.push_back(block.text());
					}
				}
				return lines.isEmpty() ? std::nullopt : std::optional{ lines.join(QLatin1Char{ '\n' }) };
			});
		ComparisonSurface->set_selection_observer(
			[this](const std::optional<core::semantic_selection>& aSelection,
				   const bool                                     aInteractive) {
				if (ConversationPanel == nullptr || ComparisonSurface == nullptr) {
					return;
				}
				const auto endpointIsCurrent = aSelection.has_value()
					&& (aSelection->PreferredLine.Side == core::diff_side::left
							? LeftConversationEndpointCurrent
							: RightConversationEndpointCurrent);
				const auto line            = aInteractive && aSelection.has_value()
						&& aSelection->Kind == core::semantic_selection_kind::line
						&& endpointIsCurrent
					? std::optional<core::source_line_ref>{ aSelection->PreferredLine }
					: std::nullopt;
				ApplyingConversationAnchor = true;
				ConversationPanel->set_anchor_file(conversation_anchor_file());
				ConversationPanel->set_anchor_line(line);
				refresh_cards();
				if (aInteractive && !line.has_value()) {
					ConversationPanel->view()->setCurrentIndex({ });
					ConversationPanel->view()->selectionModel()->clearSelection();
				}
				ApplyingConversationAnchor = false;
				if (aInteractive && !line.has_value()) {
					update_defect_surface();
				}
				update_strip_positions();
				update_home_landing();
			});
		ReviewSubmissionEditor = new review_submission_editor{
			[this](review_submission_editor_intent aIntent) {
				if (ReviewSubmissionDispatch) {
					ReviewSubmissionDispatch(std::move(aIntent));
				}
			},
			Drawers[1].Panel
		};
		ReviewEditor = new review_draft_editor{
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
		conversationLayout->addWidget(ReviewSubmissionEditor, 0, Qt::AlignLeft);
		conversationLayout->addWidget(ReviewEditor);
		AgentPanel = new agent_launch_panel{ { }, conversationContents };
		AgentPanel->setMaximumHeight(300);
		conversationLayout->addWidget(AgentPanel);
		// BPR-0513. RFC-BPR-0012's two agent surfaces go where the launcher
		// that produces their content already is, in the order the work runs:
		// ask the agent, watch the chat, read what it found, and promote a
		// finding into the draft editor above — whose comment workshop is the
		// view a promotion opens, and which is already in this drawer.
		//
		// Placement only. Neither panel is given a dispatch here, so a window
		// nobody has composed emits nothing from either, and neither gains a
		// member, an action, or a field by being laid out.
		SideChat = new side_chat_panel{ { }, conversationContents };
		conversationLayout->addWidget(SideChat);
		// Stacked, as the finding queue's actions are: the drawer never scrolls
		// sideways, so a row wider than the drawer loses its last actions rather
		// than pushing them offscreen (BPR-0506).
		auto* const chatActions = new QVBoxLayout{ };
		for (auto* const action : { SideChat->send_action(),
									SideChat->add_context_action(),
									SideChat->new_session_action(),
									SideChat->continue_old_revision_action() }) {
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			chatActions->addWidget(button);
		}
		conversationLayout->addLayout(chatActions);
		FindingQueue = new finding_queue_view{ { }, conversationContents };
		conversationLayout->addWidget(FindingQueue);
		// One button per declared action, indexed by the enumerator, so a
		// seventh finding action cannot be declared and then never offered.
		//
		// BPR-0506. Stacked rather than in a row. Six buttons whose longest
		// label is *Convert to local draft comment* need more width than any
		// drawer a reviewer is likely to give this column, and the drawer
		// deliberately does not scroll sideways, so a row put the last two
		// actions somewhere no gesture reaches. A column cannot clip: each
		// button is as wide as its own label and the drawer already scrolls
		// down. The six actions, their order, their labels, and the single
		// intent each emits are untouched - this is where they are drawn, not
		// what they are.
		auto* const findingActions = new QVBoxLayout{ };
		for (auto index = std::size_t{ 0U }; index < finding_queue_action_count;
			 ++index) {
			auto* const action = FindingQueue->action(
				static_cast<finding_queue_action>(index));
			if (action == nullptr) {
				continue;
			}
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			findingActions->addWidget(button);
		}
		conversationLayout->addLayout(findingActions);
		// BPR-0520. Stacked, for the reason BPR-0506 stacked the finding
		// actions, and now on a measurement rather than on a suspicion. At the
		// drawer's default width the column's visible region was 522 px wide
		// while these two rows asked for 621, and the drawer scrolls down but
		// never sideways. *Next defect* began 17 px past the right edge and so
		// was not offscreen but absent; *Previous defect* lost its last
		// pixels; and one row below, *Apply local classification* lost two
		// thirds of itself. A column cannot clip: each button is as wide as
		// its own label, and the drawer already scrolls down.
		//
		// The count label keeps its place between the classify action and the
		// pair that walks the queue, and loses only the horizontal stretch
		// that used to push that pair to the far edge - a column has nothing
		// to push against.
		auto* const defectNavigation = new QVBoxLayout{ };
		auto* const classifyButton   = new QToolButton{ conversationContents };
		classifyButton->setDefaultAction(ClassifyDefect);
		defectNavigation->addWidget(classifyButton);
		DefectCountLabel = new QLabel{ QStringLiteral("Defects 0/0"), conversationContents };
		DefectCountLabel->setObjectName(QStringLiteral("defectCount"));
		DefectCountLabel->setAccessibleName(QStringLiteral("Defect queue position"));
		defectNavigation->addWidget(DefectCountLabel);
		for (auto* const action : { PreviousDefect, NextDefect }) {
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			defectNavigation->addWidget(button);
		}
		conversationLayout->addLayout(defectNavigation);
		conversationLayout->addWidget(DefectCard);
		// BPR-0520, the second half of the same measurement. The five verdicts
		// RFC-BPR-0009 and RFC-BPR-0010 give a defect are the same five, in the
		// same order, under the same labels and object names, each emitting the
		// single intent it emitted before. This is where they are drawn, not
		// what they are.
		auto* const defectActions = new QVBoxLayout{ };
		for (auto* const action : { DefectCard->verify_action(),
									DefectCard->reopen_action(), DefectCard->withdraw_action(),
									DefectCard->accept_as_is_action(), DefectCard->reclassify_action() }) {
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			defectActions->addWidget(button);
		}
		conversationLayout->addLayout(defectActions);
		// BPR-0513. The column now stacks the conversation tree, the draft
		// editor with its comment workshop, the agent launcher, the side chat,
		// and the finding queue. That is more than a short drawer can show at
		// once, and a column that simply squeezed would show each panel with
		// its own fields overlapping — which is showing a reviewer nothing.
		// So the content scrolls. Collapse, width reporting, and the toggle
		// are untouched: this is inside the drawer's content, not around it.
		auto* const conversationScroll = new QScrollArea{ Drawers[1].Panel };
		conversationScroll->setObjectName(
			QStringLiteral("workbenchConversationDrawerScroll"));
		conversationScroll->setWidgetResizable(true);
		conversationScroll->setFrameShape(QFrame::NoFrame);
		// BPR-0527. The drawer opens at the width stated above rather than at
		// whatever the widest row asked for.
		conversationScroll->setMinimumWidth(conversation_drawer_default_width);
		// Vertically only. A drawer is a column, and a column that also
		// scrolled sideways would hide a control behind a gesture nobody
		// makes; the width a panel gets is still the width the reviewer gave
		// the drawer, exactly as it was before this content scrolled at all.
		conversationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		conversationScroll->setWidget(conversationContents);
		Drawers[1].Panel->set_content_widget(
			conversationScroll, ConversationPanel->view());
		CentralSplitter->addWidget(Drawers[2].Panel);
		CentralSplitter->addWidget(Drawers[0].Panel);
		CentralSplitter->addWidget(CentreSurface);
		CentralSplitter->addWidget(Drawers[1].Panel);
		// [[BPR-0646]]. The queue landing leaves the review header, which is no
		// longer a scroll area, and sits under the file list until the
		// repositories drawer of RFC-BPR-0028 takes it over ([[BPR-0651]]).
		// The review queue is how a pull request is chosen, so it is Home's
		// list, under the landing card, with the room the drawer could not
		// give it. The files drawer holds files.
		if (auto* const homeColumn = qobject_cast<QVBoxLayout*>(HomePage->layout());
			homeColumn != nullptr) {
			// [[BPR-0650]]. Open by link or owner/name#number, in the bar
			// above the list: the same intent the locator sends, and the
			// pull request arrives on Overview.
			HomeOpenBar = new QLineEdit{ HomePage };
			HomeOpenBar->setObjectName(QStringLiteral("homeOpenBar"));
			HomeOpenBar->setAccessibleName(QStringLiteral("Open a pull request"));
			HomeOpenBar->setAccessibleDescription(QStringLiteral(
				"Type a pull request link or owner/name#number and press Return to open it on its overview"));
			HomeOpenBar->setPlaceholderText(QStringLiteral("Open by link or owner/name#number"));
			HomeOpenBar->setClearButtonEnabled(true);
			auto* const openRow = new QHBoxLayout{ };
			openRow->setContentsMargins(24, 8, 24, 0);
			openRow->addWidget(HomeOpenBar, 1);
			homeColumn->addLayout(openRow);
			connect(HomeOpenBar, &QLineEdit::returnPressed, this, [this] { open_from_home_bar(); });
			auto* const queueHeading = new QLabel{ QStringLiteral("Review queue"), HomePage };
			queueHeading->setObjectName(QStringLiteral("workbenchHomeQueueHeading"));
			queueHeading->setContentsMargins(24, 12, 24, 4);
			auto headingFont = queueHeading->font();
			headingFont.setWeight(QFont::DemiBold);
			queueHeading->setFont(headingFont);
			homeColumn->addWidget(queueHeading);
			build_queue_landing(HomePage, *homeColumn);
			if (auto* const landing = HomePage->findChild<QWidget*>(
					QStringLiteral("reviewQueueLanding"), Qt::FindDirectChildrenOnly);
				landing != nullptr) {
				landing->setMaximumHeight(QWIDGETSIZE_MAX);
				landing->setContentsMargins(24, 0, 24, 12);
				homeColumn->setStretchFactor(landing, 1);
			}
			// The list had the drawer's 84-pixel bound; Home gives it the room.
			if (QueueView != nullptr) {
				QueueView->setMaximumHeight(QWIDGETSIZE_MAX);
			}
		}
		// Only the centre absorbs a window resize, so a drawer keeps the width the
		// host persisted until the user moves the handle.
		CentralSplitter->setStretchFactor(0, 0);
		CentralSplitter->setStretchFactor(1, 0);
		CentralSplitter->setStretchFactor(2, 1);
		CentralSplitter->setStretchFactor(3, 0);

		// [[BPR-0662]]. A width leaves for the host only from a handle drag.
		// Whatever the splitter is given by a resize -- the window, the
		// stack showing it, a hidden page laid out at its default size -- is
		// a squeeze the splitter itself undoes once there is room, and it is
		// never written back.
		connect(CentralSplitter, &QSplitter::splitterMoved, this,
				[this](int, int) { observe_handle_drag(); });
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
			[this](const QModelIndex& aCurrent) {
				if (!ApplyingConversationAnchor) {
					synchronize_selected_conversation(aCurrent);
				}
				update_defect_surface();
			});
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

		// [[BPR-0526]]. A profile has two ways in -- the token prompt the
		// Connect button opens, and a code read aloud into a browser -- and a
		// reviewer looking for either looks in one place. So the device panel
		// is mounted directly under the session chrome rather than behind a
		// drawer or a dialog of its own, in the same column, under the same
		// scroll bound.
		//
		// The window supplies no dispatch. Every gesture leaves through
		// `DeviceDispatch`, which the composition sets, so a panel nobody wired
		// stays visible and inert instead of quietly performing a host effect
		// from the presentation layer.
		DevicePanel = new device_authorization_panel{
			SessionPanel->selected_profile().value_or(core::host_profile_id{ }),
			[this](application::device_authorization_gesture aGesture) {
				if (DeviceDispatch) {
					DeviceDispatch(std::move(aGesture));
				}
			},
			nullptr,
		};

		// [[BPR-0646]]. RFC-BPR-0028 AC-01: the chrome above the comparison
		// is the strip and the epoch line only, and accounts live in Settings.
		// The device sign-in and the session chrome are the Accounts and
		// hosts page of the Settings window: sign-in first while signed out,
		// then the host picker, Connect and the locator. The strip's account
		// button opens that page, and a flow the core begins presents it, so
		// the code is seen. The pull request list (the queue landing) and the
		// pull request summary stay in the files drawer's bounded top region
		// until the repositories drawer (BPR-0651) and the overview
		// (BPR-0647) take them.
		SettingsWindow       = new settings_window{ this };
		auto* const accounts = new QWidget;
		accounts->setObjectName(QStringLiteral("workbenchAccountsPage"));
		accounts->setAccessibleName(QStringLiteral("Accounts and hosts"));
		auto* const accountsColumn = new QVBoxLayout{ accounts };
		accountsColumn->setContentsMargins(0, 0, 0, 0);
		// [[BPR-0652]]. The account card first: who is signed in, on what
		// host, connected or not, and the rate limit. Then the device sign-in
		// and the session chrome as BPR-0646 placed them.
		AccountCard = new account_card{ accounts };
		// [[BPR-0660]]. The card's Sign out is the same deactivation the session
		// panel's is: the host forgets the remembered grant and signs out.
		connect(AccountCard->sign_out_button(), &QToolButton::clicked, this, [this] {
			if (GithubDispatch) {
				GithubDispatch(core::deactivate_github_identity_intent{ });
			}
		});
		accountsColumn->addWidget(AccountCard);
		accountsColumn->addWidget(DevicePanel);
		accountsColumn->addWidget(SessionPanel);
		// [[BPR-0660]]. The enterprise servers the reviewer keeps, so a
		// GitHub Enterprise host is offered at the next launch without
		// being typed again. No token is kept here: signing in is still
		// the device flow on the host picked above.
		auto* const serversHeading = new QLabel{ QStringLiteral("Enterprise servers"), accounts };
		serversHeading->setObjectName(QStringLiteral("accountsServersHeading"));
		auto serversHeadingFont = serversHeading->font();
		serversHeadingFont.setWeight(QFont::DemiBold);
		serversHeading->setFont(serversHeadingFont);
		accountsColumn->addWidget(serversHeading);
		build_enterprise_servers_editor(accounts, *accountsColumn);
		SettingsWindow->adopt_panel(settings_page::accounts_and_hosts, accounts);
		// [[BPR-0652]]. Reviews & queue: the observer rules, and a plain
		// statement of what the page will hold once its authorities exist.
		auto* const reviews = new QWidget;
		reviews->setObjectName(QStringLiteral("workbenchReviewsPage"));
		reviews->setAccessibleName(QStringLiteral("Reviews and queue"));
		auto* const reviewsColumn = new QVBoxLayout{ reviews };
		reviewsColumn->setContentsMargins(0, 0, 0, 0);
		auto* const reviewsSubtitle = new QLabel{
			QStringLiteral("Local behaviour only. Nothing here changes assignments or subscriptions on GitHub."),
			reviews
		};
		reviewsSubtitle->setObjectName(QStringLiteral("reviewsSubtitle"));
		reviewsSubtitle->setWordWrap(true);
		reviewsColumn->addWidget(reviewsSubtitle);
		build_behaviour_editor(reviews, reviewsColumn);
		auto* const rulesHeading = new QLabel{ QStringLiteral("Observer rules"), reviews };
		rulesHeading->setObjectName(QStringLiteral("reviewsRulesHeading"));
		auto headingFont = rulesHeading->font();
		headingFont.setWeight(QFont::DemiBold);
		rulesHeading->setFont(headingFont);
		reviewsColumn->addWidget(rulesHeading);
		if (RulesEditor != nullptr) {
			RulesEditor->setParent(reviews);
			reviewsColumn->addWidget(RulesEditor);
		}
		// [[BPR-0659]]. Saved repositories: the ones the drawer keeps, per
		// the account the queue session names.
		auto* const savedHeading = new QLabel{ QStringLiteral("Saved repositories"), reviews };
		savedHeading->setObjectName(QStringLiteral("reviewsSavedHeading"));
		savedHeading->setFont(headingFont);
		reviewsColumn->addWidget(savedHeading);
		build_saved_repositories_editor(reviews, *reviewsColumn);
		auto* const reviewsNotice = new QLabel{
			QStringLiteral("Modality rules, reading behaviour and the default comparison arrive with "
						   "BPR-0661."),
			reviews
		};
		reviewsNotice->setObjectName(QStringLiteral("reviewsNotice"));
		reviewsNotice->setWordWrap(true);
		reviewsColumn->addWidget(reviewsNotice);
		SettingsWindow->adopt_panel(settings_page::reviews_and_queue, reviews);

		auto* const filesColumn
			= qobject_cast<QVBoxLayout*>(Drawers[0].Panel->layout());
		// [[BPR-0658]]. The files tree over the flat list: the list stays
		// the drawer's authority (the composition's courses select in it and
		// its selection dispatches the intent); the tree is what is seen, and
		// a row chosen in it becomes the list's selection.
		{
			auto* const filesSurface = new QWidget;
			filesSurface->setObjectName(QStringLiteral("workbenchFilesSurface"));
			auto* const surfaceColumn = new QVBoxLayout{ filesSurface };
			surfaceColumn->setContentsMargins(0, 0, 0, 0);
			surfaceColumn->setSpacing(2);
			auto* const controls = new QHBoxLayout{ };
			controls->setContentsMargins(4, 0, 4, 0);
			FilesOrderSelector = new QComboBox{ filesSurface };
			FilesOrderSelector->setObjectName(QStringLiteral("filesOrderSelector"));
			FilesOrderSelector->setAccessibleName(QStringLiteral("Files order"));
			FilesOrderSelector->setAccessibleDescription(QStringLiteral("Tree, flat, or the files changed since my last review first"));
			FilesOrderSelector->addItem(QStringLiteral("Tree"), static_cast<int>(files_order::tree));
			FilesOrderSelector->addItem(QStringLiteral("Flat"), static_cast<int>(files_order::flat));
			FilesOrderSelector->addItem(QStringLiteral("Changed first"), static_cast<int>(files_order::changed_first));
			controls->addWidget(FilesOrderSelector);
			FilesFilter = new QLineEdit{ filesSurface };
			FilesFilter->setObjectName(QStringLiteral("filesFilter"));
			FilesFilter->setAccessibleName(QStringLiteral("Filter files"));
			FilesFilter->setAccessibleDescription(QStringLiteral("Show only files whose path contains this; their folders stay visible"));
			FilesFilter->setPlaceholderText(QStringLiteral("Filter"));
			FilesFilter->setClearButtonEnabled(true);
			controls->addWidget(FilesFilter, 1);
			surfaceColumn->addLayout(controls);
			FilesTree = new files_tree_view{ filesSurface };
			surfaceColumn->addWidget(FilesTree, 1);
			FilesTree->set_select_dispatch([this](const core::workbench_file_id& aFile) {
				// The list's selection is the one that dispatches.
				if (ApplyingFileSelection || Drawers[0].Panel->contents() == nullptr
					|| Drawers[0].Panel->contents_view() == nullptr) {
					return;
				}
				const auto index = Drawers[0].Panel->contents()->index_for_file(aFile);
				if (index.isValid()) {
					Drawers[0].Panel->contents_view()->setCurrentIndex(index);
				}
			});
			connect(FilesOrderSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
				FilesTree->set_order(static_cast<files_order>(FilesOrderSelector->currentData().toInt()));
			});
			connect(FilesFilter, &QLineEdit::textChanged, this, [this](const QString& aText) {
				FilesTree->set_filter(aText);
			});
			Drawers[0].Panel->host_beside_list(filesSurface, FilesTree);
		}
		auto* const top = new QWidget;
		top->setObjectName(QStringLiteral("workbenchDrawerTop"));
		top->setAccessibleName(QStringLiteral("Pull request"));
		auto* const topColumn = new QVBoxLayout{ top };
		topColumn->setContentsMargins(0, 0, 0, 0);
		DrawerTopColumn = topColumn;
		DrawerTopScroll = new QScrollArea{ Drawers[0].Panel };
		DrawerTopScroll->setObjectName(QStringLiteral("workbenchDrawerTopScroll"));
		DrawerTopScroll->setWidgetResizable(true);
		DrawerTopScroll->setFrameShape(QFrame::NoFrame);
		DrawerTopScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		DrawerTopScroll->setFocusPolicy(Qt::NoFocus);
		DrawerTopScroll->setMaximumHeight(420);
		DrawerTopScroll->setWidget(top);
		filesColumn->insertWidget(1, DrawerTopScroll);
		connect(
			SessionPanel->profile_selector(),
			&QComboBox::currentIndexChanged,
			this,
			[this] { rebind_device_authorization_profile(); });
		// Switching to sign-in is offered from the ring and from every
		// refusal, and both go to the one entry point already mounted here:
		// the device panel's "Get a code", for the profile the picker names.
		// The window adds no second way in, so sign-in cannot begin against a
		// host the reviewer is not looking at.
		SessionPanel->set_public_preview_dispatch(
			[this](application::public_preview_gesture aGesture) {
				static_cast<void>(aGesture);
				if (DevicePanel == nullptr) {
					return;
				}
				rebind_device_authorization_profile();
				DevicePanel->begin_button()->click();
			});

		// The empty state is not decoration. Without it a reviewer with no pull
		// request open sees a comparison of nothing and no indication of what to
		// do, which is what the shell shipped for exactly one commit before this
		// restored it. The session chrome stays visible either way, because
		// connecting is the action the empty state is asking for.
		PullRequestStack = new QStackedWidget{ top };
		PullRequestStack->setObjectName(QStringLiteral("githubViewState"));
		auto* const emptyState = new QWidget{ PullRequestStack };
		emptyState->setObjectName(QStringLiteral("githubEmptyState"));
		auto* const emptyLayout = new QVBoxLayout{ emptyState };
		EmptyStateTitleLabel    = new QLabel{ emptyState };
		EmptyStateTitleLabel->setObjectName(QStringLiteral("githubEmptyStateTitle"));
		EmptyStateTitleLabel->setAccessibleDescription(QStringLiteral("GitHub view state"));
		EmptyStateTitleLabel->setAlignment(Qt::AlignCenter);
		EmptyStateTitleLabel->setWordWrap(true);
		detail::set_plain_text_label(EmptyStateTitleLabel);
		EmptyStateTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		EmptyStateBodyLabel = new QLabel{ emptyState };
		EmptyStateBodyLabel->setObjectName(QStringLiteral("githubEmptyStateBody"));
		EmptyStateBodyLabel->setAccessibleDescription(QStringLiteral("GitHub view guidance"));
		EmptyStateBodyLabel->setAlignment(Qt::AlignCenter);
		EmptyStateBodyLabel->setWordWrap(true);
		detail::set_plain_text_label(EmptyStateBodyLabel);
		EmptyStateBodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		emptyLayout->addStretch(1);
		emptyLayout->addWidget(EmptyStateTitleLabel);
		emptyLayout->addWidget(EmptyStateBodyLabel);
		auto* const copyMessage = new QToolButton{ emptyState };
		copyMessage->setObjectName(QStringLiteral("copyGithubMessageButton"));
		copyMessage->setText(QStringLiteral("Copy message"));
		copyMessage->setAccessibleName(QStringLiteral("Copy GitHub message"));
		connect(copyMessage, &QToolButton::clicked, this, [this] {
			if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(EmptyStateTitleLabel->text() + QStringLiteral("\n")
								   + EmptyStateBodyLabel->text());
			}
		});
		emptyLayout->addWidget(copyMessage, 0, Qt::AlignHCenter);
		emptyLayout->addStretch(2);
		PullRequestStack->addWidget(emptyState);

		auto* const content = new QWidget{ PullRequestStack };
		content->setObjectName(QStringLiteral("githubPullRequestContent"));
		auto* const contentLayout = new QVBoxLayout{ content };
		contentLayout->setContentsMargins(0, 0, 0, 0);
		contentLayout->addWidget(PullRequestPanel);
		PullRequestStack->addWidget(content);
		// Bounded: the drawer's files list is what the drawer is for, and a
		// long summary must not be able to take the height it needs.
		PullRequestStack->setMaximumHeight(200);
		// Placed by update_home_landing: on Home while nothing is open, at
		// the top of the drawer's region once a pull request is.
		// The panel builds the description but deliberately does not lay it out:
		// where a description belongs is a decision for whoever assembles the
		// window, and this one puts it under the metadata it describes. Bounded
		// rather than stretching, for the same reason.
		PullRequestPanel->body_editor()->setMaximumHeight(120);
		topColumn->addWidget(PullRequestPanel->body_editor());

		// [[BPR-0646]]. The appearance surface moves out of the review header
		// into the Settings window, a modeless child of this window reached by
		// the preferences shortcut. The composition still finds the same panel
		// through appearance_settings(), so its wiring is unchanged.
		AppearanceSettings = new appearance_settings_panel{ { } };
		AppearanceSettings->setObjectName(
			QStringLiteral("appearanceSettingsPanel"));
		SettingsWindow->adopt_panel(settings_page::appearance, AppearanceSettings);

		CentralStack = new QStackedWidget{ this };
		CentralStack->setObjectName(QStringLiteral("workbenchCentralStack"));
		CentralStack->addWidget(HomePage);
		CentralStack->addWidget(CentralSplitter);
		setCentralWidget(CentralStack);

		// [[BPR-0646]]. Refresh sits in the strip's right region, before the
		// comments toggle, as the design places it. Same action as the
		// session chrome's button, so there is one refresh with one state.
		PrimaryToolbar->insertAction(Drawers[1].Toggle, GithubRefreshAction);
		// The action's default state is applied once everything it governs
		// exists; a default that is never applied is a default in name only.
		apply_review_details_visibility();

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
		// Bounded so the file list above it keeps the drawer\'s height.
		surface->setMaximumHeight(240);
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
		// [[BPR-0650]]. Home's action list: the grouped rows are this
		// delegate's drawing of the same decided queue; a click or Return
		// on a row opens its pull request on Overview, as a row of the
		// repositories tree does.
		QueueDelegate = new review_landing_delegate{ QueueView };
		QueueView->setItemDelegate(QueueDelegate);
		QueueView->setUniformItemSizes(false);
		connect(QueueView, &QAbstractItemView::clicked, this, [this](const QModelIndex& aIndex) {
			if (const auto item = QueueFilterModel->item_at(aIndex); item.has_value()) {
				open_pull_request_item(*item);
			}
		});
		connect(QueueView, &QAbstractItemView::activated, this, [this](const QModelIndex& aIndex) {
			if (const auto item = QueueFilterModel->item_at(aIndex); item.has_value()) {
				open_pull_request_item(*item);
			}
		});
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
		// [[BPR-0652]]. The rule editor is a settings surface: it lives on
		// Reviews & queue, adopted there once the Settings window exists.
		// Home keeps the queue and its filters.
		RulesEditor = new QWidget;
		RulesEditor->setObjectName(QStringLiteral("observerRulesEditor"));
		RulesEditor->setAccessibleName(QStringLiteral("Observer rules"));
		controls->setContentsMargins(0, 0, 0, 0);
		RulesEditor->setLayout(controls);
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
		if (RepositoriesTree != nullptr) {
			RepositoriesTree->apply(aProjection.Items, aProjection.SavedRepositories);
			update_rail_summaries();
		}
		// [[BPR-0660]]. The remembered servers, hostname first so the list
		// reads as the reviewer typed them.
		if (SavedServerSelector != nullptr) {
			const auto selectedServer = SavedServerSelector->currentText();
			SavedServerSelector->clear();
			for (const auto& server : aProjection.SavedServers) {
				SavedServerSelector->addItem(
					server.Account.empty()
						? QString::fromStdString(server.Hostname)
						: QStringLiteral("%1 - %2")
							  .arg(QString::fromStdString(server.Hostname),
								   QString::fromStdString(server.Account)));
			}
			if (const auto restored = SavedServerSelector->findText(selectedServer); restored >= 0) {
				SavedServerSelector->setCurrentIndex(restored);
			}
		}
		if (SavedRepositorySelector != nullptr) {
			const auto selected = SavedRepositorySelector->currentText();
			SavedRepositorySelector->clear();
			for (const auto& saved : aProjection.SavedRepositories) {
				SavedRepositorySelector->addItem(QString::fromStdString(saved.Repository));
			}
			if (const auto restored = SavedRepositorySelector->findText(selected); restored >= 0) {
				SavedRepositorySelector->setCurrentIndex(restored);
			}
		}
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
		update_saved_repository_actions();
		update_saved_server_actions();
		apply_focus_order();
	}

	void workbench_window::build_saved_repositories_editor(QWidget* const aParent, QVBoxLayout& aLayout)
	{
		SavedRepositoriesEditor = new QWidget{ aParent };
		SavedRepositoriesEditor->setObjectName(QStringLiteral("savedRepositoriesEditor"));
		SavedRepositoriesEditor->setAccessibleName(QStringLiteral("Saved repositories"));
		SavedRepositoriesEditor->setAccessibleDescription(QStringLiteral(
			"The repositories the drawer keeps for this account; add one as owner/name or remove the selected one"));
		auto* const controls = new QHBoxLayout{ SavedRepositoriesEditor };
		controls->setContentsMargins(0, 0, 0, 0);
		SavedRepositorySelector = new QComboBox{ SavedRepositoriesEditor };
		SavedRepositorySelector->setObjectName(QStringLiteral("savedRepositorySelector"));
		SavedRepositorySelector->setAccessibleName(QStringLiteral("Saved repositories list"));
		SavedRepositorySelector->setAccessibleDescription(QStringLiteral("Select a saved repository to remove"));
		controls->addWidget(SavedRepositorySelector, 1);
		SavedRepositoryName = new QLineEdit{ SavedRepositoriesEditor };
		SavedRepositoryName->setObjectName(QStringLiteral("savedRepositoryName"));
		SavedRepositoryName->setAccessibleName(QStringLiteral("Repository to save"));
		SavedRepositoryName->setAccessibleDescription(QStringLiteral("owner/name of a repository to keep in the drawer"));
		SavedRepositoryName->setPlaceholderText(QStringLiteral("owner/name"));
		SavedRepositoryName->setMaxLength(static_cast<int>(core::maximum_observer_rule_pattern_bytes));
		controls->addWidget(SavedRepositoryName, 1);
		AddSavedRepository = new QAction{ tr("Save repository"), this };
		AddSavedRepository->setObjectName(QStringLiteral("addSavedRepository"));
		RemoveSavedRepository = new QAction{ tr("Remove saved repository"), this };
		RemoveSavedRepository->setObjectName(QStringLiteral("removeSavedRepository"));
		AddSavedRepositoryButton = new QToolButton{ SavedRepositoriesEditor };
		AddSavedRepositoryButton->setDefaultAction(AddSavedRepository);
		AddSavedRepositoryButton->setObjectName(QStringLiteral("addSavedRepositoryButton"));
		AddSavedRepositoryButton->setAccessibleName(QStringLiteral("Save repository"));
		AddSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Keep the named repository in the drawer for this account; nothing changes on GitHub"));
		controls->addWidget(AddSavedRepositoryButton);
		RemoveSavedRepositoryButton = new QToolButton{ SavedRepositoriesEditor };
		RemoveSavedRepositoryButton->setDefaultAction(RemoveSavedRepository);
		RemoveSavedRepositoryButton->setObjectName(QStringLiteral("removeSavedRepositoryButton"));
		RemoveSavedRepositoryButton->setAccessibleName(QStringLiteral("Remove saved repository"));
		RemoveSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Stop keeping the selected repository in the drawer"));
		controls->addWidget(RemoveSavedRepositoryButton);
		connect(AddSavedRepository, &QAction::triggered, this, [this] {
			dispatch_saved_repository(application::saved_repository_action::add);
		});
		connect(RemoveSavedRepository, &QAction::triggered, this, [this] {
			dispatch_saved_repository(application::saved_repository_action::remove);
		});
		connect(SavedRepositoryName, &QLineEdit::textChanged, this, [this] { update_saved_repository_actions(); });
		connect(SavedRepositorySelector, qOverload<int>(&QComboBox::currentIndexChanged), this,
				[this] { update_saved_repository_actions(); });
		aLayout.addWidget(SavedRepositoriesEditor);
		update_saved_repository_actions();
	}

	void workbench_window::build_enterprise_servers_editor(QWidget* const aParent, QVBoxLayout& aLayout)
	{
		EnterpriseServersEditor = new QWidget{ aParent };
		EnterpriseServersEditor->setObjectName(QStringLiteral("enterpriseServersEditor"));
		EnterpriseServersEditor->setAccessibleName(QStringLiteral("Enterprise servers"));
		EnterpriseServersEditor->setAccessibleDescription(QStringLiteral(
			"Keep a GitHub Enterprise server so it is offered the next time BetterPullReview starts. "
			"No sign-in is stored here."));
		auto* const controls = new QHBoxLayout{ EnterpriseServersEditor };
		controls->setContentsMargins(0, 0, 0, 0);
		SavedServerSelector = new QComboBox{ EnterpriseServersEditor };
		SavedServerSelector->setObjectName(QStringLiteral("savedServerSelector"));
		SavedServerSelector->setAccessibleName(QStringLiteral("Remembered servers"));
		SavedServerSelector->setAccessibleDescription(QStringLiteral("Select a remembered server to forget"));
		controls->addWidget(SavedServerSelector, 1);
		SavedServerHostname = new QLineEdit{ EnterpriseServersEditor };
		SavedServerHostname->setObjectName(QStringLiteral("savedServerHostname"));
		SavedServerHostname->setAccessibleName(QStringLiteral("Server to remember"));
		SavedServerHostname->setAccessibleDescription(
			QStringLiteral("The hostname of a GitHub Enterprise server, without a scheme"));
		SavedServerHostname->setPlaceholderText(QStringLiteral("github.example.com"));
		SavedServerHostname->setMaxLength(253);
		controls->addWidget(SavedServerHostname, 1);
		SavedServerAccount = new QLineEdit{ EnterpriseServersEditor };
		SavedServerAccount->setObjectName(QStringLiteral("savedServerAccount"));
		SavedServerAccount->setAccessibleName(QStringLiteral("Account on that server"));
		SavedServerAccount->setAccessibleDescription(
			QStringLiteral("The account you sign in with there, if you know it"));
		SavedServerAccount->setPlaceholderText(QStringLiteral("account (optional)"));
		SavedServerAccount->setMaxLength(128);
		controls->addWidget(SavedServerAccount, 1);
		AddSavedServer = new QAction{ tr("Remember server"), this };
		AddSavedServer->setObjectName(QStringLiteral("addSavedServer"));
		ForgetSavedServer = new QAction{ tr("Forget server"), this };
		ForgetSavedServer->setObjectName(QStringLiteral("forgetSavedServer"));
		AddSavedServerButton = new QToolButton{ EnterpriseServersEditor };
		AddSavedServerButton->setDefaultAction(AddSavedServer);
		AddSavedServerButton->setObjectName(QStringLiteral("addSavedServerButton"));
		AddSavedServerButton->setAccessibleName(QStringLiteral("Remember server"));
		AddSavedServerButton->setAccessibleDescription(
			QStringLiteral("Offer this server the next time BetterPullReview starts"));
		controls->addWidget(AddSavedServerButton);
		ForgetSavedServerButton = new QToolButton{ EnterpriseServersEditor };
		ForgetSavedServerButton->setDefaultAction(ForgetSavedServer);
		ForgetSavedServerButton->setObjectName(QStringLiteral("forgetSavedServerButton"));
		ForgetSavedServerButton->setAccessibleName(QStringLiteral("Forget server"));
		ForgetSavedServerButton->setAccessibleDescription(
			QStringLiteral("Stop offering the selected server"));
		controls->addWidget(ForgetSavedServerButton);
		connect(AddSavedServer, &QAction::triggered, this, [this] {
			dispatch_saved_server(application::saved_server_action::add);
		});
		connect(ForgetSavedServer, &QAction::triggered, this, [this] {
			dispatch_saved_server(application::saved_server_action::remove);
		});
		connect(SavedServerHostname, &QLineEdit::textChanged, this, [this] { update_saved_server_actions(); });
		connect(SavedServerSelector, qOverload<int>(&QComboBox::currentIndexChanged), this,
				[this] { update_saved_server_actions(); });
		aLayout.addWidget(EnterpriseServersEditor);
		update_saved_server_actions();
	}

	void workbench_window::set_saved_server_dispatch(workbench_saved_server_dispatch aDispatch)
	{
		SavedServerDispatch = std::move(aDispatch);
		update_saved_server_actions();
	}

	auto workbench_window::enterprise_servers_editor() const noexcept -> QWidget*
	{
		return EnterpriseServersEditor;
	}

	auto workbench_window::saved_server_hostname() const noexcept -> QLineEdit*
	{
		return SavedServerHostname;
	}

	auto workbench_window::saved_server_account() const noexcept -> QLineEdit*
	{
		return SavedServerAccount;
	}

	auto workbench_window::saved_server_selector() const noexcept -> QComboBox*
	{
		return SavedServerSelector;
	}

	auto workbench_window::add_saved_server_action() const noexcept -> QAction*
	{
		return AddSavedServer;
	}

	auto workbench_window::forget_saved_server_action() const noexcept -> QAction*
	{
		return ForgetSavedServer;
	}

	void workbench_window::dispatch_saved_server(const application::saved_server_action aAction)
	{
		if (!SavedServerDispatch || !QueueReady) {
			return;
		}
		auto server = application::saved_server{ };
		if (aAction == application::saved_server_action::add) {
			server.Hostname = SavedServerHostname->text().trimmed().toLower().toStdString();
			server.Account  = SavedServerAccount->text().trimmed().toStdString();
		} else {
			const auto index = SavedServerSelector->currentIndex();
			if (index < 0 || static_cast<std::size_t>(index) >= QueueProjection.SavedServers.size()) {
				return;
			}
			server = QueueProjection.SavedServers[static_cast<std::size_t>(index)];
		}
		if (!application::valid(server)) {
			return;
		}
		SavedServerDispatch(application::saved_server_intent{
			.Action = aAction,
			.Server = std::move(server),
		});
		if (aAction == application::saved_server_action::add) {
			SavedServerHostname->clear();
			SavedServerAccount->clear();
		}
	}

	void workbench_window::update_saved_server_actions()
	{
		if (AddSavedServer == nullptr || ForgetSavedServer == nullptr) {
			return;
		}
		// A server is remembered without an account and without signing in:
		// only the queue's own durable state is needed.
		const auto durable = QueueReady && static_cast<bool>(SavedServerDispatch);
		AddSavedServer->setEnabled(
			durable
			&& application::valid(application::saved_server{
				.Hostname = SavedServerHostname != nullptr
					? SavedServerHostname->text().trimmed().toLower().toStdString()
					: std::string{ },
				.Account  = SavedServerAccount != nullptr
					? SavedServerAccount->text().trimmed().toStdString()
					: std::string{ },
			}));
		ForgetSavedServer->setEnabled(
			durable && SavedServerSelector != nullptr && SavedServerSelector->currentIndex() >= 0);
		AddSavedServer->setToolTip(
			QStringLiteral("Offer this server at the next start. Signing in there is still the device flow."));
		ForgetSavedServer->setToolTip(QStringLiteral("Stop offering the selected server."));
	}

	void workbench_window::set_saved_repository_dispatch(workbench_saved_repository_dispatch aDispatch)
	{
		SavedRepositoryDispatch = std::move(aDispatch);
		update_saved_repository_actions();
	}

	auto workbench_window::saved_repositories_editor() const noexcept -> QWidget*
	{
		return SavedRepositoriesEditor;
	}

	auto workbench_window::saved_repository_name() const noexcept -> QLineEdit*
	{
		return SavedRepositoryName;
	}

	auto workbench_window::add_saved_repository_action() const noexcept -> QAction*
	{
		return AddSavedRepository;
	}

	auto workbench_window::remove_saved_repository_action() const noexcept -> QAction*
	{
		return RemoveSavedRepository;
	}

	void workbench_window::dispatch_saved_repository(const application::saved_repository_action aAction)
	{
		if (!SavedRepositoryDispatch || !QueueReady) {
			return;
		}
		auto repository = application::saved_repository{
			.Host    = QueueProjection.Session.Host,
			.Account = QueueProjection.Session.Account,
		};
		if (aAction == application::saved_repository_action::add) {
			repository.Repository = SavedRepositoryName->text().trimmed().toStdString();
		} else {
			const auto index = SavedRepositorySelector->currentIndex();
			if (index < 0
				|| static_cast<std::size_t>(index) >= QueueProjection.SavedRepositories.size()) {
				return;
			}
			repository = QueueProjection.SavedRepositories[static_cast<std::size_t>(index)];
		}
		if (!application::valid(repository)) {
			return;
		}
		SavedRepositoryDispatch(application::saved_repository_intent{
			.Action     = aAction,
			.Repository = std::move(repository),
		});
	}

	void workbench_window::update_saved_repository_actions()
	{
		if (AddSavedRepository == nullptr || RemoveSavedRepository == nullptr) {
			return;
		}
		// An account is needed to save for: the queue session names it once
		// a pull request has been opened signed in.
		const auto account = QueueReady && static_cast<bool>(SavedRepositoryDispatch)
			&& !QueueProjection.Session.Host.empty() && !QueueProjection.Session.Account.empty();
		AddSavedRepository->setEnabled(
			account
			&& application::valid(application::saved_repository{
				.Host       = QueueProjection.Session.Host,
				.Account    = QueueProjection.Session.Account,
				.Repository = SavedRepositoryName != nullptr ? SavedRepositoryName->text().trimmed().toStdString() : std::string{ },
			}));
		RemoveSavedRepository->setEnabled(
			account && SavedRepositorySelector != nullptr && SavedRepositorySelector->currentIndex() >= 0);
		AddSavedRepository->setToolTip(account
										   ? QStringLiteral("Keep this repository in the drawer; this never changes GitHub.")
										   : QStringLiteral("Open a pull request signed in first, so there is an account to save for."));
		RemoveSavedRepository->setToolTip(QStringLiteral("Stop keeping the selected repository in the drawer."));
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
			const auto previousFile = conversation_anchor_file();
			Cursor                  = aProjection.Selection->Cursor;
			apply_file_selection();
			if (ConversationPanel != nullptr && previousFile != conversation_anchor_file()) {
				ApplyingConversationAnchor = true;
				ConversationPanel->set_anchor_file(conversation_anchor_file());
				ConversationPanel->set_anchor_line(std::nullopt);
				ApplyingConversationAnchor = false;
				refresh_cards();
			}
		}
		if (aProjection.Drawers) {
			DrawerStates = *aProjection.Drawers;
			++Statistics.DrawerUpdates;
		}
		if (aProjection.LineNumbers) {
			LineNumberState = *aProjection.LineNumbers;
		}
		if (aProjection.Layout) {
			LayoutWidths = *aProjection.Layout;
			++Statistics.LayoutUpdates;
		}
		if (aProjection.Behaviour) {
			Behaviour          = *aProjection.Behaviour;
			BehaviourPublished = true;
			apply_behaviour_controls();
			consider_mark_read_when_fully_seen();
			// A pull request that opened before the preferences landed takes
			// them now, once each.
			if (HasGithubPullRequest) {
				apply_first_comments_filter();
				if (!ReturnComparisonAsked) {
					ReturnComparisonPending = Behaviour.ReturnComparison
						!= application::review_return_comparison::whole_pr;
					honour_return_comparison();
				}
			}
		}
		if (aProjection.Drawers || aProjection.Layout) {
			apply_drawer_layout();
		}
		if (aProjection.Drawers) {
			infer_modality_from_drawers();
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
		if (FilesTree != nullptr) {
			FilesTree->apply(rows);
		}
		ApplyingFileSelection = false;
		++Statistics.FileRowUpdates;
		apply_file_selection();
	}

	auto workbench_window::conversation_anchor_file() const
		-> std::optional<core::file_identity>
	{
		// File selection survives the asynchronous gap between clearing the
		// previous diff and receiving the next one. Widening to the whole PR
		// in that gap rebuilds unrelated threads on every file switch.
		if (Cursor.has_value()) {
			return Cursor->File;
		}
		// Standalone diff presentation has no workbench cursor projection.
		if (ComparisonSurface != nullptr && ComparisonSurface->current_context()) {
			return ComparisonSurface->current_context()->Key.File;
		}
		return std::nullopt;
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
		if (FilesTree != nullptr) {
			FilesTree->select_file(Cursor ? Cursor->File : std::nullopt);
		}
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
		honour_return_comparison();
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
		consider_mark_read_when_fully_seen();
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

	void workbench_window::set_review_submission_dispatch(review_submission_editor_dispatch aDispatch) { ReviewSubmissionDispatch = std::move(aDispatch); }
	auto workbench_window::review_submission_editor_view() const noexcept -> review_submission_editor* { return ReviewSubmissionEditor; }

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

	void workbench_window::set_device_authorization_dispatch(
		application::device_authorization_gesture_dispatch aDispatch)
	{
		DeviceDispatch = std::move(aDispatch);
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
		if (!DefectClassificationDispatch) {
			return;
		}
		auto classification = selected_conversation_classification();
		if (!classification.has_value()) {
			return;
		}
		++Statistics.DefectClassifications;
		DefectClassificationDispatch(std::move(*classification));
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

	void workbench_window::synchronize_selected_conversation(
		const QModelIndex& aIndex)
	{
		if (ComparisonSurface == nullptr) {
			return;
		}
		auto root = aIndex;
		while (root.parent().isValid()) {
			root = root.parent();
		}
		if (!root.isValid()
			|| root.data(github_conversation_model::anchor_availability_role).toInt()
				!= static_cast<int>(
					application::github_conversation_anchor_availability::current)) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		bool       validLine = false;
		const auto oneBased  = root.data(github_conversation_model::line_role)
								   .toULongLong(&validLine);
		const auto sideValue = root.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U
			|| oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto side = github_conversation_source_side(sideValue.toInt());
		if (!side.has_value()) {
			ComparisonSurface->clear_conversation_anchor();
			ConversationPanel->model()->set_anchor_availability(
				root,
				application::github_conversation_anchor_availability::missing_side);
			return;
		}
		const auto endpointIsCurrent = *side == core::diff_side::left
			? LeftConversationEndpointCurrent
			: RightConversationEndpointCurrent;
		if (!endpointIsCurrent) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		if (!ComparisonSurface->reveal_conversation_line(
				*side, static_cast<std::uint32_t>(oneBased - 1U))) {
			ComparisonSurface->clear_conversation_anchor();
			ConversationPanel->model()->set_anchor_availability(
				root,
				application::github_conversation_anchor_availability::selected_line_out_of_bounds);
		}
	}

	void workbench_window::update_defect_surface()
	{
		ClassifyDefect->setEnabled(
			static_cast<bool>(DefectClassificationDispatch)
			&& selected_conversation_classification().has_value());
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

	auto workbench_window::selected_conversation_classification() const
		-> std::optional<workbench_defect_classification>
	{
		if (ConversationPanel == nullptr || ConversationPanel->view() == nullptr
			|| ConversationPanel->model() == nullptr || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return std::nullopt;
		}
		const auto current = ConversationPanel->view()->currentIndex();
		if (!current.isValid() || current.parent().isValid()
			|| current.data(github_conversation_model::kind_role).toInt()
				!= static_cast<int>(
					application::github_conversation_kind::review_thread)
			|| current
					.data(github_conversation_model::anchor_availability_role)
					.toInt()
				!= static_cast<int>(
					application::github_conversation_anchor_availability::current)) {
			return std::nullopt;
		}
		bool       validLine = false;
		const auto oneBased  = current.data(github_conversation_model::line_role)
								   .toULongLong(&validLine);
		const auto sideValue = current.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U
			|| oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			return std::nullopt;
		}
		const auto projectedSide = sideValue.toInt();
		if (projectedSide
				!= static_cast<int>(
					application::github_conversation_diff_side::left)
			&& projectedSide
				!= static_cast<int>(
					application::github_conversation_diff_side::right)) {
			return std::nullopt;
		}
		const auto  side              = projectedSide
				== static_cast<int>(
							  application::github_conversation_diff_side::left)
			? core::diff_side::left
			: core::diff_side::right;
		const auto  endpointIsCurrent = side == core::diff_side::left
			? LeftConversationEndpointCurrent
			: RightConversationEndpointCurrent;
		const auto& alignment         = ComparisonSurface->current_result()->Alignment;
		const auto  line              = static_cast<std::uint32_t>(oneBased - 1U);
		const auto  lineCount         = side == core::diff_side::left
			? alignment.LeftLineCount
			: alignment.RightLineCount;
		const auto  identity          = ConversationPanel->model()->id_for_index(current);
		if (!endpointIsCurrent || line >= lineCount || identity.empty()) {
			return std::nullopt;
		}
		return workbench_defect_classification{
			.Conversation = identity,
			.Anchor       = {
				.Comparison = alignment.Digest,
				.Side       = side,
				.Line       = line,
			},
		};
	}

	auto workbench_window::primary_toolbar() const noexcept -> QToolBar*
	{
		return PrimaryToolbar;
	}

	auto workbench_window::modality() const noexcept -> modality_control*
	{
		return Modality;
	}

	auto workbench_window::current_modality() const noexcept -> review_modality
	{
		return Modality != nullptr ? Modality->current() : review_modality::review;
	}

	void workbench_window::choose_modality(const review_modality aModality)
	{
		if (Modality != nullptr) {
			Modality->adopt(aModality);
		}
		// [[BPR-0661]]. "Last used" is the modality chosen while a pull
		// request was open.
		if (HasGithubPullRequest) {
			LastUsedModality = aModality;
		}
		ModalityChosen = true;
		apply_modality_surface();
		if (HomeRequested) {
			HomeRequested = false;
			update_home_landing();
		}
		// A modality is two exact set-drawer intents; the core decides them
		// as it decides a toggle. With no active workbench the drawers cannot
		// change, so nothing is asked and the strip merely shows the preset.
		const auto files = command(core::workbench_command_id::toggle_files_drawer);
		const auto conversations
			= command(core::workbench_command_id::toggle_conversations_drawer);
		if (!Dispatch || !files || !files->Enabled || !conversations
			|| !conversations->Enabled) {
			return;
		}
		const auto filesState         = aModality == review_modality::read
				|| aModality == review_modality::discuss
			? core::workbench_drawer_state::collapsed
			: core::workbench_drawer_state::expanded;
		const auto conversationsState = aModality == review_modality::overview
			? core::workbench_drawer_state::collapsed
			: core::workbench_drawer_state::expanded;
		Dispatch(core::set_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::files,
			.State  = filesState,
		});
		Dispatch(core::set_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::conversations,
			.State  = conversationsState,
		});
	}

	auto workbench_window::home_landing() const noexcept -> workbench_landing*
	{
		return HomeLanding;
	}

	void workbench_window::publish(
		const application::pull_request_overview_projection& aProjection)
	{
		if (OverviewPage == nullptr) {
			return;
		}
		OverviewPage->present(aProjection);
		if (FilesTree != nullptr) {
			// [[BPR-0658]]. The tree draws the overview's per-file facts.
			ApplyingFileSelection = true;
			FilesTree->apply_overview(aProjection.Files);
			ApplyingFileSelection = false;
		}
		apply_modality_surface();
	}

	void workbench_window::clear_overview()
	{
		if (OverviewPage == nullptr) {
			return;
		}
		OverviewPage->clear();
		if (FilesTree != nullptr) {
			ApplyingFileSelection = true;
			FilesTree->apply_overview({ });
			ApplyingFileSelection = false;
		}
		apply_modality_surface();
	}

	auto workbench_window::overview_page() const noexcept -> pull_request_overview_page*
	{
		return OverviewPage;
	}

	auto workbench_window::showing_overview() const -> bool
	{
		return CentreStack != nullptr && CentreStack->currentWidget() == OverviewPage;
	}

	void workbench_window::refresh_cards()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		cards->set_draft(
			ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt,
			ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt);
		cards->relayout();
		apply_thread_marks();
	}

	void workbench_window::apply_thread_marks()
	{
		if (ComparisonSurface == nullptr || ConversationPanel == nullptr) {
			return;
		}
		// Amber for a thread on the line; blue for a thread whose first post
		// is newer than what the reader has read ([[BPR-0656]]).
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			auto       marks  = std::vector<diff_line_mark>{ };
			const auto unread = ConversationPanel->cards()->unread_marked_lines(side);
			for (const auto line : ConversationPanel->cards()->marked_lines(side)) {
				const auto fresh = std::ranges::find(unread, line) != unread.end();
				marks.push_back(diff_line_mark{
					.Line = line,
					.Kind = fresh ? diff_line_mark_kind::new_thread : diff_line_mark_kind::thread });
			}
			ComparisonSurface->set_line_marks(side, std::move(marks));
		}
		update_new_comment_group();
	}

	void workbench_window::set_conversation_read_dispatch(conversation_read_dispatch aDispatch)
	{
		ConversationReadDispatch = std::move(aDispatch);
	}

	void workbench_window::present_read_state(std::vector<thread_read_state> aState)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		// [[BPR-0649]]. The map fills the same posts the cards mark.
		auto unread = std::vector<std::string>{ };
		for (const auto& thread : aState) {
			unread.insert(unread.end(), thread.UnreadPosts.begin(), thread.UnreadPosts.end());
		}
		if (ConversationPanel->thread_map() != nullptr) {
			ConversationPanel->thread_map()->set_unread(std::move(unread));
		}
		ConversationPanel->cards()->set_read_state(std::move(aState));
		apply_thread_marks();
	}

	auto workbench_window::next_new_comment_action() const noexcept -> QAction*
	{
		return NextNewCommentAction;
	}

	auto workbench_window::new_comment_count_label() const noexcept -> QLabel*
	{
		return NewCommentCountLabel;
	}

	void workbench_window::update_new_comment_group()
	{
		if (NextNewCommentAction == nullptr || NewCommentCountLabel == nullptr
			|| ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto unread = ConversationPanel->cards()->unread_threads_in_file_order();
		NextNewCommentAction->setEnabled(!unread.empty());
		NewCommentCountLabel->setText(
			unread.empty() ? QString{ } : QStringLiteral("%1 new").arg(unread.size()));
		NextNewCommentAction->setToolTip(
			unread.empty() ? QStringLiteral("Next new comment: nothing unread")
						   : QStringLiteral("Next new comment (%1 threads)").arg(unread.size()));
	}

	void workbench_window::step_to_next_new_comment()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto unread = ConversationPanel->cards()->unread_threads_in_file_order();
		if (unread.empty()) {
			return;
		}
		// The first unread thread after the current one in file order, and
		// the first of all once the last is passed.
		const auto& current = ConversationPanel->cards()->current_thread();
		auto        next    = unread.front();
		if (const auto at = std::ranges::find(unread, current); at != unread.end()) {
			const auto following = std::next(at);
			next                 = following != unread.end() ? *following : unread.front();
		}
		// The conversation view's current index is what reveals the line, as a
		// press on a card does; the cards follow it.
		const auto index = ConversationPanel->model()->index_for_id(next);
		if (index.isValid()) {
			ConversationPanel->view()->setCurrentIndex(index);
		}
		ConversationPanel->cards()->set_current_thread(next);
	}

	void workbench_window::apply_modality_surface()
	{
		if (CentreStack == nullptr || OverviewPage == nullptr) {
			return;
		}
		// The overview is shown for the Overview modality once the host has
		// decided one; before that, and in every other modality, the centre
		// is the comparison. An empty overview would say less than the panes.
		const auto overview = Modality != nullptr
			&& Modality->current() == review_modality::overview
			&& OverviewPage->presented();
		CentreStack->setCurrentWidget(overview ? static_cast<QWidget*>(OverviewPage) : ComparisonPage);
		// [[BPR-0649]]. Discuss widens the conversation over the code: the
		// drawer takes its Discuss width and its panel the Discuss arrangement.
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_discuss(
				Modality != nullptr && Modality->current() == review_modality::discuss);
		}
		apply_drawer_widths();
	}

	void workbench_window::enter_read_at(const QString& aPath)
	{
		// The overview names a path; the core knows the file by identity.
		// Selecting it is the same intent the files drawer sends, and Read is
		// the modality the accepted design enters from Continue review.
		if (const auto file = file_for_path(aPath); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
		// [[BPR-0661]]. Read unless the reviewer asked Continue review to
		// enter Review.
		choose_modality(Behaviour.ContinueModality == application::review_continue_modality::review
							? review_modality::review
							: review_modality::read);
	}

	auto workbench_window::select_thread_line(const std::string& aIdentity) -> bool
	{
		if (ConversationPanel == nullptr || ComparisonSurface == nullptr) {
			return false;
		}
		auto root = ConversationPanel->model()->index_for_id(aIdentity);
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		if (!root.isValid()
			|| root.data(github_conversation_model::anchor_availability_role).toInt()
				!= static_cast<int>(application::github_conversation_anchor_availability::current)) {
			return false;
		}
		bool       validLine = false;
		const auto oneBased  = root.data(github_conversation_model::line_role).toULongLong(&validLine);
		const auto sideValue = root.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U || oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			return false;
		}
		const auto side = github_conversation_source_side(sideValue.toInt());
		if (!side.has_value()) {
			return false;
		}
		return ComparisonSurface->select_conversation_line(*side, static_cast<std::uint32_t>(oneBased - 1U));
	}

	void workbench_window::set_review_reply_dispatch(workbench_review_reply_dispatch aDispatch)
	{
		ReviewReplyDispatch = std::move(aDispatch);
	}

	void workbench_window::set_thread_resolution_dispatch(workbench_thread_resolution_dispatch aDispatch)
	{
		ThreadResolutionDispatch = std::move(aDispatch);
	}

	void workbench_window::set_thread_resolution_provider(const bool aResolves)
	{
		ProviderResolvesThreads = aResolves;
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_resolution_offered(
				ProviderResolvesThreads
				&& (Capabilities.ResolveThread == core::github_capability_state::supported
					|| Capabilities.UnresolveThread == core::github_capability_state::supported));
		}
	}

	void workbench_window::set_behaviour_dispatch(review_behaviour_dispatch aDispatch)
	{
		BehaviourDispatch = std::move(aDispatch);
	}

	auto workbench_window::behaviour() const noexcept -> const application::review_behaviour_settings&
	{
		return Behaviour;
	}

	auto workbench_window::behaviour_editor() const noexcept -> QWidget*
	{
		return BehaviourEditor;
	}

	// [[BPR-0661]]. Two groups of segmented rows: the modality rules (which
	// modality a pull request opens in; which one Continue review enters) and
	// the reading behaviour (when a file is marked read; the comparison on
	// return; the comments drawer's first filter). Each group resets on its
	// own. A row dispatches the whole record; the host validates, persists and
	// publishes it, and the page follows what is published, so a refused
	// record reverts.
	void workbench_window::build_behaviour_editor(QWidget* const aPage, QVBoxLayout* const aColumn)
	{
		BehaviourEditor = new QWidget{ aPage };
		BehaviourEditor->setObjectName(QStringLiteral("reviewBehaviourEditor"));
		BehaviourEditor->setAccessibleName(QStringLiteral("Review behaviour"));
		auto* const column = new QVBoxLayout{ BehaviourEditor };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);

		struct option
		{
			QString Label;
			QString Name;
		};
		struct row
		{
			std::size_t         Index;
			QString             Label;
			QString             Name;
			std::vector<option> Options;
		};
		const auto addGroup = [this, column](const QString&          aHeading,
											 const QString&          aResetName,
											 const std::vector<row>& aRows,
											 auto                    aReset) {
			auto* const headingRow = new QHBoxLayout{ };
			auto* const heading    = new QLabel{ aHeading, BehaviourEditor };
			heading->setObjectName(aResetName + QStringLiteral("Heading"));
			headingRow->addWidget(heading, 1);
			auto* const reset = new QToolButton{ BehaviourEditor };
			reset->setObjectName(aResetName);
			reset->setText(QStringLiteral("Reset"));
			reset->setAccessibleName(QStringLiteral("Reset %1").arg(aHeading.toLower()));
			reset->setAccessibleDescription(
				QStringLiteral("Returns this group to its defaults; the other group is left alone"));
			reset->setFocusPolicy(Qt::StrongFocus);
			connect(reset, &QToolButton::clicked, this, [this, aReset] {
				if (!BehaviourDispatch) {
					return;
				}
				auto next = behaviour_from_controls();
				aReset(next);
				BehaviourDispatch(next);
			});
			headingRow->addWidget(reset);
			column->addLayout(headingRow);
			for (const auto& one : aRows) {
				auto* const line  = new QHBoxLayout{ };
				auto* const label = new QLabel{ one.Label, BehaviourEditor };
				label->setObjectName(one.Name + QStringLiteral("Label"));
				label->setMinimumWidth(180);
				line->addWidget(label);
				auto& buttons = BehaviourOptions[one.Index];
				// One exclusive group per row: autoExclusive would make every
				// button under the editor one group.
				auto* const group = new QButtonGroup{ BehaviourEditor };
				group->setExclusive(true);
				for (const auto& choice : one.Options) {
					auto* const button = new QToolButton{ BehaviourEditor };
					button->setObjectName(choice.Name);
					button->setText(choice.Label);
					button->setAccessibleName(one.Label + QStringLiteral(": ") + choice.Label);
					button->setCheckable(true);
					group->addButton(button);
					button->setFocusPolicy(Qt::StrongFocus);
					connect(button, &QToolButton::toggled, this, [this](const bool aChecked) {
						if (aChecked && !ApplyingBehaviour) {
							dispatch_behaviour_from_controls();
						}
					});
					line->addWidget(button);
					buttons.push_back(button);
				}
				line->addStretch(1);
				column->addLayout(line);
			}
		};
		addGroup(
			QStringLiteral("Modality rules"),
			QStringLiteral("resetModalityRules"),
			{
				row{ 0U,
					 QStringLiteral("A pull request opens in"),
					 QStringLiteral("openModality"),
					 { { QStringLiteral("Overview"), QStringLiteral("openModalityOverview") },
					   { QStringLiteral("Read"), QStringLiteral("openModalityRead") },
					   { QStringLiteral("Last used"), QStringLiteral("openModalityLastUsed") } } },
				row{ 1U,
					 QStringLiteral("Continue review enters"),
					 QStringLiteral("continueModality"),
					 { { QStringLiteral("Read"), QStringLiteral("continueModalityRead") },
					   { QStringLiteral("Review"), QStringLiteral("continueModalityReview") } } },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.OpenModality     = application::review_behaviour_settings{ }.OpenModality;
				aNext.ContinueModality = application::review_behaviour_settings{ }.ContinueModality;
			});
		addGroup(
			QStringLiteral("Reading"),
			QStringLiteral("resetReadingBehaviour"),
			{
				row{ 2U,
					 QStringLiteral("Mark a file read"),
					 QStringLiteral("markRead"),
					 { { QStringLiteral("By hand"), QStringLiteral("markReadByHand") },
					   { QStringLiteral("When fully seen"), QStringLiteral("markReadWhenFullySeen") } } },
				row{ 3U,
					 QStringLiteral("On return, compare"),
					 QStringLiteral("returnComparison"),
					 { { QStringLiteral("Whole PR"), QStringLiteral("returnComparisonWholePr") },
					   { QStringLiteral("Since my last review"), QStringLiteral("returnComparisonSinceLastReview") },
					   { QStringLiteral("Since last upload"), QStringLiteral("returnComparisonSinceLastUpload") } } },
				row{ 4U,
					 QStringLiteral("Comments drawer opens on"),
					 QStringLiteral("commentsFilter"),
					 { { QStringLiteral("All"), QStringLiteral("commentsFilterAll") },
					   { QStringLiteral("Unread"), QStringLiteral("commentsFilterUnread") },
					   { QStringLiteral("Unresolved"), QStringLiteral("commentsFilterUnresolved") } } },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.MarkRead         = application::review_behaviour_settings{ }.MarkRead;
				aNext.ReturnComparison = application::review_behaviour_settings{ }.ReturnComparison;
				aNext.CommentsFilter   = application::review_behaviour_settings{ }.CommentsFilter;
			});
		aColumn->addWidget(BehaviourEditor);
		apply_behaviour_controls();
	}

	void workbench_window::apply_behaviour_controls()
	{
		if (BehaviourEditor == nullptr) {
			return;
		}
		ApplyingBehaviour = true;
		const auto check  = [this](const std::size_t aRow, const std::size_t aChoice) {
			auto& buttons = BehaviourOptions[aRow];
			if (aChoice < buttons.size()) {
				buttons[aChoice]->setChecked(true);
			}
		};
		check(0U, static_cast<std::size_t>(Behaviour.OpenModality));
		check(1U, static_cast<std::size_t>(Behaviour.ContinueModality));
		check(2U, static_cast<std::size_t>(Behaviour.MarkRead));
		check(3U, static_cast<std::size_t>(Behaviour.ReturnComparison));
		check(4U, static_cast<std::size_t>(Behaviour.CommentsFilter));
		ApplyingBehaviour = false;
	}

	auto workbench_window::behaviour_from_controls() const -> application::review_behaviour_settings
	{
		const auto chosen = [this](const std::size_t aRow) -> std::size_t {
			const auto& buttons = BehaviourOptions[aRow];
			for (std::size_t index = 0U; index < buttons.size(); ++index) {
				if (buttons[index]->isChecked()) {
					return index;
				}
			}
			return 0U;
		};
		return application::review_behaviour_settings{
			.OpenModality     = static_cast<application::review_open_modality>(chosen(0U)),
			.ContinueModality = static_cast<application::review_continue_modality>(chosen(1U)),
			.MarkRead         = static_cast<application::review_mark_read_behaviour>(chosen(2U)),
			.ReturnComparison = static_cast<application::review_return_comparison>(chosen(3U)),
			.CommentsFilter   = static_cast<application::review_comments_filter>(chosen(4U)),
		};
	}

	void workbench_window::dispatch_behaviour_from_controls()
	{
		if (!BehaviourDispatch) {
			return;
		}
		const auto next = behaviour_from_controls();
		if (next != Behaviour) {
			BehaviourDispatch(next);
		}
	}

	auto workbench_window::opening_modality() const -> review_modality
	{
		switch (Behaviour.OpenModality) {
		case application::review_open_modality::read:
			return review_modality::read;
		case application::review_open_modality::last_used:
			return LastUsedModality.value_or(review_modality::overview);
		case application::review_open_modality::overview:
			break;
		}
		return review_modality::overview;
	}

	void workbench_window::apply_first_comments_filter()
	{
		if (ConversationPanel == nullptr || FirstFilterApplied || !BehaviourPublished) {
			return;
		}
		FirstFilterApplied = true;
		ConversationPanel->apply_first_filter(Behaviour.CommentsFilter);
	}

	// The named comparison is dispatched through the control's own action,
	// which is what carries the catalog revision the core expects; until the
	// history offers it the preference waits, and a later epoch publish asks
	// again.
	void workbench_window::honour_return_comparison()
	{
		if (!ReturnComparisonPending || EpochSelectors == nullptr) {
			return;
		}
		auto* const action = Behaviour.ReturnComparison
				== application::review_return_comparison::since_last_review
			? EpochSelectors->last_reviewed_action()
			: Behaviour.ReturnComparison == application::review_return_comparison::since_last_upload
			? EpochSelectors->previous_upload_action()
			: nullptr;
		if (action == nullptr) {
			ReturnComparisonPending = false;
			return;
		}
		if (!action->isEnabled()) {
			return;
		}
		ReturnComparisonPending = false;
		ReturnComparisonAsked   = true;
		if (!action->isChecked()) {
			action->trigger();
		}
	}

	// "Fully seen" is the shared scrollbar at its end (or nothing to scroll)
	// with a comparison shown; the file's own mark-read command is dispatched
	// once per file and material, only while the ledger offers it.
	void workbench_window::consider_mark_read_when_fully_seen()
	{
		if (Behaviour.MarkRead != application::review_mark_read_behaviour::when_fully_seen
			|| !ReceiptDispatch || !Receipt.has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return;
		}
		const auto* const scrollbar = ComparisonSurface->shared_scrollbar();
		if (scrollbar == nullptr || scrollbar->value() < scrollbar->maximum()) {
			return;
		}
		for (const auto& action : Receipt->Actions) {
			if (action.Command.Kind != core::file_receipt_command_kind::mark_read || !action.Enabled) {
				continue;
			}
			if (MarkedReadWhenSeen == std::optional{ action.Command }) {
				return;
			}
			MarkedReadWhenSeen = action.Command;
			ReceiptDispatch(action.Command);
			return;
		}
	}

	auto workbench_window::file_for_path(const QString& aPath) const
		-> std::optional<core::workbench_file_id>
	{
		const auto* const model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
		if (model == nullptr) {
			return std::nullopt;
		}
		for (int row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (model->data(index, Qt::DisplayRole).toString() == aPath) {
				return model->file_at(index);
			}
		}
		return std::nullopt;
	}

	void workbench_window::update_home_landing()
	{
		if (HomeLanding == nullptr || SessionPanel == nullptr) {
			return;
		}
		// Shown only while the centre has nothing: no pull request open, no
		// comparison held, and no load under way. Which way in it offers
		// follows the session: an account first, then a pull request.
		const auto status   = SessionPanel->status();
		const auto signedIn = status == core::github_status::session_ready
			|| status == core::github_status::loading_pull_request
			|| status == core::github_status::pull_request_ready
			|| status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
		const auto comparing = ComparisonSurface != nullptr
			&& ComparisonSurface->current_result() != nullptr;
		const auto loading = status == core::github_status::loading_pull_request;
		HomeLanding->set_review_identity(IdentityText);
		HomeLanding->set_state(
			HasGithubPullRequest ? landing_state::back_to_review
				: signedIn       ? landing_state::choose_pull_request
								 : landing_state::sign_in);
		// Home is the centre until a pull request, a comparison or an active
		// workbench gives the review chrome something to be about.
		// "Active" is the core's word for it: the drawer command is offered
		// only while a workbench is published. A projection with every
		// command refused, which the host sends at launch, is not one.
		// Any command the core offers means a workbench; hiding a QAction
		// also disables it, so a partly refused projection must keep the
		// review chrome and its enabled toggles.
		auto active = false;
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			const auto projection = command(static_cast<core::workbench_command_id>(index));
			active                = active || (projection && projection->Enabled);
		}
		const auto nothing = !HasGithubPullRequest && !comparing && !loading && !active;
		const auto home    = nothing || HomeRequested;
		if (HomeAction != nullptr && HomeAction->isChecked() != home) {
			HomeAction->setChecked(home);
		}
		if (CentralStack != nullptr) {
			CentralStack->setCurrentWidget(home ? HomePage : static_cast<QWidget*>(CentralSplitter));
		}
		// On Home the strip keeps only what Home can use: the account and
		// settings at its right, the inset and the spacers that place them.
		// Drawers, navigation, the modalities, review details and refresh
		// are a pull request's, and return with one.
		if (PrimaryToolbar != nullptr && (StripHome != home || home)) {
			StripHome = home;
			for (auto* const action : PrimaryToolbar->actions()) {
				const auto name = action->objectName();
				const auto kept = action == AccountAction || action == SettingsAction
					|| action == HomeAction
					|| name == QLatin1String{ "workbenchStripInsetAction" }
					|| name == QLatin1String{ "workbenchNavigationSpacerAction" }
					|| name == QLatin1String{ "workbenchModalitySpacerAction" }
					// With a review open behind Home, the modalities are the
					// way back to it.
					|| (HasGithubPullRequest
						&& name == QLatin1String{ "workbenchModalityAction" });
				action->setVisible(!home || kept);
			}
			apply_strip_width();
			// The toolbar shows and hides a widget action's widget on its
			// next layout; take it now so what the strip shows is settled
			// when this returns.
			if (auto* const layout = PrimaryToolbar->layout(); layout != nullptr) {
				layout->invalidate();
				layout->activate();
			}
		}
		// The pull request stack is Home's while nothing is open -- its empty
		// page is where a failed load says why -- and the drawer's once a pull
		// request is, where its summary heads the files. One widget, moved,
		// so what the projections write to it never has two homes.
		if (PullRequestStack != nullptr && HomeColumn != nullptr
			&& DrawerTopColumn != nullptr) {
			auto* const wanted = home ? HomeColumn : DrawerTopColumn;
			if (PullRequestStack->parentWidget() != wanted->parentWidget()) {
				if (auto* const current = PullRequestStack->parentWidget();
					current != nullptr && current->layout() != nullptr) {
					current->layout()->removeWidget(PullRequestStack);
				}
				if (home) {
					// Under the landing card, above the queue heading.
					wanted->insertWidget(1, PullRequestStack);
				} else {
					wanted->insertWidget(0, PullRequestStack);
				}
				PullRequestStack->show();
			}
			// On Home while signed out, the stack's empty page would only
			// restate the card; it earns its place when it carries a load
			// failure or a diagnostic, or once there is a session.
			PullRequestStack->setVisible(
				!home || signedIn || SessionPanel->showing_diagnostic());
		}
	}

	auto workbench_window::home_action() const noexcept -> QAction*
	{
		return HomeAction;
	}

	void workbench_window::choose_home(const bool aHome)
	{
		// Asked for, Home shows over an open review until a modality or a
		// pull request brings the review back; with nothing open Home is
		// simply where the window is, and cannot be left.
		HomeRequested = aHome;
		update_home_landing();
	}

	auto workbench_window::home_page() const noexcept -> QWidget*
	{
		return HomePage;
	}

	auto workbench_window::showing_home() const -> bool
	{
		return CentralStack != nullptr && CentralStack->currentWidget() == HomePage;
	}

	auto workbench_window::account_action() const noexcept -> QAction*
	{
		return AccountAction;
	}

	auto workbench_window::identity_label() const noexcept -> QLabel*
	{
		return IdentityLabel;
	}

	auto workbench_window::change_position_label() const noexcept -> QLabel*
	{
		return ChangePositionLabel;
	}

	auto workbench_window::file_position_label() const noexcept -> QLabel*
	{
		return FilePositionLabel;
	}

	void workbench_window::update_strip_positions()
	{
		if (FilePositionLabel == nullptr || ChangePositionLabel == nullptr) {
			return;
		}
		auto fileText = QString{ };
		if (Cursor && Cursor->ActionableCount > 0U) {
			fileText = Cursor->Ordinal > 0U
				? QStringLiteral("file %1 of %2")
					  .arg(Cursor->Ordinal)
					  .arg(Cursor->ActionableCount)
				: QStringLiteral("%1 files").arg(Cursor->ActionableCount);
		}
		FilePositionLabel->setText(fileText);

		// Derived from the comparison the surface holds, exactly as the host
		// derives the change cursor it observes for the core: the group the
		// selection names, counted among the alignment's groups. No second
		// authority, and nothing when the selection is for another comparison.
		auto changeText = QString{ };
		if (ComparisonSurface != nullptr) {
			const auto& result    = ComparisonSurface->current_result();
			const auto& selection = ComparisonSurface->selection();
			if (result != nullptr) {
				const auto& groups  = result->Alignment.Groups;
				auto        ordinal = std::size_t{ 0U };
				if (selection.has_value() && selection->Group.has_value()
					&& selection->Comparison == result->Alignment.Digest) {
					const auto found = std::ranges::find(
						groups, *selection->Group, &core::change_group::Id);
					if (found != groups.end()) {
						ordinal = static_cast<std::size_t>(
							std::ranges::distance(groups.begin(), found) + 1);
					}
				}
				changeText = ordinal > 0U
					? QStringLiteral("change %1 of %2").arg(ordinal).arg(groups.size())
					: groups.empty() ? QStringLiteral("no changes")
									 : QStringLiteral("%1 changes").arg(groups.size());
			}
		}
		ChangePositionLabel->setText(changeText);
	}

	void workbench_window::update_strip_identity()
	{
		if (IdentityLabel == nullptr) {
			return;
		}
		IdentityLabel->setText(IdentityLabel->fontMetrics().elidedText(
			IdentityText, Qt::ElideRight, 360));
		IdentityLabel->setToolTip(IdentityText);
	}

	void workbench_window::update_account_action()
	{
		if (AccountAction == nullptr || SessionPanel == nullptr) {
			return;
		}
		const auto status   = SessionPanel->status();
		const auto signedIn = status == core::github_status::session_ready
			|| status == core::github_status::loading_pull_request
			|| status == core::github_status::pull_request_ready
			|| status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
		const auto account     = QString::fromStdString(SessionPanel->active_account_text());
		const auto description = signedIn
			? QStringLiteral("Signed in as %1. Open the accounts and hosts settings.")
				  .arg(account)
			: QStringLiteral("Sign in with a device code.");
		AccountAction->setText(signedIn ? account : QStringLiteral("Sign in"));
		AccountAction->setToolTip(description);
		AccountAction->setStatusTip(description);
		if (AccountCard != nullptr) {
			AccountCard->set_status(
				signedIn ? QStringLiteral("Connected") : QStringLiteral("Signed out"), signedIn);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(AccountAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Account"));
			button->setAccessibleDescription(description);
			button->setFocusPolicy(Qt::StrongFocus);
		}
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

	auto workbench_window::menu_bar() const noexcept -> QMenuBar*
	{
		return PrimaryMenuBar;
	}

	auto workbench_window::view_menu() const noexcept -> QMenu*
	{
		return ViewMenu;
	}

	auto workbench_window::line_numbers_action() const noexcept -> QAction*
	{
		return LineNumbersAction;
	}

	auto workbench_window::files_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[0].Panel;
	}

	auto workbench_window::files_tree() const noexcept -> files_tree_view*
	{
		return FilesTree;
	}

	auto workbench_window::files_order_selector() const noexcept -> QComboBox*
	{
		return FilesOrderSelector;
	}

	auto workbench_window::files_filter() const noexcept -> QLineEdit*
	{
		return FilesFilter;
	}

	auto workbench_window::central_splitter() const noexcept -> workbench_central_splitter*
	{
		return CentralSplitter;
	}

	auto workbench_window::repositories_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[2].Panel;
	}

	auto workbench_window::repositories_drawer_toggle_action() const noexcept -> QAction*
	{
		return Drawers[2].Toggle;
	}

	auto workbench_window::repositories_tree() const noexcept -> repositories_tree_view*
	{
		return RepositoriesTree;
	}

	auto workbench_window::account_card_view() const noexcept -> account_card*
	{
		return AccountCard;
	}

	auto workbench_window::rules_editor() const noexcept -> QWidget*
	{
		return RulesEditor;
	}

	auto workbench_window::presented_repositories_state() const
		-> core::workbench_drawer_state
	{
		// As the files drawer: shown while the core offers no drawer command,
		// so a reviewer with nothing open can still reach a pull request.
		const auto toggle = command(core::workbench_command_id::toggle_repositories_drawer);
		if (!toggle || !toggle->Enabled) {
			return core::workbench_drawer_state::expanded;
		}
		return DrawerStates.Repositories;
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
			if (AccountCard != nullptr) {
				AccountCard->apply(*aUpdate.Session, QString::fromStdString(SessionPanel->active_host_text()));
			}
			if (ConversationPanel != nullptr) {
				ConversationPanel->cards()->set_viewer_login(
					QString::fromStdString(aUpdate.Session->Login));
			}
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
			// An accepted identity moves the picker with its signals blocked,
			// so the rebind the picker's own change would have performed has to
			// be performed here as well. Both paths end at the same profile.
			rebind_device_authorization_profile();
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
		ReviewThreadTotal = aPullRequest.ReviewThreadCount;
		HomeRequested     = false;
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_pull_request_author(
				QString::fromStdString(aPullRequest.AuthorLogin));
		}
		IdentityText = QStringLiteral("%1/%2  #%3 %4")
						   .arg(QString::fromStdString(aPullRequest.Owner),
								QString::fromStdString(aPullRequest.RepositoryName),
								QString::number(aPullRequest.Number.value()),
								QString::fromStdString(aPullRequest.Title));
		update_strip_identity();
		// The gap BPR-0191 recorded closes here. In the two-window arrangement
		// the anchor could only follow a conversation projection, because nothing
		// observed a comparison context change. The shell composes both surfaces,
		// so it sets the anchor from the file the reviewer is actually reading.
		ConversationPanel->set_anchor_file(conversation_anchor_file());
		ApplyingConversationAnchor = true;
		ConversationPanel->adopt(
			aPullRequest.Conversations,
			detail::review_thread_availability_text(
				aPullRequest.ReviewThreadsAvailable,
				aPullRequest.ReviewThreadCount,
				aPullRequest.Capabilities.ReviewThreads));
		ApplyingConversationAnchor = false;
		synchronize_selected_conversation(
			ConversationPanel->view()->currentIndex());
		refresh_cards();
		update_defect_surface();
		HasGithubPullRequest = true;
		// [[BPR-0651]]. A pull request chosen from the repositories tree
		// opens on its overview, as the design says it does.
		if (OpenOnOverview) {
			OpenOnOverview = false;
			// [[BPR-0661]]. The modality is the reviewer's preference: the
			// overview the design opens on, Read, or the one last used here.
			choose_modality(opening_modality());
		}
		// [[BPR-0661]]. The drawer's first filter and the comparison on return
		// are the reviewer's preferences too; the comparison waits until the
		// history offers it.
		FirstFilterApplied    = false;
		ReturnComparisonAsked = false;
		apply_first_comments_filter();
		ReturnComparisonPending = Behaviour.ReturnComparison
			!= application::review_return_comparison::whole_pr;
		honour_return_comparison();
		SessionPanel->apply_capabilities(aPullRequest.Capabilities);
		SessionPanel->apply_rate_limits(aPullRequest.RateLimits);
		// [[BPR-0657]]. Resolve is offered once the provider and the server
		// both resolve threads; a notice from an earlier gesture is stale.
		Capabilities = aPullRequest.Capabilities;
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_resolution_offered(
				ProviderResolvesThreads
				&& (Capabilities.ResolveThread == core::github_capability_state::supported
					|| Capabilities.UnresolveThread == core::github_capability_state::supported));
			ConversationPanel->present_notice(QString{ });
		}
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
			true,
			aDiagnostic.BootstrapStage, aDiagnostic.CapabilityDetail);
	}

	void workbench_window::clear_github_pull_request()
	{
		HasGithubPullRequest = false;
		ReviewThreadTotal    = 0U;
		IdentityText.clear();
		update_strip_identity();
		update_rail_summaries();
		PullRequestPanel->clear();
		clear_overview();
		ConversationPanel->clear(QStringLiteral(
			"Review threads are unavailable until a pull request is open."));
		refresh_cards();
		update_github_presentation_state();
	}

	void workbench_window::publish(
		const application::public_preview_presentation& aPresentation)
	{
		PublicPreview          = aPresentation;
		PublicPreviewSignedOut = aPresentation.Mode
			== application::public_preview_mode::signed_out;
		if (SessionPanel != nullptr) {
			SessionPanel->apply_public_preview(aPresentation, PublicPreviewNow);
		}
		update_public_preview_title();
		apply_public_preview_conversation();
		// Re-decides the commands from their projections first, then lets the
		// mode have the last word over them.
		update_action_state();
		apply_focus_order();
		update_github_presentation_state();
	}

	void workbench_window::set_public_preview_now(
		const std::uint64_t aNowUnixSeconds)
	{
		PublicPreviewNow = aNowUnixSeconds;
		if (SessionPanel != nullptr) {
			SessionPanel->apply_public_preview(PublicPreview, PublicPreviewNow);
		}
	}

	void workbench_window::update_public_preview_title()
	{
		setWindowTitle(
			PublicPreviewSignedOut
				? QStringLiteral("BetterPullReview — %1")
					  .arg(public_preview_mode_text())
				: QStringLiteral("BetterPullReview"));
	}

	void workbench_window::apply_public_preview_availability()
	{
		const auto restore = [](QWidget* const aWidget) {
			if (aWidget != nullptr) {
				aWidget->setEnabled(true);
				aWidget->setToolTip(QString{ });
			}
		};
		if (!PublicPreviewSignedOut) {
			restore(QueueRoleFilter);
			restore(ReviewEditor);
			restore(
				ConversationPanel != nullptr ? ConversationPanel->reveal_button()
											 : nullptr);
			return;
		}
		// Disabled and explained, never hidden and never defaulted. A control
		// that vanished would leave the reviewer guessing whether the mode
		// cannot do this or the pull request simply has none of it.
		const auto refuseAction
			= [](QAction* const aAction, const QString& aName) {
				  if (aAction == nullptr) {
					  return;
				  }
				  const auto wording = public_preview_unavailable_text(aName);
				  aAction->setEnabled(false);
				  aAction->setToolTip(wording);
				  aAction->setStatusTip(wording);
			  };
		const auto refuseWidget
			= [](QWidget* const aWidget, const QString& aName) {
				  if (aWidget == nullptr) {
					  return;
				  }
				  aWidget->setEnabled(false);
				  aWidget->setToolTip(public_preview_unavailable_text(aName));
			  };
		for (auto* const action : ReceiptActions) {
			refuseAction(action, QStringLiteral("Private review progress"));
		}
		refuseAction(
			ShowReviewThreadsAction,
			QStringLiteral("Review-thread resolution"));
		refuseWidget(
			ConversationPanel != nullptr ? ConversationPanel->reveal_button()
										 : nullptr,
			QStringLiteral("Review-thread resolution"));
		refuseWidget(QueueRoleFilter, QStringLiteral("Queue role"));
		refuseWidget(ReviewEditor, QStringLiteral("Publishing a review"));
	}

	void workbench_window::apply_public_preview_conversation()
	{
		if (!PublicPreviewSignedOut || ConversationPanel == nullptr) {
			return;
		}
		if (PublicPreview.ReviewComments
			!= application::public_preview_observation::observed) {
			// Not an empty conversation. An unobservable conversation and an
			// observed empty one are different answers and the wording keeps
			// them apart.
			ConversationPanel->clear(
				QStringLiteral("Review comments were not observable."));
			return;
		}
		std::vector<application::github_conversation_projection> conversations;
		const auto                                               identifier = [](const std::string& aId) {
			return std::string{ "public-preview-comment:" } + aId;
		};
		const auto project =
			[](const application::public_preview_review_comment& aComment,
			   std::string                                       aId,
			   std::optional<std::string>                        aParent) {
				return application::github_conversation_projection{
					.Id           = std::move(aId),
					.ParentId     = std::move(aParent),
					.Kind         = application::github_conversation_kind::review_comment,
					.AuthorLogin  = aComment.AuthorLogin,
					.Body         = aComment.Body,
					.Path         = aComment.Path,
					.Line         = aComment.Line,
					.OriginalLine = std::nullopt,
					.DiffSide     = std::nullopt,
					// Signed out there is no selected-revision agreement to
					// claim, so the anchor is reported as file level rather
					// than asserted current.
					.AnchorAvailability = application::
						github_conversation_anchor_availability::file_level,
					.ReviewState
					= application::github_review_state_projection::none,
					// Resolution needs a reader and there is none. False here
					// renders nothing at all, which is the absence the mode
					// requires rather than a claim that it is unresolved.
					.Resolved = false,
					.Outdated = false,
					.Deleted  = false,
				};
			};
		for (const auto& group : PublicPreview.ReviewCommentGroups) {
			auto rootId = identifier(group.Root.Id);
			conversations.push_back(project(group.Root, rootId, std::nullopt));
			for (const auto& reply : group.Replies) {
				conversations.push_back(
					project(reply, identifier(reply.Id), rootId));
			}
		}
		ConversationPanel->adopt(
			conversations,
			QStringLiteral(
				"Review comments are read-only in public preview. Review "
				"threads and their resolution are unavailable."));
	}

	auto workbench_window::usage_indicator() const noexcept -> usage_ring*
	{
		return SessionPanel != nullptr ? SessionPanel->usage_indicator()
									   : nullptr;
	}

	auto workbench_window::public_preview_signed_out() const noexcept -> bool
	{
		return PublicPreviewSignedOut;
	}

	auto workbench_window::public_preview_refusal_text() const -> QString
	{
		return SessionPanel != nullptr && SessionPanel->refusal_label() != nullptr
			? SessionPanel->refusal_label()->text()
			: QString{ };
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
		PullRequestPanel->body_editor()->setVisible(HasGithubPullRequest);
		// [[BPR-0646]]. Sign-in is a state of the window: the device panel is
		// shown while there is no accepted session and hidden once there is,
		// so it is reachable without scrolling and absent when it is noise.
		if (DevicePanel != nullptr) {
			const auto status = SessionPanel->status();
			DevicePanel->setVisible(
				status == core::github_status::no_profile
				|| status == core::github_status::establishing_session
				|| status == core::github_status::session_failed
				|| status == core::github_status::session_cancelled);
			// A flow begins from the accounts page itself -- the strip's
			// account button opens it -- so nothing here presents a window;
			// presenting one would take activation from the workbench in the
			// middle of whatever began the flow.
		}
		update_account_action();
		update_home_landing();

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
							  "Choose a host under the account button, then connect."));
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

	auto workbench_window::device_authorization() const noexcept
		-> device_authorization_panel*
	{
		return DevicePanel;
	}

	void workbench_window::rebind_device_authorization_profile()
	{
		if (DevicePanel == nullptr || SessionPanel == nullptr) {
			return;
		}
		if (const auto profile = SessionPanel->selected_profile();
			profile.has_value()) {
			DevicePanel->bind_profile(*profile);
		}
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

	auto workbench_window::side_chat() const noexcept -> side_chat_panel*
	{
		return SideChat;
	}

	auto workbench_window::finding_queue() const noexcept -> finding_queue_view*
	{
		return FindingQueue;
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

	auto workbench_window::home_open_bar() const noexcept -> QLineEdit*
	{
		return HomeOpenBar;
	}

	auto workbench_window::queue_delegate() const noexcept -> review_landing_delegate*
	{
		return QueueDelegate;
	}

	void workbench_window::open_pull_request_item(const core::queue_item& aItem)
	{
		if (!GithubDispatch) {
			return;
		}
		const auto repository = QString::fromStdString(aItem.Ref.Repository);
		const auto slash      = repository.indexOf(QLatin1Char{ '/' });
		if (slash <= 0 || aItem.Ref.Number <= 0) {
			return;
		}
		// [[BPR-0651]] [[BPR-0650]]. A pull request chosen from a list
		// arrives on Overview.
		OpenOnOverview = true;
		GithubDispatch(core::open_pull_request_intent{
			.Repository = core::repository_locator{
				.Owner = repository.left(slash).toStdString(),
				.Name  = repository.mid(slash + 1).toStdString(),
			},
			.Number = core::pull_request_number{ static_cast<std::uint32_t>(aItem.Ref.Number) },
		});
	}

	void workbench_window::open_from_home_bar()
	{
		if (HomeOpenBar == nullptr || !GithubDispatch) {
			return;
		}
		const auto intent = parse_pull_request_locator(HomeOpenBar->text());
		if (!intent.has_value()) {
			return;
		}
		OpenOnOverview = true;
		GithubDispatch(*intent);
		HomeOpenBar->clear();
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

		// The strip's stops in the strip's reading order: files, the change
		// pair, the file pair, the four modalities, the secondary review
		// controls, refresh, comments, account, settings.
		const auto presenter = [this](QAction* const aAction) {
			return qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(aAction));
		};
		append(presenter(HomeAction));
		append(presenter(Drawers[2].Toggle));
		append(presenter(Drawers[0].Toggle));
		append(presenter(Navigation[2].Action));
		append(presenter(Navigation[3].Action));
		append(presenter(Navigation[0].Action));
		append(presenter(Navigation[1].Action));
		if (Modality != nullptr) {
			for (std::size_t index = 0U; index < review_modality_count; ++index) {
				append(Modality->button(static_cast<review_modality>(index)));
			}
		}
		append(presenter(ReviewDetailsAction));
		append(ReviewControlsButton);
		append(presenter(GithubRefreshAction));
		append(presenter(Drawers[1].Toggle));
		append(presenter(AccountAction));
		append(presenter(SettingsAction));
		// The review header sits under the toolbars and above the workbench, so
		// its stops are declared there. Reading order and focus order agree.
		// The session chrome and the device sign-in are the Accounts and
		// hosts page of the Settings window, another top-level window with
		// its own tab chain; the workbench declares nothing across windows.
		append(QueueView);
		append(QueueRoleFilter);
		append(QueueObserverFilter);
		append(QueueBucketFilter);
		append(QueueRepositoryFilter);
		append(QueueQuietFilter);
		append(QueueSort);
		// The files drawer's column, top to bottom: sign-in, the session
		// chrome, the queue, the pull request, then the files list.
		if (PullRequestPanel != nullptr) {
			append(PullRequestPanel->technical_details_button());
			append(PullRequestPanel->body_editor());
		}
		// A collapsed drawer's rail is its one stop; hidden while expanded.
		append(Drawers[2].Panel->rail());
		append(Drawers[2].Panel->focus_stop());
		append(Drawers[0].Panel->rail());
		append(Drawers[0].Panel->focus_stop());
		// The line and its comparing menu first, then the two selectors on
		// one grid row above their two exact OIDs, the row order the composed
		// control already walks.
		if (HomeLanding != nullptr) {
			append(HomeLanding->primary_button());
			append(HomeLanding->secondary_button());
		}
		append(EpochSelectors->line());
		append(EpochSelectors->comparing_button());
		append(EpochSelectors->left_selector());
		append(EpochSelectors->right_selector());
		append(EpochSelectors->left_oid_editor());
		append(EpochSelectors->right_oid_editor());
		// One stop stands for the whole composed comparison surface: its last.
		// Everything before it inside that surface is RFC-BPR-0004's declared
		// order, which the shell reports through that card rather than restating
		// here.
		append(ComparisonSurface->detail_pane());
		append(Drawers[1].Panel->rail());
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

	void workbench_window::set_conversation_endpoint_placement(
		const bool aLeftHasCurrentDiffRevision,
		const bool aRightHasCurrentDiffRevision)
	{
		LeftConversationEndpointCurrent  = aLeftHasCurrentDiffRevision;
		RightConversationEndpointCurrent = aRightHasCurrentDiffRevision;
		if (ConversationPanel == nullptr) {
			return;
		}
		ApplyingConversationAnchor = true;
		ConversationPanel->set_current_endpoint_placement(
			aLeftHasCurrentDiffRevision, aRightHasCurrentDiffRevision);
		ApplyingConversationAnchor = false;
		synchronize_selected_conversation(
			ConversationPanel->view()->currentIndex());
		update_defect_surface();
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

	void workbench_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QMainWindow::resizeEvent(aEvent);
		apply_strip_width();
	}

	void workbench_window::showEvent(QShowEvent* const aEvent)
	{
		QMainWindow::showEvent(aEvent);
		// The native window exists once shown; the title text goes then.
		hide_native_title_text(*this);
	}

	void workbench_window::update_rail_summaries()
	{
		// The rails restate what the drawers hold: the selected file's place
		// among the actionable files, and how many review threads there are.
		// The unread count the design names waits for the projection that
		// carries it; nothing is invented meanwhile.
		auto files = QString{ };
		if (Cursor && Cursor->ActionableCount > 0U) {
			files = Cursor->Ordinal > 0U
				? QStringLiteral("%1 of %2")
					  .arg(Cursor->Ordinal)
					  .arg(Cursor->ActionableCount)
				: QString::number(Cursor->ActionableCount);
		}
		Drawers[0].Panel->rail()->set_summary(files);
		Drawers[1].Panel->rail()->set_summary(
			ReviewThreadTotal > 0U ? QString::number(ReviewThreadTotal)
								   : QString{ });
		// The repositories rail: how many pull requests need me.
		const auto needsMe = RepositoriesTree != nullptr
			? RepositoriesTree->tree_model()->needs_me_count()
			: std::size_t{ 0U };
		Drawers[2].Panel->rail()->set_summary(
			needsMe > 0U ? QString::number(needsMe) : QString{ });
	}

	void workbench_window::infer_modality_from_drawers()
	{
		// A restored layout arrives before any modality was chosen in this
		// window, so the segment shows the preset the drawers already are.
		// Once a modality has been chosen here, a manual toggle leaves the
		// segment alone, as the accepted design requires.
		if (Modality == nullptr || ModalityChosen) {
			return;
		}
		const auto files = DrawerStates.Files == core::workbench_drawer_state::expanded;
		const auto conversations
			= DrawerStates.Conversations == core::workbench_drawer_state::expanded;
		if (files && !conversations) {
			Modality->adopt(review_modality::overview);
		} else if (!files && conversations) {
			if (Modality->current() != review_modality::discuss) {
				Modality->adopt(review_modality::read);
			}
		} else if (files && conversations) {
			Modality->adopt(review_modality::review);
		}
		apply_modality_surface();
	}

	void workbench_window::apply_review_details_visibility()
	{
		if (ReviewDetailsAction == nullptr) {
			return;
		}
		const auto visible = ReviewDetailsAction->isChecked();
		// The epoch line is chrome and stays; the exact anchors, OIDs and
		// history maintenance beneath it and the selected-change detail pane
		// are the review details.
		if (EpochSelectors != nullptr) {
			EpochSelectors->set_details_visible(visible);
		}
		if (ComparisonSurface != nullptr
			&& ComparisonSurface->detail_pane() != nullptr) {
			ComparisonSurface->detail_pane()->setVisible(visible);
		}
		const auto description = visible
			? QStringLiteral(
				  "Hide the exact review anchors and the selected-change details and give their height to the comparison.")
			: QStringLiteral(
				  "Show the exact review anchors and the selected-change details.");
		ReviewDetailsAction->setToolTip(description);
		ReviewDetailsAction->setStatusTip(description);
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(ReviewDetailsAction));
			button != nullptr) {
			button->setAccessibleDescription(description);
		}
	}

	void workbench_window::apply_strip_width()
	{
		// The strip never folds a control into an overflow menu: a control
		// that disappears at one width and returns at another is exactly the
		// chrome the accepted design rejects. What yields first is the text
		// that restates what the panes already show -- the identity, then the
		// two positions -- and it yields before the window's floor is reached.
		if (IdentityAction == nullptr || ChangePositionAction == nullptr
			|| FilePositionAction == nullptr) {
			return;
		}
		const auto available = width();
		// [[BPR-0647]]. 1400, not 1180: with the Home button in the row, a
		// long identity at 1280 pushed the right region into the overflow
		// menu the design forbids (the UIforETW contract's compact row).
		IdentityAction->setVisible(!StripHome && available >= 1'400);
		ChangePositionAction->setVisible(!StripHome && available >= 960);
		FilePositionAction->setVisible(!StripHome && available >= 960);
		if (NextNewCommentAction != nullptr && NewCommentCountAction != nullptr) {
			NextNewCommentAction->setVisible(!StripHome);
			NewCommentCountAction->setVisible(!StripHome && available >= 960);
		}
		if (Modality != nullptr) {
			// The segments keep their names and lose the shortcut beside them.
			Modality->set_compact(available < 1'000);
		}
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

	/// \brief Builds the window's menu bar and its View menu.
	///
	/// \details This window had no menu bar until BPR-0457. The owner asked
	/// for a "view menu option" and was given the choice between a real menu
	/// bar and a checkable entry in the existing toolbar popup; they chose the
	/// menu bar, which is the option that matches the words and, on macOS,
	/// merges into the system bar and costs the window no height. On Windows
	/// and Linux it takes a row, which is the price of the same answer being
	/// true everywhere.
	void workbench_window::build_menu_bar()
	{
		PrimaryMenuBar = new QMenuBar{ this };
		PrimaryMenuBar->setObjectName(QStringLiteral("workbenchMenuBar"));
		PrimaryMenuBar->setAccessibleName(QStringLiteral("Workbench menu bar"));
		PrimaryMenuBar->setAccessibleDescription(
			QStringLiteral("Presentation menus for the review workbench"));
		setMenuBar(PrimaryMenuBar);

		ViewMenu = PrimaryMenuBar->addMenu(QStringLiteral("&View"));
		ViewMenu->setObjectName(QStringLiteral("workbenchViewMenu"));
		ViewMenu->setAccessibleName(QStringLiteral("View"));
		ViewMenu->setAccessibleDescription(
			QStringLiteral("Presentation options for the comparison surface"));

		LineNumbersAction
			= new QAction{ QStringLiteral("&Line numbers"), this };
		LineNumbersAction->setObjectName(
			QStringLiteral("toggleWorkbenchLineNumbersAction"));
		LineNumbersAction->setCheckable(true);
		LineNumbersAction->setChecked(true);
		LineNumbersAction->setShortcut(
			QKeySequence{ QStringLiteral("Alt+Shift+5") });
		LineNumbersAction->setShortcutContext(Qt::WindowShortcut);
		addAction(LineNumbersAction);
		ViewMenu->addAction(LineNumbersAction);
		// `triggered` rather than `toggled`: the checked state is the
		// projection's to decide, and a toggled handler would also fire when
		// the projection sets it, turning one gesture into two.
		connect(LineNumbersAction, &QAction::triggered, this, [this] {
			dispatch_line_numbers_toggle();
		});
		// [[BPR-0646]]. Settings is a window, reached by the platform's
		// preferences shortcut; on macOS the role places it in the app menu.
		SettingsAction = new QAction{ QStringLiteral("&Settings…"), this };
		SettingsAction->setObjectName(QStringLiteral("openSettingsAction"));
		SettingsAction->setMenuRole(QAction::PreferencesRole);
		SettingsAction->setShortcut(QKeySequence::Preferences);
		SettingsAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(SettingsAction);
		ViewMenu->addSeparator();
		ViewMenu->addAction(SettingsAction);
		connect(SettingsAction, &QAction::triggered, this, [this] {
			open_settings(settings_page::appearance);
		});
	}

	void workbench_window::open_settings(const settings_page aPage)
	{
		if (SettingsWindow != nullptr) {
			SettingsWindow->present(aPage);
		}
	}

	auto workbench_window::settings() const noexcept -> settings_window*
	{
		return SettingsWindow;
	}

	auto workbench_window::settings_action() const noexcept -> QAction*
	{
		return SettingsAction;
	}

	void workbench_window::dispatch_line_numbers_toggle()
	{
		// The widget never decides. Whatever Qt just did to the check mark is
		// undone by re-applying the projection, and the intent is what
		// eventually changes it.
		apply_line_numbers();
		if (!Dispatch) {
			return;
		}
		Dispatch(core::toggle_workbench_line_numbers_intent{ });
	}

	void workbench_window::apply_line_numbers()
	{
		const auto shown
			= LineNumberState.State == core::workbench_line_numbers::shown;
		LineNumbersAction->setChecked(shown);
		const auto projection
			= command(core::workbench_command_id::toggle_line_numbers);
		describe_action(
			LineNumbersAction,
			projection && projection->Enabled,
			shown
				? QStringLiteral(
					  "Hide the source line numbers and give the gutter's width "
					  "back to the code.")
				: QStringLiteral("Number each pane's own source lines."),
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_line_numbers_visible(shown);
		}
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
		const auto drawer = [aIndex] {
			switch (aIndex) {
			case 0U:
				return core::workbench_drawer::files;
			case 1U:
				return core::workbench_drawer::conversations;
			default:
				return core::workbench_drawer::repositories;
			}
		}();
		Dispatch(core::toggle_workbench_drawer_intent{ .Drawer = drawer });
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
		// The scroll column can be wider than a manually narrowed drawer.
		// Keep the exact review identity and controls inside its visible width.
		if (aDrawer == core::workbench_drawer::conversations && ReviewSubmissionEditor != nullptr) {
			ReviewSubmissionEditor->setMaximumWidth(std::max(1, Drawers[1].Panel->contentsRect().width()));
		}

		// Geometry this window just applied is not a user resize. Reporting it
		// would let a width the window could merely grant overwrite the width the
		// host persisted.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		auto& retained = [this, aDrawer]() -> std::uint32_t& {
			switch (aDrawer) {
			case core::workbench_drawer::files:
				return LayoutWidths->FilesWidth;
			case core::workbench_drawer::conversations:
				return LayoutWidths->ConversationsWidth;
			case core::workbench_drawer::repositories:
				return LayoutWidths->RepositoriesWidth;
			}
			return LayoutWidths->FilesWidth;
		}();
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
		Drawers[0].Panel->adopt(presented_files_state());
		Drawers[1].Panel->adopt(DrawerStates.Conversations);
		Drawers[2].Panel->adopt(presented_repositories_state());
		ApplyingLayout = false;
		apply_drawer_widths();
	}

	void workbench_window::apply_drawer_widths()
	{
		if (!LayoutWidths || ApplyingLayout) {
			return;
		}
		ApplyingLayout = true;
		// A layout from before the repositories drawer carries no width
		// for it. The design's 300 is what the window grants, echoed here
		// so the grant is not reported back as a resize.
		if (LayoutWidths->RepositoriesWidth == 0U) {
			LayoutWidths->RepositoriesWidth = 300U;
		}
		CentralSplitter->setSizes(central_sizes());
		ApplyingLayout = false;
	}

	void workbench_window::observe_handle_drag()
	{
		// [[BPR-0662]]. The handle moved under the reviewer's hold: each
		// expanded drawer's width is now theirs, and the host persists it.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		const auto sizes = CentralSplitter->sizes();
		if (sizes.size() != 4) {
			return;
		}
		const auto report = [this, &sizes](const int aIndex, const core::workbench_drawer aDrawer,
										   const core::workbench_drawer_state aState) {
			if (aState == core::workbench_drawer_state::expanded && sizes[aIndex] > 0) {
				observe_drawer_width(aDrawer, static_cast<std::uint32_t>(sizes[aIndex]));
			}
		};
		report(0, core::workbench_drawer::repositories, presented_repositories_state());
		report(1, core::workbench_drawer::files, presented_files_state());
		report(3, core::workbench_drawer::conversations, DrawerStates.Conversations);
	}

	workbench_central_splitter::workbench_central_splitter(QWidget* const aParent)
		: QSplitter{ Qt::Horizontal, aParent }
	{
	}

	void workbench_central_splitter::move_handle(const int aIndex, const int aPosition)
	{
		moveSplitter(aPosition, aIndex);
	}

	auto workbench_window::presented_files_state() const
		-> core::workbench_drawer_state
	{
		// The core decides the drawer while it offers the drawer command. When
		// it offers none -- no workbench is active -- the files drawer holds
		// the sign-in, the host chrome and the queue that would make one, so it
		// presents expanded: a rail the reviewer could not reopen would shut
		// the way in. The core's state is untouched and returns with the
		// command.
		const auto toggle = command(core::workbench_command_id::toggle_files_drawer);
		if (!toggle || !toggle->Enabled) {
			return core::workbench_drawer_state::expanded;
		}
		return DrawerStates.Files;
	}

	auto workbench_window::central_sizes() const -> QList<int>
	{
		const auto granted = [](const std::uint32_t aWidth, const std::uint32_t aDefault) {
			// An arithmetic guard on a host-owned value, not a second clamp: the
			// host already refuses to retain a width outside its own bound. A
			// width beneath the presented floor is a squeeze that was written
			// back, not a choice; the drawer opens at its design width.
			if (aWidth < minimum_presented_drawer_width) {
				return static_cast<int>(aDefault);
			}
			return static_cast<int>(
				std::min(aWidth, application::maximum_workbench_drawer_width));
		};
		const auto defaults = application::workbench_layout_settings{ };
		// A collapsed drawer is its rail, so the rail's width is what it keeps.
		const auto files = presented_files_state() == core::workbench_drawer_state::expanded
			? granted(LayoutWidths->FilesWidth, defaults.FilesWidth)
			: workbench_drawer_rail::width_pixels;
		// [[BPR-0649]]. In Discuss the conversation is at least its Discuss
		// width; the persisted width is not changed by it.
		const auto discuss = Modality != nullptr && Modality->current() == review_modality::discuss;
		const auto conversations
			= DrawerStates.Conversations == core::workbench_drawer_state::expanded
			? std::max(granted(LayoutWidths->ConversationsWidth, conversation_drawer_default_width),
					   discuss ? discuss_conversation_width : 0)
			: workbench_drawer_rail::width_pixels;
		const auto repositories
			= presented_repositories_state() == core::workbench_drawer_state::expanded
			? granted(LayoutWidths->RepositoriesWidth, defaults.RepositoriesWidth)
			: workbench_drawer_rail::width_pixels;
		const auto handles = 3 * CentralSplitter->handleWidth();
		const auto centre  = std::max(
			1, CentralSplitter->width() - repositories - files - conversations - handles);
		return { repositories, files, centre, conversations };
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
		Strip->adopt_button(ReviewControlsButton);
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(GithubRefreshAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Refresh"));
			button->setAccessibleDescription(QStringLiteral(
				"Reload the open pull request from its host."));
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(HomeAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Home"));
			button->setAccessibleDescription(HomeAction->toolTip());
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(SettingsAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Settings"));
			button->setAccessibleDescription(QStringLiteral(
				"Open the Settings window."));
			button->setFocusPolicy(Qt::StrongFocus);
		}
		update_account_action();
		update_home_landing();
	}

	void workbench_window::refresh_toolbar_icons()
	{
		auto* const nativeStyle = style();
		if (nativeStyle == nullptr) {
			return;
		}
		// [[BPR-0646]]. The strip's glyphs are painted in the live palette's
		// ink, so a theme change repaints them here exactly as the arrows were.
		const auto& ink = palette();
		if (HomeAction != nullptr) {
			HomeAction->setIcon(strip_icon(strip_glyph::home, ink));
		}
		Drawers[0].Toggle->setIcon(strip_icon(strip_glyph::files_drawer, ink));
		Drawers[2].Toggle->setIcon(strip_icon(strip_glyph::repositories_drawer, ink));
		Navigation[0].Action->setIcon(strip_icon(strip_glyph::previous_file, ink));
		Navigation[1].Action->setIcon(strip_icon(strip_glyph::next_file, ink));
		Navigation[2].Action->setIcon(strip_icon(strip_glyph::previous_change, ink));
		Navigation[3].Action->setIcon(strip_icon(strip_glyph::next_change, ink));
		if (NextNewCommentAction != nullptr) {
			NextNewCommentAction->setIcon(strip_icon(strip_glyph::new_comment, ink));
		}
		ReviewDetailsAction->setIcon(strip_icon(strip_glyph::review_details, ink));
		ReviewControlsButton->setIcon(strip_icon(strip_glyph::private_progress, ink));
		Drawers[1].Toggle->setIcon(strip_icon(strip_glyph::comments_drawer, ink));
		if (GithubRefreshAction != nullptr) {
			GithubRefreshAction->setIcon(strip_icon(strip_glyph::refresh, ink));
		}
		if (AccountAction != nullptr) {
			AccountAction->setIcon(strip_icon(strip_glyph::account, ink));
		}
		if (SettingsAction != nullptr) {
			SettingsAction->setIcon(strip_icon(strip_glyph::settings, ink));
		}

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
		// The presented files state depends on whether the command is offered.
		if (Drawers[0].Panel->state() != presented_files_state()) {
			apply_drawer_layout();
		}
		update_drawer_action(
			1U,
			core::workbench_command_id::toggle_conversations_drawer,
			DrawerStates.Conversations);
		update_drawer_action(
			2U,
			core::workbench_command_id::toggle_repositories_drawer,
			DrawerStates.Repositories);
		if (Drawers[2].Panel->state() != presented_repositories_state()) {
			apply_drawer_layout();
		}
		apply_line_numbers();
		update_receipt_actions();
		// Last, and deliberately. The mode is not a second opinion about a
		// command's legality; it is the answer that stands once the core's
		// projections have had theirs.
		apply_public_preview_availability();
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
		update_strip_positions();
		update_rail_summaries();
		update_home_landing();
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
