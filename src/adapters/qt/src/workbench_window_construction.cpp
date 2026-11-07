#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/draft_state_chip.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/identity_avatar_pictures.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/menu_tool_button.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "conversation_glyphs.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QChar>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFlags>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QMenu>
#include <QMetaObject>
#include <QObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStatusBar>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace bpr::qt {
	auto editor_keeps_typed_text(const QObject* const aReceiver, const QKeyEvent& aEvent) -> bool
	{
		const auto altGr = Qt::ControlModifier | Qt::AltModifier;
		if ((aEvent.modifiers() & altGr) != altGr || aEvent.text().isEmpty() || !aEvent.text().front().isPrint()) {
			return false;
		}
		if (const auto* const line = qobject_cast<const QLineEdit*>(aReceiver); line != nullptr) {
			return !line->isReadOnly();
		}
		return editable_multi_line(aReceiver);
	}

	workbench_window::workbench_window(
		workbench_intent_dispatch                       aDispatch,
		workbench_change_dispatch                       aChangeDispatch,
		workbench_drawer_width_dispatch                 aWidthDispatch,
		std::vector<application::github_profile_option> aProfiles,
		application::github_intent_dispatch             aGithubDispatch,
		QWidget* const                                  aParent,
		std::shared_ptr<dif_catalog_provider>           aDifCatalog)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, ChangeDispatch{ std::move(aChangeDispatch) }
		, WidthDispatch{ std::move(aWidthDispatch) }
		, DifCatalog{ std::move(aDifCatalog) }
		, GithubProfiles{ std::move(aProfiles) }
		, GithubDispatch{ std::move(aGithubDispatch) }
	{
		DifCues     = std::make_unique<dif_cue_router>();
		DifCueClock = std::make_unique<QElapsedTimer>();
		DifCueClock->start();
		setObjectName(QStringLiteral("workbenchWindow"));
		// [[BPR-1807]]. Before any face is drawn, so every one finds it.
		AvatarPictures = new qt::identity_avatar_pictures{ this };
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
		// [[BPR-1255]]. A larger font can raise this to the review bar's fully
		// yielded width (see apply_review_bar_width).
		setMinimumSize(platform_floor_width, 360);
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
		// [[BPR-1357]]. What was just done is said in Home's band too, for as
		// long as the status bar says it.
		connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString&) { update_home_status(); });
		// [[BPR-1381]]. The band is docked just above the status bar, so while
		// it says the status bar's message the status bar does not draw it a
		// second time beneath (see eventFilter).
		statusBar()->installEventFilter(this);
		// [[BPR-0775]] mandate r2. Dif rests in the window's bottom-left corner
		// while Home is not the first-launch card. This slot reserves the start
		// of the status line for him; the sprite is laid over it (see
		// place_dif_corner), so the status bar keeps its height.
		DifCornerSlot = new QWidget{ this };
		DifCornerSlot->setObjectName(QStringLiteral("workbenchDifCornerSlot"));
		DifCornerSlot->setAccessibleName(QStringLiteral("Decorative Dif illustration"));
		DifCornerSlot->setFixedWidth(dif_corner_size + 8);
		DifCornerSlot->hide();
		DifCornerSlot->installEventFilter(this);
		statusBar()->insertWidget(0, DifCornerSlot, 0);
		// A message shown before Home arrives must not be painted over either
		// (review of BPR-0775, finding 1).
		connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString&) { update_dif_presentation(); });

		configure_tool_buttons();
		refresh_toolbar_icons();
		apply_focus_order();
		update_action_state();
		update_status_text();
	}

	workbench_window::~workbench_window()
	{
		// Child settings windows can change focus while QWidget tears down the
		// already-deleted menu actions. Stop application focus ingress first.
		disconnect(qApp, &QApplication::focusChanged, this, nullptr);
		unmount_settings();
	}

	void workbench_window::build_primary_toolbar()
	{
		// [[BPR-0646]]. RFC-BPR-0028's strip: one 30-pixel row in full ink
		// with bordered buttons. It stays the window's one toolbar so every
		// action keeps its presenter, shortcut and place in the declared focus
		// order. [[BPR-1129]]: RFC-BPR-0052 makes it the calm title row, and
		// what acts on the review moves to the review bar beneath it.
		Strip = new workbench_strip{ QStringLiteral("Title row"), this };
		addToolBar(Strip);
		PrimaryToolbar = Strip;
		PrimaryToolbar->setObjectName(QStringLiteral("workbenchPrimaryToolbar"));
		PrimaryToolbar->setAccessibleName(QStringLiteral("Title row"));
		PrimaryToolbar->setAccessibleDescription(QStringLiteral(
			"Home, the drawer toggles, the pull request, the review modality, "
			"refresh, comments, account and settings"));

		const auto makeDrawer = [this](const QString&             aText,
									   const QString&             aName,
									   const QString&             aAccessible,
									   const QString&             aObjectName,
									   const QString&             aCollapse,
									   const QString&             aExpand,
									   const QList<QKeySequence>& aShortcuts) -> drawer_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setCheckable(true);
			// [[BPR-1678]]. Every catalogue key: Files and Repositories
			// have a second, command-modifier key that acts while typing.
			action->setShortcuts(aShortcuts);
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
			shortcut_keys(shortcut_command::files_drawer));
		Drawers[1] = makeDrawer(
			// [[BPR-1293]]. The drawer is Comments, as its tab and title-row
			// toggle say; only the modality is called Threads ([[BPR-1341]]).
			QStringLiteral("&Comments drawer"),
			QStringLiteral("Comments drawer"),
			QStringLiteral("Comments drawer toggle"),
			QStringLiteral("toggleWorkbenchConversationsDrawerAction"),
			QStringLiteral("Hide the comments drawer and give its width to the comparison."),
			QStringLiteral("Show the comments drawer at its saved width."),
			shortcut_keys(shortcut_command::comments_drawer));
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
			shortcut_keys(shortcut_command::repositories_drawer));

		const auto make = [this](const QString&             aText,
								 const QString&             aName,
								 const QString&             aAccessible,
								 const QString&             aObjectName,
								 const QString&             aPurpose,
								 const QList<QKeySequence>& aShortcuts) -> navigation_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setShortcuts(aShortcuts);
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
							 shortcut_keys(shortcut_command::previous_file));
		Navigation[1] = make(QStringLiteral("&Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("nextWorkbenchFileAction"),
							 QStringLiteral("Select the next actionable file."),
							 shortcut_keys(shortcut_command::next_file));
		Navigation[2] = make(QStringLiteral("Pre&vious change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("previousWorkbenchChangeAction"),
							 QStringLiteral("Select the previous semantic change."),
							 // [[BPR-1328]]. Option-Up (Alt+Up) first, which the
							 // tooltip names; Option-Page Up stays as before.
							 shortcut_keys(shortcut_command::previous_change));
		Navigation[3] = make(QStringLiteral("Ne&xt change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("nextWorkbenchChangeAction"),
							 QStringLiteral("Select the next semantic change."),
							 shortcut_keys(shortcut_command::next_change));

		// The native menu and toolbar share each retained navigation action.
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			ViewMenu->insertAction(HeaderSourceAction, Navigation[index].Action);
			connect(Navigation[index].Action,
					&QAction::triggered,
					this,
					[this, index] { dispatch_navigation(index); });
		}
		// [[BPR-1532]]. First and Last change, after Next change. Menu and
		// keys only: the review bar keeps its four arrows. R and H are the
		// View menu's unused mnemonics.
		ChangeEnds[0] = make(QStringLiteral("Fi&rst change"),
							 QStringLiteral("First change"),
							 QStringLiteral("First change"),
							 QStringLiteral("firstWorkbenchChangeAction"),
							 QStringLiteral("Select the first change in this file."),
							 { change_end_keys()[0] });
		ChangeEnds[1] = make(QStringLiteral("Last c&hange"),
							 QStringLiteral("Last change"),
							 QStringLiteral("Last change"),
							 QStringLiteral("lastWorkbenchChangeAction"),
							 QStringLiteral("Select the last change in this file."),
							 { change_end_keys()[1] });
		for (const auto& [entry, id] : { std::pair{ &ChangeEnds[0], core::workbench_command_id::first_change }, std::pair{ &ChangeEnds[1], core::workbench_command_id::last_change } }) {
			ViewMenu->insertAction(HeaderSourceAction, entry->Action);
			connect(entry->Action, &QAction::triggered, this, [this, id] { dispatch_navigation(static_cast<std::size_t>(id)); });
		}
		connect(qApp, &QApplication::focusChanged, this, [this] { update_change_end_keys(); });
		ViewMenu->insertSeparator(HeaderSourceAction);
		// [[BPR-1287]]. Open first file is Next file's gesture, so it is
		// offered exactly when Next file is.
		connect(Navigation[1].Action, &QAction::changed, this, [this] {
			if (ComparisonSurface != nullptr) {
				ComparisonSurface->idle_open_first_button()->setEnabled(Navigation[1].Action->isEnabled());
			}
		});

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
		ReviewDetailsAction->setShortcuts(shortcut_keys(shortcut_command::review_details));
		ReviewDetailsAction->setShortcutContext(Qt::WindowShortcut);
		ReviewDetailsAction->setToolTip(QStringLiteral(
			"Show the exact review anchors."));
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
		ReceiptActions[2]->setShortcutContext(Qt::WindowShortcut);
		ReceiptActions[2]->setAutoRepeat(false);
		ReceiptActions[2]->setShortcutVisibleInContextMenu(true);
		receiptAction(3U,
					  QStringLiteral("E&xclude"),
					  QStringLiteral("toggleSelectedFileExclusionAction"),
					  QStringLiteral("Change private exclusion for the selected file."));

		ReviewControlsButton = new menu_tool_button{ this };
		ReviewControlsButton->setObjectName(QStringLiteral("workbenchReviewControlsButton"));
		ReviewControlsButton->setText(QStringLiteral("Private &progress"));
		ReviewControlsButton->setShortcut(shortcut_key(shortcut_command::private_progress));
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
			open_settings(settings_page::accounts_and_hosts);
			// [[BPR-0773]] D5. Signed out, the account button is the chosen
			// server's own Sign in: the card that offers the device flow and
			// shows Sign in is pressed, exactly as the reviewer would press it.
			// Anything else only presents the page.
			if (auto* const card = current_server_card();
				card != nullptr && card->offers_device_sign_in()
				&& card->sign_in_button()->isVisibleTo(card)) {
				card->sign_in_button()->click();
			}
		});

		const auto rowLabel = [](QWidget* const aRow,
								 const QString& aObjectName,
								 const QString& aAccessible,
								 const QString& aDescription) {
			auto* const label = new QLabel{ aRow };
			label->setObjectName(aObjectName);
			label->setAccessibleName(aAccessible);
			label->setAccessibleDescription(aDescription);
			label->setTextFormat(Qt::PlainText);
			label->setFocusPolicy(Qt::NoFocus);
			label->setContentsMargins(2, 0, 4, 0);
			return label;
		};
		const auto rowWidget = [](QToolBar* const aRow, QWidget* const aWidget, const QString& aActionName) {
			auto* const action = aRow->addWidget(aWidget);
			action->setObjectName(aActionName);
			return action;
		};
		const auto rowSpacer = [&rowWidget](QToolBar* const aRow, const QString& aObjectName, const QString& aActionName) {
			auto* const spacer = new QWidget{ aRow };
			spacer->setObjectName(aObjectName);
			spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
			spacer->setFocusPolicy(Qt::NoFocus);
			rowWidget(aRow, spacer, aActionName);
			return spacer;
		};

		// [[BPR-1129]]. RFC-BPR-0052's title row carries the window, the pull
		// request and the modality, in this exact persistent order: the
		// platform inset, Home, the repositories and files toggles, the pull
		// request's name, then at the right the modality, refresh (inserted
		// once the session chrome has made it), the comments toggle with the
		// new-comment count, a rule, account and settings. No navigation,
		// progress or composition control sits in it; they are the review
		// bar's, so the name has the room they took.
		// The platform inset comes first: on macOS the width the three window
		// buttons occupy plus the gap Fork and Visual Studio Code leave before
		// their first control; nothing elsewhere ([[BPR-1596]]: one named value
		// per platform, title_row_leading_inset). A widget item rather than a
		// margin, because the toolbar layout lays its items from its own edge.
		StripInset = new QWidget{ PrimaryToolbar };
		StripInset->setObjectName(QStringLiteral("workbenchStripInset"));
		StripInset->setFocusPolicy(Qt::NoFocus);
		StripInset->setFixedWidth(title_row_leading_inset(current_title_row_platform()));
		rowWidget(PrimaryToolbar, StripInset, QStringLiteral("workbenchStripInsetAction"));
#if defined(_WIN32)
		// [[BPR-0653]]. The Windows form of the title row, after Visual Studio
		// Code: no menu bar row, but one compact menu control first in the
		// strip that opens the same menus the window builds (File, Profiles,
		// Pull Request and View), read from the menu bar each time so a menu
		// added later is there too. Its description names them from the bar
		// for the same reason.
		StripMenuButton = new QToolButton{ PrimaryToolbar };
		StripMenuButton->setObjectName(QStringLiteral("workbenchStripMenuButton"));
		StripMenuButton->setAccessibleName(QStringLiteral("Application menu"));
		{
			auto menus = QStringList{ };
			for (auto* const action : PrimaryMenuBar->actions()) {
				if (action->menu() != nullptr) {
					menus.push_back(action->menu()->accessibleName());
				}
			}
			const auto last = menus.isEmpty() ? QString{ } : menus.takeLast();
			StripMenuButton->setAccessibleDescription(
				menus.isEmpty() ? QStringLiteral("The %1 menu").arg(last)
								: QStringLiteral("The %1 and %2 menus").arg(menus.join(QStringLiteral(", ")), last));
		}
		StripMenuButton->setToolTip(QStringLiteral("Menu"));
		StripMenuButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		StripMenuButton->setFocusPolicy(Qt::StrongFocus);
		StripMenuButton->setIcon(windows_menu_icon(palette()));
		connect(StripMenuButton, &QToolButton::clicked, this, &workbench_window::open_strip_menu);
		rowWidget(PrimaryToolbar, StripMenuButton, QStringLiteral("workbenchStripMenuAction"));
#endif
		// [[BPR-0650]]. Home first: the way back to the landing and the queue
		// from any review, checked while Home shows. [[BPR-1535]]: Command-Shift-H
		// (Ctrl+Shift+H), as Safari's and Finder's Home; Command-0 is Actual Size.
		HomeAction = new QAction{ QStringLiteral("&Home"), this };
		HomeAction->setObjectName(QStringLiteral("workbenchHomeAction"));
		HomeAction->setCheckable(true);
		HomeAction->setShortcuts(shortcut_keys(shortcut_command::home));
		HomeAction->setShortcutContext(Qt::WindowShortcut);
		HomeAction->setIconText(QStringLiteral("Home"));
		HomeAction->setToolTip(QStringLiteral(
								   "Show Home (%1): sign-in or the review queue, and the way back to the review.")
								   .arg(HomeAction->shortcut().toString(QKeySequence::NativeText)));
		HomeAction->setStatusTip(HomeAction->toolTip());
		addAction(HomeAction);
		connect(HomeAction, &QAction::triggered, this, [this](const bool aChecked) {
			choose_home(aChecked);
		});
		PrimaryToolbar->addAction(HomeAction);
		PrimaryToolbar->addAction(Drawers[2].Toggle);
		PrimaryToolbar->addAction(Drawers[0].Toggle);
		IdentityLabel = rowLabel(
			PrimaryToolbar,
			QStringLiteral("workbenchStripIdentity"),
			QStringLiteral("Open pull request"),
			QStringLiteral("Repository, number and title of the open pull request"));
		IdentityLabel->setContentsMargins(4, 0, 4, 0);
		IdentityAction = rowWidget(
			PrimaryToolbar, IdentityLabel, QStringLiteral("workbenchStripIdentityAction"));
		// [[BPR-1852]]. A draft says so right after its name, before Open on
		// GitHub, in every modality: the owner could barely tell their pull
		// request was a draft. It is measured with the row's other items, so
		// the title elides around it.
		StripDraftChip = new draft_state_chip{ PrimaryToolbar };
		StripDraftChip->setObjectName(QStringLiteral("workbenchStripDraftChip"));
		StripDraftChipAction = rowWidget(
			PrimaryToolbar, StripDraftChip, QStringLiteral("workbenchStripDraftChipAction"));
		StripDraftChipAction->setVisible(false);
		// [[BPR-1338]]. Open on GitHub, the owner's "open in browser button next
		// to the pr title". The window's action names a pull request and the
		// host admits it and builds the address; Qt never holds one. Its button
		// sits right after the name and goes with it: counted in the name's
		// room, so the title elides around it, and shown exactly while the name
		// of a GitHub pull request is. It is borderless, as the modality's
		// segments are, so it reads as part of the name rather than as another
		// control in the row.
		OpenOnGithubAction = new QAction{ tr("Open on &GitHub"), this };
		OpenOnGithubAction->setObjectName(QStringLiteral("openOnGithubAction"));
		OpenOnGithubAction->setShortcuts(shortcut_keys(shortcut_command::open_on_github));
		OpenOnGithubAction->setShortcutContext(Qt::WindowShortcut);
		OpenOnGithubAction->setIconText(QStringLiteral("Open on GitHub"));
		OpenOnGithubAction->setToolTip(QStringLiteral("Open on GitHub (%1)").arg(OpenOnGithubAction->shortcut().toString(QKeySequence::NativeText)));
		OpenOnGithubAction->setStatusTip(QStringLiteral("Open the pull request's page on GitHub in your browser"));
		OpenOnGithubAction->setEnabled(false);
		addAction(OpenOnGithubAction);
		connect(OpenOnGithubAction, &QAction::triggered, this, [this] {
			if (const auto target = pull_request_page_target(); target.has_value()) {
				open_pull_request_page(*target);
			}
		});
		PullRequestMenu->insertAction(FollowPullRequest, OpenOnGithubAction);
		// [[BPR-1852]]. The author's draft-state change, where the menu bar
		// keeps what acts on the pull request: Mark Ready for Review on a
		// draft, Convert to Draft otherwise. Shown only while it is offered.
		DraftStateAction = new QAction{ tr("Mark &Ready for Review"), this };
		DraftStateAction->setObjectName(QStringLiteral("draftStateAction"));
		DraftStateAction->setStatusTip(QStringLiteral("Mark this draft ready for review on GitHub"));
		DraftStateAction->setVisible(false);
		DraftStateAction->setEnabled(false);
		connect(DraftStateAction, &QAction::triggered, this, [this] {
			if (DraftStateOffer.has_value() && DraftStateDispatch) {
				DraftStateDispatch(*DraftStateOffer);
			}
		});
		PullRequestMenu->insertAction(FollowPullRequest, DraftStateAction);
		PullRequestMenu->insertSeparator(FollowPullRequest);
		OpenOnGithubButton = new QToolButton{ PrimaryToolbar };
		OpenOnGithubButton->setObjectName(QStringLiteral("workbenchOpenOnGithubButton"));
		OpenOnGithubButton->setDefaultAction(OpenOnGithubAction);
		OpenOnGithubButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		OpenOnGithubButton->setAccessibleName(QStringLiteral("Open on GitHub"));
		OpenOnGithubButton->setAccessibleDescription(QStringLiteral("Opens the open pull request's page on GitHub in your browser"));
		OpenOnGithubButton->setProperty(strip_style::filled_property, true);
		Strip->adopt_button(OpenOnGithubButton);
		OpenOnGithubButtonAction = rowWidget(
			PrimaryToolbar, OpenOnGithubButton, QStringLiteral("workbenchOpenOnGithubAction"));
		// [[BPR-0656]]. Next new comment keeps its key and its window
		// ownership; [[BPR-1129]] its count rides on the comments toggle and its
		// presenter leaves the title row.
		NextNewCommentAction = new QAction{ QStringLiteral("Next new &comment"), this };
		NextNewCommentAction->setObjectName(QStringLiteral("nextNewCommentAction"));
		NextNewCommentAction->setIconText(QStringLiteral("Next new comment"));
		NextNewCommentAction->setToolTip(QStringLiteral("Next new comment"));
		NextNewCommentAction->setStatusTip(QStringLiteral(
			"Reveal the next thread with a post newer than what you have read."));
		NextNewCommentAction->setShortcuts(shortcut_keys(shortcut_command::next_new_comment));
		NextNewCommentAction->setShortcutContext(Qt::WindowShortcut);
		NextNewCommentAction->setEnabled(false);
		addAction(NextNewCommentAction);
		connect(NextNewCommentAction, &QAction::triggered, this, [this] { step_to_next_new_comment(); });

		ToolbarSpacer = rowSpacer(
			PrimaryToolbar,
			QStringLiteral("workbenchNavigationSpacer"),
			QStringLiteral("workbenchNavigationSpacerAction"));
		Modality = new modality_control{ *Strip, PrimaryToolbar };
		// [[BPR-1295]]. The strip's button and key choose; choosing Review
		// while it shows is the way back to its preset.
		Modality->set_dispatch([this](const review_modality aModality) {
			choose_modality(aModality, core::workbench_modality_entry::choose);
		});
		rowWidget(PrimaryToolbar, Modality, QStringLiteral("workbenchModalityAction"));
		PrimaryToolbar->addAction(Drawers[1].Toggle);
		// [[BPR-1295]]. Hide Drawers ends the drawer controls, right of the
		// Comments toggle and before the rule that sets off Account and
		// Settings: away from the modality segment, beside the drawer it
		// hides most often.
		PrimaryToolbar->addAction(ToggleDrawersAction);
		if (auto* const button = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(ToggleDrawersAction));
			button != nullptr) {
			button->setObjectName(QStringLiteral("workbenchToggleDrawersButton"));
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		rowWidget(
			PrimaryToolbar,
			new strip_divider{ PrimaryToolbar },
			QStringLiteral("workbenchStripRightDividerAction"));
		PrimaryToolbar->addAction(AccountAction);
		PrimaryToolbar->addAction(SettingsAction);
#if defined(_WIN32)
		// [[BPR-0653]]. The caption buttons end the row, after settings. The
		// host dispatches their presses and reports the window's state.
		CaptionButtons = new windows_caption_buttons{ PrimaryToolbar };
		rowWidget(PrimaryToolbar, CaptionButtons->reserve(), QStringLiteral("workbenchCaptionButtonsAction"));
		// The menu bar's row is the strip's menu control on Windows. The bar
		// stays the menus' owner, hidden, so the Quit shortcut it carried is
		// the window's to keep live.
		PrimaryMenuBar->setVisible(false);
		addAction(QuitProfilesAction);
#endif

		// [[BPR-1129]]. RFC-BPR-0052's review bar: full width under the title
		// row and always shown with a review, in this exact order. From its
		// left, Updates and the comparison it names with its dates and menu (the
		// epoch control hands its "Comparing" button over once it exists), the
		// file label and pair, the change label and pair and
		// private progress; at its right Compose review and Comment. It is an
		// ordinary row: its bare surface drags nothing.
		ReviewBar = new workbench_strip{ QStringLiteral("Review bar"), this, strip_role::review_bar };
		// [[BPR-1255]]. Its own font or style can move the window's floor.
		ReviewBar->installEventFilter(this);
		ReviewBar->setObjectName(QStringLiteral("workbenchReviewBar"));
		ReviewBar->setAccessibleName(QStringLiteral("Review bar"));
		ReviewBar->setAccessibleDescription(QStringLiteral(
			"Updates and the comparison, file and change navigation, private "
			"progress, the layout and header/source, Compose review and Comment"));
		auto* const barInset = new QWidget{ ReviewBar };
		barInset->setObjectName(QStringLiteral("reviewBarInset"));
		barInset->setFocusPolicy(Qt::NoFocus);
		barInset->setFixedWidth(5);
		rowWidget(ReviewBar, barInset, QStringLiteral("reviewBarInsetAction"));
		// [[BPR-1130]]. The Updates disclosure folds the timeline below the bar
		// away and opens it again, and the bar never moves. The core decides the
		// fold; the button shows it and sends the gesture.
		UpdatesDisclosure = new updates_disclosure{ ReviewBar };
		UpdatesDisclosure->setText(QStringLiteral("Updates"));
		UpdatesDisclosure->setAccessibleDescription(QStringLiteral(
			"Folds or opens the Updates line, the pull request's updates, whose timeline runs below this bar"));
		connect(UpdatesDisclosure, &QToolButton::clicked, this, [this] { dispatch_updates_line_toggle(); });
		rowWidget(ReviewBar, UpdatesDisclosure, QStringLiteral("reviewBarUpdatesAction"));
		FilePositionLabel = rowLabel(
			ReviewBar,
			QStringLiteral("workbenchStripFilePosition"),
			QStringLiteral("File position"),
			QStringLiteral("The selected file's position among the actionable files"));
		// [[BPR-1519]]. Keep each label beside its arrows inside the stable counter slot.
		FilePositionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		FilePositionAction = rowWidget(
			ReviewBar, FilePositionLabel, QStringLiteral("workbenchStripFilePositionAction"));
		ReviewBar->addAction(Navigation[0].Action);
		ReviewBar->addAction(Navigation[1].Action);
		ChangePositionLabel = rowLabel(
			ReviewBar,
			QStringLiteral("workbenchStripChangePosition"),
			QStringLiteral("Change position"),
			QStringLiteral("The selected change's position among the comparison's changes"));
		ChangePositionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		ChangePositionAction = rowWidget(
			ReviewBar, ChangePositionLabel, QStringLiteral("workbenchStripChangePositionAction"));
		ReviewBar->addAction(Navigation[2].Action);
		ReviewBar->addAction(Navigation[3].Action);
		// [[BPR-1507]]. The normal file control exposes the existing exact
		// acceptance action directly; its state menu keeps every alternative.
		// [[BPR-1578]]. The owner, 2026-10-17: "Accept is a button next to
		// other similar text labels in Changes and Files which are not
		// buttons, it's kind of confusing. It also doesn't have the under bar
		// that connects it to the dropdown control". After two rounds of
		// directions the owner chose the progress track (v002): Seen, Read and
		// Accept as steps of one control, the menu's chevron its last cell,
		// on one rail as File and Changes are with their arrows. The rails of
		// neighbouring groups must not run together: File and Changes are
		// parted by the Changes label's slot, Changes and this group by a bare
		// gap.
		auto* const progressGap = new QWidget{ ReviewBar };
		progressGap->setObjectName(QStringLiteral("reviewBarProgressGap"));
		progressGap->setFocusPolicy(Qt::NoFocus);
		progressGap->setFixedWidth(8);
		rowWidget(ReviewBar, progressGap, QStringLiteral("reviewBarProgressGapAction"));
		const auto makeStep = [this](const QString& aName, const QString& aAccessible, const std::size_t aIndex) {
			auto* const step = new QToolButton{ ReviewBar };
			step->setObjectName(aName);
			step->setText(aAccessible);
			step->setAccessibleName(aAccessible);
			// [[BPR-1701]]. Seen and Read are checked buttons, as Accept's
			// retained action is.
			step->setCheckable(true);
			step->setEnabled(false);
			connect(step, &QToolButton::clicked, this, [this, aIndex] { take_progress_step(aIndex); });
			return step;
		};
		SeenStepButton   = makeStep(QStringLiteral("workbenchProgressSeenStep"), QStringLiteral("Seen"), progress_track::seen_step);
		ReadStepButton   = makeStep(QStringLiteral("workbenchProgressReadStep"), QStringLiteral("Read"), progress_track::read_step);
		AcceptFileButton = new QToolButton{ ReviewBar };
		AcceptFileButton->setObjectName(QStringLiteral("workbenchAcceptFileButton"));
		AcceptFileButton->setDefaultAction(ReceiptActions[2]);
		ProgressTrack = new progress_track{ *SeenStepButton, *ReadStepButton, *AcceptFileButton, *ReviewControlsButton, ReviewBar };
		ProgressTrack->setAccessibleName(QStringLiteral("Private progress"));
		auto* const trackAction = rowWidget(ReviewBar, ProgressTrack, QStringLiteral("workbenchProgressTrackAction"));
		ReviewBar->set_group_rails({
			{ .First = FilePositionAction, .Last = Navigation[1].Action },
			{ .First = ChangePositionAction, .Last = Navigation[3].Action },
			{ .First = trackAction, .Last = trackAction },
		});
		// [[BPR-1815]]. How the file is shown, right of what the reviewer has
		// done with it: a rule, then the layout toggle and Go to
		// header/source, icon buttons on no rail. They are the View menu's
		// own actions, so the bar, the menu and the keys are one command
		// each, and the core decides the layout.
		rowWidget(ReviewBar, new strip_divider{ ReviewBar }, QStringLiteral("reviewBarViewDividerAction"));
		ReviewBar->addAction(OnePaneAction);
		ReviewBar->addAction(HeaderSourceAction);
		for (const auto& [action, name] : { std::pair{ OnePaneAction, QStringLiteral("workbenchOnePaneButton") }, std::pair{ HeaderSourceAction, QStringLiteral("workbenchHeaderSourceButton") } }) {
			if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(action)); button != nullptr) {
				button->setObjectName(name);
				button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			}
		}
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(HeaderSourceAction)); button != nullptr) {
			button->setAccessibleName(QStringLiteral("Go to header/source"));
		}
		rowSpacer(ReviewBar, QStringLiteral("reviewBarSpacer"), QStringLiteral("reviewBarSpacerAction"));
		// [[BPR-1129]]. The two composition commands are the window's, so their
		// keys work from anywhere in it without a visible presenter; the bar and
		// the Pull Request menu present them. They keep the object names the
		// composition pages and their tests already know.
		const auto makeComposition = [this](const QString& aText, const QString& aName, const shortcut_command aKey, const bool aInline) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setIconText(action->text().remove(QLatin1Char{ '&' }));
			action->setShortcuts(shortcut_keys(aKey));
			action->setShortcutContext(Qt::WindowShortcut);
			addAction(action);
			connect(action, &QAction::triggered, this, [this, aInline] {
				// [[BPR-1133]]. Comment answers the selected thread when there
				// is one, and comments on the current line otherwise.
				if (aInline) {
					comment_or_reply();
				} else {
					open_review_composition(false);
				}
			});
			return action;
		};
		ComposeReviewAction = makeComposition(tr("Compose &review"), QStringLiteral("composeReviewAction"), shortcut_command::compose_review, false);
		CommentAction       = makeComposition(tr("Co&mment"), QStringLiteral("composeInlineDraftAction"), shortcut_command::comment, true);
		// [[BPR-1345]]. While a review is in progress the bar says so, with
		// how many comments it holds, and Finish review opens its summary in
		// Discuss (the start-a-review frames, owner's Accept 2026-10-08).
		auto* const chip = new review_progress_chip{ ReviewBar };
		ReviewChip       = chip;
		connect(chip, &QToolButton::clicked, this, [this] { open_review_composition(false); });
		ReviewChipAction = rowWidget(ReviewBar, chip, QStringLiteral("reviewBarReviewChipAction"));
		ReviewChipAction->setVisible(false);
		// [[BPR-1345]] v002. Finish review has a key of its own (owner,
		// finish-review v001: "find some hotkey for finish review, maybe
		// cmd+option+enter"), bound once, for the whole window.
		FinishReviewAction = new QAction{ tr("Finish review"), this };
		FinishReviewAction->setObjectName(QStringLiteral("finishReviewAction"));
		FinishReviewAction->setShortcuts(shortcut_keys(shortcut_command::finish_review));
		FinishReviewAction->setShortcutContext(Qt::WindowShortcut);
		FinishReviewAction->setEnabled(false);
		addAction(FinishReviewAction);
		connect(FinishReviewAction, &QAction::triggered, this, [this] { open_review_composition(false); });
		ReviewBar->addAction(ComposeReviewAction);
		ReviewBar->addAction(CommentAction);
		// [[BPR-1133]]. Comment's arrow opens its menu, which names what
		// Comment would start: "Comment on line 431…" or "Reply to tbutler's
		// thread…". It offers that plain comment alone. Labels are chosen in
		// the editor's chip, never here (owner, BPR-1083 comment-menu-v001).
		CommentMenu = new QMenu{ this };
		CommentMenu->setObjectName(QStringLiteral("commentMenu"));
		CommentMenu->addAction(CommentAction);
		connect(CommentMenu, &QMenu::aboutToShow, this, [this] { update_comment_target(); });
		if (auto* const commentButton = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(CommentAction)); commentButton != nullptr) {
			commentButton->setMenu(CommentMenu);
			commentButton->setPopupMode(QToolButton::MenuButtonPopup);
		}
		PullRequestMenu->addSeparator();
		PullRequestMenu->addAction(ComposeReviewAction);
		PullRequestMenu->addAction(CommentAction);
		PrepareTourAction = new QAction{ tr("Prepare review &tour"), this };
		PrepareTourAction->setObjectName(QStringLiteral("prepareReviewTourAction"));
		PrepareTourAction->setShortcuts(shortcut_keys(shortcut_command::prepare_tour));
		PrepareTourAction->setEnabled(false);
		addAction(PrepareTourAction);
		PullRequestMenu->addAction(PrepareTourAction);
		connect(PrepareTourAction, &QAction::triggered, this, [this] { open_review_tour(); });
		AddTourStopAction = new QAction{ tr("Add tour stop"), this };
		AddTourStopAction->setObjectName(QStringLiteral("addTourStopAction"));
		AddTourStopAction->setShortcuts(shortcut_keys(shortcut_command::add_tour_stop));
		AddTourStopAction->setEnabled(false);
		addAction(AddTourStopAction);
		PullRequestMenu->addAction(AddTourStopAction);
		connect(PrepareTourAction, &QAction::changed, this, [this] { AddTourStopAction->setEnabled(PrepareTourAction->isEnabled()); });
		connect(AddTourStopAction, &QAction::triggered, this, [this] { TourEditor->trigger(tour_editor_action::add_stop); });
		// [[BPR-1212]]. Add comment and Suggest change, on the code's own
		// menu. Their keys are the code's, Return and Shift+Return, read by
		// the pane while it has focus (owner's Q1 answer). No window action
		// claims those keys; the menu's items only show them.
		AddCommentAction = new QAction{ tr("Add comment"), this };
		AddCommentAction->setObjectName(QStringLiteral("addCommentAction"));
		AddCommentAction->setIcon(conversation_icon(conversation_glyph::comment, palette()));
		connect(AddCommentAction, &QAction::triggered, this, [this] { static_cast<void>(comment_on_selected_lines(false)); });
		SuggestChangeAction = new QAction{ tr("Suggest change"), this };
		SuggestChangeAction->setObjectName(QStringLiteral("suggestChangeAction"));
		SuggestChangeAction->setIcon(conversation_icon(conversation_glyph::suggest, palette()));
		connect(SuggestChangeAction, &QAction::triggered, this, [this] { static_cast<void>(comment_on_selected_lines(true)); });
		// [[BPR-1594]]. The Agent tab is one key away (⌥⇧A); ⌘I asks it about
		// the selected lines, as the code's menu does.
		OpenAgentAction = new QAction{ tr("Agent"), this };
		OpenAgentAction->setObjectName(QStringLiteral("openAgentTabAction"));
		OpenAgentAction->setShortcuts(shortcut_keys(shortcut_command::open_agent));
		OpenAgentAction->setShortcutContext(Qt::WindowShortcut);
		OpenAgentAction->setToolTip(tr("Open the Agent tab"));
		addAction(OpenAgentAction);
		connect(OpenAgentAction, &QAction::triggered, this, [this] { show_agent(false); });
		AskAgentAction = new QAction{ tr("Ask about these lines"), this };
		AskAgentAction->setObjectName(QStringLiteral("askAgentAboutLinesAction"));
		AskAgentAction->setShortcuts(shortcut_keys(shortcut_command::ask_agent));
		AskAgentAction->setShortcutContext(Qt::WindowShortcut);
		AskAgentAction->setToolTip(tr("Ask the agent about the lines selected in the code"));
		addAction(AskAgentAction);
		connect(AskAgentAction, &QAction::triggered, this, [this] { show_agent(true); });
		update_compose_mark();
		update_strip_positions();
		update_strip_identity();
		apply_strip_width();
	}
}
