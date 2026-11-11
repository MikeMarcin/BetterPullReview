#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/usage_ring.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "github_presentation_text.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QByteArrayView>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QFlags>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QNonConstOverload>
#include <QObject>
#include <QOverload>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
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
		GithubConnectAction->setShortcuts(shortcut_keys(shortcut_command::connect));
		GithubConnectAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubConnectAction);
		GithubOpenAction = new QAction{
			QStringLiteral("&Open pull request"), this
		};
		GithubOpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
		GithubOpenAction->setShortcuts(shortcut_keys(shortcut_command::open_pull_request));
		GithubOpenAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubOpenAction);
		GithubRefreshAction = new QAction{ QStringLiteral("&Refresh"), this };
		GithubRefreshAction->setObjectName(
			QStringLiteral("refreshPullRequestAction"));
		GithubRefreshAction->setShortcuts(shortcut_keys(shortcut_command::refresh));
		GithubRefreshAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubRefreshAction);
		// [[BPR-0990]]. Home's refresh glyph is the profile's discovery, not
		// the pull request's refresh: the same place in the strip, its own
		// action, shown only on Home with an account. [[BPR-1600]]: the
		// window's Refresh key moves to it while Home shows
		// (`apply_refresh_key`).
		QueueRefreshAction = new QAction{ QStringLiteral("Refresh my pull requests"), this };
		QueueRefreshAction->setObjectName(QStringLiteral("refreshQueueAction"));
		QueueRefreshAction->setIconText(QStringLiteral("Refresh"));
		QueueRefreshAction->setVisible(false);
		QueueRefreshAction->setEnabled(false);
		addAction(QueueRefreshAction);
		connect(QueueRefreshAction, &QAction::triggered, this, [this] {
			// [[BPR-1575]]. The refresh this starts is the reviewer's own.
			HomeRefreshAsked = true;
			if (QueueDiscoveryDispatch) {
				QueueDiscoveryDispatch(application::queue_discovery_intent::refresh);
			}
		});

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
			this,
		};
		// [[BPR-0771]]. Open names what the Home bar names; the locator it
		// once read is gone. With nothing written there it takes the reviewer
		// to the bar.
		connect(GithubOpenAction, &QAction::triggered, this, [this] {
			if (HomeOpenBar == nullptr) {
				return;
			}
			if (HomeOpenBar->text().trimmed().isEmpty()) {
				choose_home(true);
				HomeOpenBar->setFocus(Qt::ShortcutFocusReason);
				return;
			}
			open_from_home_bar();
		});
		PullRequestPanel = new github_pull_request_panel{ };

		// [[BPR-0526]], [[BPR-0773]]. A profile has two ways in -- the token
		// prompt the Connect action opens, and a code read aloud into a browser
		// -- and a reviewer looking for either looks in one place: the server's
		// card. So the device panel is laid into that card's account row
		// (`place_device_sign_in`) rather than behind a drawer or a dialog of
		// its own.
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
		// [[BPR-0773]]. The window hears what the panel now shows so the card
		// holding it can make its account row the sign-in and its header say
		// Signing in. Where this host has no device flow, the token prompt the
		// Connect action already opens is the way in beside the refusal.
		DevicePanel->set_presentation_observer([this] {
			place_device_sign_in();
			// A flow belongs to the host it was asked for: while one runs, the
			// card holding the row is that host, and its terminal sentence is
			// said only there.
			if (DevicePanel->signing_in()) {
				const auto holder = std::ranges::find_if(ServerCards, [this](const server_card* const aCard) {
					return aCard->device_sign_in() == DevicePanel;
				});
				if (holder != ServerCards.end()) {
					DeviceFlowHost = (*holder)->hostname();
				}
			}
			update_server_cards();
		});
		DevicePanel->set_manual_token_dispatch([this] {
			if (GithubConnectAction != nullptr) {
				GithubConnectAction->trigger();
			}
		});

		// [[BPR-0646]]. RFC-BPR-0028 AC-01: the chrome above the comparison
		// is the strip and the epoch line only, and accounts live in Settings.
		// The server cards are the Accounts and hosts page of the Settings
		// window, one per configured server ([[BPR-0771]], [[BPR-0772]]), and
		// the device sign-in is the account row of its server's card
		// ([[BPR-0773]]). The strip's account button opens that page, and a
		// flow the core begins presents it, so the code is seen. The pull
		// request list (the queue landing) and the
		// pull request summary stay in the files drawer's bounded top region
		// until the repositories drawer (BPR-0651) and the overview
		// (BPR-0647) take them.
		SettingsWindow       = new settings_window{ this };
		auto* const accounts = new QWidget;
		AccountsPanel        = accounts;
		accounts->setObjectName(QStringLiteral("workbenchAccountsPage"));
		accounts->setAccessibleName(QStringLiteral("Accounts and hosts"));
		auto* const accountsColumn = new QVBoxLayout{ accounts };
		accountsColumn->setContentsMargins(0, 0, 0, 0);
		// [[BPR-0771]], [[BPR-0772]]. RFC-BPR-0032: the sentence, then one card
		// per configured server. The host picker, the locator, the status banner and
		// the public-preview strip are not here: a host is chosen by its card's
		// Connect, a pull request is named in the Home bar, a load refusal is
		// said under that bar, and a session failure is said in its card.
		auto* const lede = new QLabel{ QStringLiteral("One account per server. Credentials are never shown here."), accounts };
		lede->setObjectName(QStringLiteral("accountsLede"));
		lede->setWordWrap(true);
		lede->setTextFormat(Qt::PlainText);
		lede->setForegroundRole(QPalette::PlaceholderText);
		accountsColumn->addWidget(lede);
		// [[BPR-0773]]. Device sign-in is no longer a panel of its own on the
		// page: it is the account row of the card whose host it is bound to,
		// laid in once that card exists.
		DevicePanel->setParent(accounts);
		DevicePanel->hide();
		ServerCardColumn = new QVBoxLayout{ };
		ServerCardColumn->setContentsMargins(0, 0, 0, 0);
		ServerCardColumn->setSpacing(16);
		accountsColumn->addLayout(ServerCardColumn);
		for (const auto& profile : GithubProfiles) {
			add_server_card(profile);
		}
		place_device_sign_in();
		// [[BPR-0660]], [[BPR-0774]]. The enterprise servers the reviewer keeps,
		// so a GitHub Enterprise host is offered at the next launch without
		// being typed again. There is no list of them: a kept server is its
		// host's card, with Remember and Forget, and one bordered button under
		// the cards adds another as a card in its edit state. No token is kept.
		build_enterprise_server_draft(accounts, *accountsColumn);
		SettingsWindow->adopt_panel(settings_page::accounts_and_hosts, accounts);
		build_repositories_page();
		// [[BPR-0652]]. Home & queue (Reviews & queue until [[BPR-1597]]):
		// the queue defaults, the observer rules and the way to Repositories.
		// [[BPR-1597]]. The review and comment rows have pages of their own,
		// Review and Comments (owner's pick A, 2026-10-17: "Reviews & queue is
		// too much of a big grab bag of settings").
		auto* const reviews = new QWidget;
		reviews->setObjectName(QStringLiteral("workbenchReviewsPage"));
		reviews->setAccessibleName(QStringLiteral("Home and queue"));
		auto* const reviewsColumn = new QVBoxLayout{ reviews };
		reviewsColumn->setContentsMargins(0, 0, 0, 0);
		auto* const reviewsSubtitle = new QLabel{
			QStringLiteral("Local behaviour only. Nothing here changes assignments or subscriptions on GitHub."),
			reviews
		};
		reviewsSubtitle->setObjectName(QStringLiteral("reviewsSubtitle"));
		reviewsSubtitle->setWordWrap(true);
		reviewsColumn->addWidget(reviewsSubtitle);
		const auto settingsPage = [](const QString& aName, const QString& aAccessible, const QString& aSubtitle) {
			auto* const page = new QWidget;
			page->setObjectName(aName);
			page->setAccessibleName(aAccessible);
			auto* const pageColumn = new QVBoxLayout{ page };
			pageColumn->setContentsMargins(0, 0, 0, 0);
			auto* const subtitle = new QLabel{ aSubtitle, page };
			subtitle->setObjectName(aName + QStringLiteral("Subtitle"));
			subtitle->setWordWrap(true);
			pageColumn->addWidget(subtitle);
			return std::pair{ page, pageColumn };
		};
		const auto [reviewPage, reviewColumn]     = settingsPage(QStringLiteral("workbenchReviewPage"), QStringLiteral("Review"), QStringLiteral("How a pull request opens and reads. Local behaviour only."));
		const auto [commentsPage, commentsColumn] = settingsPage(QStringLiteral("workbenchCommentsPage"), QStringLiteral("Comments"), QStringLiteral("Labels, the comment workshop, and how comments read. Local behaviour only."));
		build_behaviour_editor(reviewPage, reviewColumn, commentsPage, commentsColumn);
		reviewColumn->addStretch(1);
		commentsColumn->addStretch(1);
		build_queue_settings(reviews, reviewsColumn);
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
		// [[BPR-0659]]. Saved repositories moved to a page of their own
		// (RFC-BPR-0062, owner direction 2026-10-07). Observer rules stay: they
		// are queue behaviour over any repository, not the saved list. Home's
		// empty sentence still links here, so the page says where they went.
		auto* const savedHeading = new QLabel{ QStringLiteral("Saved repositories"), reviews };
		savedHeading->setObjectName(QStringLiteral("reviewsSavedHeading"));
		savedHeading->setFont(headingFont);
		reviewsColumn->addWidget(savedHeading);
		auto* const savedPointer = new QWidget{ reviews };
		savedPointer->setObjectName(QStringLiteral("reviewsRepositoriesPointer"));
		auto* const savedPointerRow = new QHBoxLayout{ savedPointer };
		savedPointerRow->setContentsMargins(0, 0, 0, 0);
		auto* const savedPointerText = new QLabel{ tr("Saved repositories are managed on their own page."), savedPointer };
		savedPointerText->setWordWrap(true);
		savedPointerText->setTextFormat(Qt::PlainText);
		savedPointerRow->addWidget(savedPointerText, 1);
		auto* const openRepositories = new quiet_push_button{ tr("Open Repositories"), savedPointer };
		openRepositories->setObjectName(QStringLiteral("reviewsOpenRepositories"));
		openRepositories->setAccessibleDescription(tr("Shows Settings › Repositories, where saved repositories are added and removed"));
		openRepositories->setAutoDefault(false);
		savedPointerRow->addWidget(openRepositories, 0, Qt::AlignRight);
		connect(openRepositories, &QPushButton::clicked, this, [this] { open_settings(settings_page::repositories); });
		reviewsColumn->addWidget(savedPointer);
		SettingsWindow->adopt_panel(settings_page::home_and_queue, reviews);
		SettingsWindow->adopt_panel(settings_page::review, reviewPage);
		SettingsWindow->adopt_panel(settings_page::comments, commentsPage);
		build_capability_settings();

		static_cast<void>(Drawers[0].Panel->layout());
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
			// [[BPR-0981]]. The resident fuzzy match of RFC-BPR-0025.
			FilesFilter->setAccessibleDescription(QStringLiteral(
				"Show only files whose name or path contains these letters in order; their folders stay visible"));
			FilesFilter->setPlaceholderText(QStringLiteral("Filter"));
			FilesFilter->setClearButtonEnabled(true);
			controls->addWidget(FilesFilter, 1);
			surfaceColumn->addLayout(controls);
			// RFC-BPR-0089. A commit filter is never active out of sight: the
			// chip names it here and opens the Commits tab.
			FilesCommitChip = new commit_filter_chip{ true, filesSurface };
			FilesCommitChip->set_clear_dispatch([this] {
				if (CommitFilterDispatch) {
					CommitFilterDispatch(application::commit_filter_gesture{ .Kind = core::commit_filter_operation_kind::clear, .Commits = { } });
				}
			});
			FilesCommitChip->set_commits_dispatch([this] { show_commits(); });
			surfaceColumn->addWidget(FilesCommitChip);
			FilesFilterNotice = new filter_empty_notice{
				QStringLiteral("files"), QStringLiteral("files"), filesSurface
			};
			FilesFilterNotice->hide();
			surfaceColumn->addWidget(FilesFilterNotice);
			FilesTree = new files_tree_view{ filesSurface };
			surfaceColumn->addWidget(FilesTree, 1);
			// RFC-BPR-0089 M2. The tree ends with the generated files set aside,
			// and every file's menu offers its generated gestures.
			FilesGeneratedRow = new generated_files_row{ filesSurface };
			FilesGeneratedRow->set_gesture_dispatch([this](const application::generated_file_gesture& aGesture) {
				if (GeneratedDispatch) {
					GeneratedDispatch(aGesture);
				}
			});
			// [[BPR-1693]]. A click opens a listed file, as one in the tree does.
			FilesGeneratedRow->set_open_on_click(true);
			surfaceColumn->addWidget(FilesGeneratedRow, 0);
			FilesTree->set_menu_extension([this](QMenu& aMenu, const QString& aPath) {
				if (!FilterProjection) {
					return;
				}
				const auto path = aPath.toStdString();
				add_generated_actions(aMenu, path, generated_gestures_for(*FilterProjection, path), GeneratedDispatch);
			});
			// [[BPR-1132]]. Cmd-click (Ctrl-click) and Open in New Tab.
			FilesTree->set_new_tab_dispatch([this](const core::workbench_file_id& aFile) {
				if (DocumentTabDispatch) {
					DocumentTabDispatch(core::open_in_new_tab_intent{ .File = aFile });
				}
			});
			FilesTree->set_select_dispatch([this](const core::workbench_file_id& aFile) {
				// The list's selection normally dispatches. A repeated tree
				// click is still an explicit open when its cursor is unchanged.
				if (ApplyingFileSelection || Drawers[0].Panel->contents() == nullptr
					|| Drawers[0].Panel->contents_view() == nullptr) {
					return;
				}
				const auto index = Drawers[0].Panel->contents()->index_for_file(aFile);
				if (index.isValid()) {
					if (Drawers[0].Panel->contents_view()->currentIndex() == index) {
						dispatch_file_selection(index);
					} else {
						Drawers[0].Panel->contents_view()->setCurrentIndex(index);
					}
				}
			});
			connect(FilesOrderSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
				FilesTree->set_order(static_cast<files_order>(FilesOrderSelector->currentData().toInt()));
			});
			connect(FilesFilter, &QLineEdit::textChanged, this, [this](const QString& aText) {
				FilesTree->set_filter(aText);
				refresh_filter_notices();
			});
			connect(FilesFilterNotice->clear_button(), &QAbstractButton::clicked, this, [this] {
				FilesFilter->clear();
			});
			FileSearchTabs = new QTabWidget;
			FileSearchTabs->setObjectName(QStringLiteral("fileSearchTabs"));
			FileSearchTabs->setAccessibleName(tr("Files and Search"));
			// [[BPR-1509]]. The tabs are the drawer's header, as in Comments/Notes.
			FileSearchTabs->setDocumentMode(true);
			FileSearchTabs->tabBar()->setExpanding(false);
			FileSearchTabs->tabBar()->setDrawBase(true);
			FileSearchTabs->tabBar()->setFocusPolicy(Qt::StrongFocus);
			Drawers[0].Panel->set_heading_shown(false);
			FileSearchTabs->addTab(filesSurface, tr("Files"));
			SearchPanel          = new source_search_panel{ FileSearchTabs };
			SearchPanel->Changed = [this] { update_source_find_actions(); };
			FileSearchTabs->addTab(SearchPanel, tr("Search"));
			Drawers[0].Panel->host_beside_list(FileSearchTabs, FilesTree);
		}
		// [[BPR-0671]]. The pull request's identity, author, branches, state
		// and description are the overview's, and the strip already names the
		// repository, the number and the title. [[BPR-0646]] parked them in a
		// bounded box at the top of the files drawer until the overview
		// ([[BPR-0647]]) took them, and the placeholder shipped: the owner met
		// "the entire set of information for this PR crammed into a tiny box
		// in files browser", above the files it was covering. The box is gone;
		// the widget survives as Home's, where its empty page says why a load
		// failed. The files drawer holds files.
		// The description lives in this column, which no longer has a place
		// in the window: the overview presents it. The widget is kept, parented
		// and hidden, so the panel it belongs to keeps its shape and nothing
		// that writes to it has to learn a new one.
		auto* const top = new QWidget{ this };
		top->setObjectName(QStringLiteral("workbenchDrawerTop"));
		top->setAccessibleName(QStringLiteral("Pull request"));
		top->hide();
		auto* const topColumn = new QVBoxLayout{ top };
		topColumn->setContentsMargins(0, 0, 0, 0);
		DrawerTopColumn = topColumn;
		// Switching to sign-in is offered from the ring and from every
		// refusal, and both go to the one entry point already mounted here:
		// the device panel's "Get a code", for the host last chosen. The
		// window adds no second way in, so sign-in cannot begin against a
		// host the reviewer is not looking at.
		set_public_preview_dispatch(
			[this](application::public_preview_gesture aGesture) {
				static_cast<void>(aGesture);
				begin_selected_sign_in();
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
		auto* const copyMessage = new quiet_tool_button{ emptyState };
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
		// [[BPR-0775]]. The stack's empty page is not Home's, and it has no other
		// place: it stays hidden here so what writes to it keeps its shape.
		PullRequestStack->hide();
		// Placed by update_home_landing: on Home while nothing is open, at
		// the top of the drawer's region once a pull request is.
		// The panel builds the description but deliberately does not lay it out:
		// where a description belongs is a decision for whoever assembles the
		// window, and this one puts it under the metadata it describes. Bounded
		// rather than stretching, for the same reason.
		PullRequestPanel->body_editor()->setMaximumHeight(120);
		topColumn->addWidget(PullRequestPanel->body_editor());
		// [[BPR-0671]]. The one thing the summary box carried that the
		// overview did not: the commit oids behind the branches.
		if (OverviewPage != nullptr) {
			OverviewPage->adopt_technical_details(
				PullRequestPanel->technical_details_button(),
				PullRequestPanel->technical_details());
		}

		// [[BPR-0646]]. The appearance surface moves out of the review header
		// into the Settings window, a modeless child of this window reached by
		// the preferences shortcut. The composition still finds the same panel
		// through appearance_settings(), so its wiring is unchanged.
		AppearanceSettings = new appearance_settings_panel{ { } };
		AppearanceSettings->setObjectName(
			QStringLiteral("appearanceSettingsPanel"));
		SettingsWindow->adopt_panel(settings_page::appearance, AppearanceSettings);
		SettingsWindow->set_close_handler([this] { AppearanceSettings->cancel_preview(); });

		CentralStack = new QStackedWidget{ this };
		CentralStack->setObjectName(QStringLiteral("workbenchCentralStack"));
		CentralStack->addWidget(HomePage);
		CentralStack->addWidget(ReviewSurface);
		setCentralWidget(CentralStack);
		connect(CentralStack, &QStackedWidget::currentChanged, statusBar(), [this] { statusBar()->update(); });

		// [[BPR-0646]]. Refresh sits in the strip's right region, before the
		// comments toggle, as the design places it. Same action as the
		// session chrome's button, so there is one refresh with one state.
		PrimaryToolbar->insertAction(Drawers[1].Toggle, GithubRefreshAction);
		PrimaryToolbar->insertAction(Drawers[1].Toggle, QueueRefreshAction);
		if (auto* const glyph = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(QueueRefreshAction)); glyph != nullptr) {
			glyph->setFocusPolicy(Qt::StrongFocus);
			glyph->setAccessibleName(QStringLiteral("Refresh my pull requests"));
		}
		// [[BPR-0783]], [[BPR-1580]]. Home's View control sits before the refresh
		// glyph, so the strip reads View, refresh, the divider, account and
		// settings.
		build_home_strip_controls();
		// The action's default state is applied once everything it governs
		// exists; a default that is never applied is a default in name only.
		apply_review_details_visibility();

		SessionPanel->apply_capabilities({ });
		SessionPanel->apply_rate_limits({ });
		clear_github_pull_request();
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
		PullRequestPanel->refresh_native_appearance();
		build_pending_veils();
		// [[BPR-1575]]. A refresh's note under its button, its freshness in
		// the status bar, and the clocks that turn and age them.
		RefreshNote = new refresh_note{ *this };
		RefreshNote->set_retry([this] {
			auto* const action = showing_home() ? QueueRefreshAction : GithubRefreshAction;
			if (action != nullptr && action->isEnabled()) {
				action->trigger();
			}
		});
		RefreshFreshness = new QLabel{ this };
		RefreshFreshness->setObjectName(QStringLiteral("refreshFreshness"));
		RefreshFreshness->setAccessibleName(QStringLiteral("Last refresh"));
		RefreshFreshness->setTextFormat(Qt::RichText);
		RefreshFreshness->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		RefreshFreshness->setForegroundRole(QPalette::PlaceholderText);
		// Its words whole: the status line beside it is the one that yields.
		RefreshFreshness->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
		RefreshFreshness->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		RefreshFreshness->hide();
		connect(RefreshFreshness, &QLabel::linkActivated, this, [this](const QString&) {
			auto* const action = showing_home() ? QueueRefreshAction : GithubRefreshAction;
			if (action != nullptr && action->isEnabled()) {
				action->trigger();
			}
		});
		statusBar()->addPermanentWidget(RefreshFreshness);
		RefreshFrames = new QTimer{ this };
		RefreshFrames->setInterval(80);
		connect(RefreshFrames, &QTimer::timeout, this, [this] {
			RefreshFrame = (RefreshFrame + 1) % 12;
			present_refresh();
		});
		const auto cue = [this](bool& aCue) {
			auto* const timer = new QTimer{ this };
			timer->setSingleShot(true);
			timer->setInterval(1'500);
			connect(timer, &QTimer::timeout, this, [this, &aCue] {
				aCue = false;
				present_refresh();
			});
			return timer;
		};
		PullCueTimer    = cue(PullSettledCue);
		HomeCueTimer    = cue(HomeSettledCue);
		RefreshAgeTimer = new QTimer{ this };
		RefreshAgeTimer->setSingleShot(true);
		connect(RefreshAgeTimer, &QTimer::timeout, this, [this] { present_refresh(); });
		for (auto** const slow : { &RefreshSlowTimer, &HomeSlowTimer }) {
			*slow = new QTimer{ this };
			(*slow)->setSingleShot(true);
			(*slow)->setInterval(static_cast<int>(core::pending_slow_milliseconds));
			connect(*slow, &QTimer::timeout, this, [this] { present_refresh(); });
		}
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
		// [[BPR-0990]]. What discovery last did, in one sentence, where Home
		// already reports what the queue refused.
		HomeRepositoryRow = new QWidget{ HomePage };
		HomeRepositoryRow->setObjectName(QStringLiteral("homeRepositoryRow"));
		auto* repositoryRow = new QBoxLayout{ QBoxLayout::LeftToRight, HomeRepositoryRow };
		repositoryRow->setContentsMargins(24, 0, 24, 4);
		HomeDiscoveryStatus = new QLabel{ HomeRepositoryRow };
		HomeDiscoveryStatus->setObjectName(QStringLiteral("homeDiscoveryStatus"));
		HomeDiscoveryStatus->setAccessibleName(QStringLiteral("My pull requests status"));
		HomeDiscoveryStatus->setWordWrap(true);
		HomeDiscoveryStatus->setVisible(false);
		repositoryRow->addWidget(HomeDiscoveryStatus, 1);
		AddSavedRepository = new QAction{ tr("Add repository"), this };
		AddSavedRepository->setObjectName(QStringLiteral("addSavedRepository"));
		connect(AddSavedRepository, &QAction::triggered, this, [this] { open_repository_addition(); });
		HomeAddRepositoryButton = new QToolButton{ HomeRepositoryRow };
		HomeAddRepositoryButton->setObjectName(QStringLiteral("homeAddRepositoryButton"));
		HomeAddRepositoryButton->setDefaultAction(AddSavedRepository);
		HomeAddRepositoryButton->setFocusPolicy(Qt::StrongFocus);
		HomeAddRepositoryButton->setAccessibleName(tr("Add repository"));
		HomeAddRepositoryButton->setAccessibleDescription(tr("Add a repository to find my pull requests and review requests"));
		repositoryRow->addWidget(HomeAddRepositoryButton, 0, Qt::AlignLeft);
		// RFC-BPR-0062, owner verdicts of 2026-10-07 and 2026-10-08: with no
		// saved repository for the displayed account and host, the row gives way
		// to one default-styled button in its place under the bar, "Just the
		// button that opens settings". It opens Settings › Repositories with the
		// repository field ready, as every Add repository does.
		HomeOnboarding = new QWidget{ HomePage };
		HomeOnboarding->setObjectName(QStringLiteral("homeRepositoryOnboarding"));
		HomeOnboarding->setAccessibleName(tr("Add a repository"));
		auto* const onboarding = new QVBoxLayout{ HomeOnboarding };
		onboarding->setContentsMargins(24, 16, 24, 8);
		onboarding->setSpacing(0);
		HomeOnboardingButton = new QPushButton{ tr("Add a repository"), HomeOnboarding };
		HomeOnboardingButton->setObjectName(QStringLiteral("homeRepositoryOnboardingButton"));
		HomeOnboardingButton->setAccessibleName(tr("Add a repository"));
		HomeOnboardingButton->setAccessibleDescription(tr("Opens Settings, Repositories, to add a repository where my pull requests and review requests are found; nothing changes on GitHub"));
		HomeOnboardingButton->setToolTip(tr("Find my pull requests and review requests in a repository"));
		HomeOnboardingButton->setFocusPolicy(Qt::StrongFocus);
		HomeOnboardingButton->setAutoDefault(false);
		// The default look only: the main window has no default-button key.
		HomeOnboardingButton->setDefault(true);
		connect(HomeOnboardingButton, &QPushButton::clicked, AddSavedRepository, &QAction::trigger);
		connect(AddSavedRepository, &QAction::changed, HomeOnboardingButton, [this] {
			HomeOnboardingButton->setEnabled(AddSavedRepository->isEnabled());
		});
		onboarding->addWidget(HomeOnboardingButton, 0, Qt::AlignLeft);
		if (auto* homeColumn = qobject_cast<QVBoxLayout*>(HomePage->layout())) {
			homeColumn->insertWidget(homeColumn->indexOf(HomeCycles), HomeOnboarding);
			homeColumn->insertWidget(homeColumn->indexOf(HomeCycles), HomeRepositoryRow);
		}
		HomeOnboarding->hide();
		HomeRepositoryRow->hide();

		QueueLanding     = new review_landing_model{ surface };
		QueueFilterModel = new review_landing_filter_model{ *QueueLanding, surface };
		// [[BPR-0650]] [[BPR-0776]]. Home's list: a panel per state, in state
		// order, over the same decided queue, and Recently viewed last. A
		// click or Return on a row opens its pull request on Overview, as a
		// row of the repositories tree does; an empty queue's sentence links
		// to Home & queue.
		HomeList = new qt::home_list{ *QueueFilterModel, surface };
		// The built-in state colours until the composition hands over the
		// profile it resolved.
		HomeList->set_state_colours(home_state_colours_for(appearance_profile{ }));
		HomeList->set_open_dispatch([this](const core::queue_item& aItem) { open_pull_request_item(aItem); });
		// [[BPR-1357]]. Cancel or Retry on the row being opened.
		HomeList->set_open_action_dispatch([this] { dispatch_home_open_action(); });
		// [[BPR-1338]]. Open on GitHub from a queue row: its icon names the row
		// directly; its menu holds the window's action, which names the row the
		// right click made current, as the action's key does.
		HomeList->set_page_dispatch([this](const core::queue_item& aItem) { open_pull_request_page(aItem.Ref); });
		HomeList->set_current_observer([this] {
			update_open_on_github_action();
			update_home_review_actions();
		});
		// [[BPR-1342]]. ⌘Return, ⌘-click or middle-click on a queue row.
		HomeList->set_new_window_dispatch([this](const core::queue_item& aItem) { request_review_window(aItem.Ref); });
		// Which of Home's lists the reviewer is in: the one that last took the
		// keyboard. A menu taking it for a moment changes nothing. [[BPR-1365]]:
		// it is also the one list that shows a selection.
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* const aNow) {
			if (aNow == nullptr || HomeList == nullptr) {
				return;
			}
			if (aNow == HomeRecentList) {
				claim_home_selection(home_selection_list::recent);
			} else if (aNow == HomeCyclesList) {
				claim_home_selection(home_selection_list::cycles);
			} else if (qobject_cast<QListView*>(aNow) != nullptr && HomeList->isAncestorOf(aNow)) {
				claim_home_selection(home_selection_list::queue);
			}
		});
		// [[BPR-1342]]. Home's one row menu, for a queue row and a Recently
		// viewed row alike: Open Review (the default, in this window), Open
		// Review in New Window, Resume for a Recently viewed row, then Open on
		// GitHub. Each shows its key. The keys are read by the lists
		// themselves, so these shortcuts are context-bound to the menu and
		// never act from the window.
		const auto menuKey = [](QAction& aAction, const shortcut_command aKey) {
			aAction.setShortcuts(shortcut_keys(aKey));
			aAction.setShortcutContext(Qt::WidgetShortcut);
			aAction.setShortcutVisibleInContextMenu(true);
		};
		HomeOpenReviewAction = new QAction{ tr("Open Review"), this };
		HomeOpenReviewAction->setObjectName(QStringLiteral("homeOpenReviewAction"));
		HomeOpenReviewAction->setStatusTip(QStringLiteral("Open this pull request in this window"));
		menuKey(*HomeOpenReviewAction, shortcut_command::home_open_review);
		HomeOpenReviewAction->setEnabled(false);
		connect(HomeOpenReviewAction, &QAction::triggered, this, [this] { open_home_review(false); });
		HomeOpenReviewWindowAction = new QAction{ tr("Open Review in New Window"), this };
		HomeOpenReviewWindowAction->setObjectName(QStringLiteral("homeOpenReviewInNewWindowAction"));
		HomeOpenReviewWindowAction->setStatusTip(QStringLiteral("Open this pull request in a new window of this profile"));
		menuKey(*HomeOpenReviewWindowAction, shortcut_command::home_open_window);
		HomeOpenReviewWindowAction->setEnabled(false);
		connect(HomeOpenReviewWindowAction, &QAction::triggered, this, [this] { open_home_review(true); });
		// [[BPR-1683]]. Ignore, a queue row's only; Unignore on an ignored
		// row. Delete (or Backspace) is read by the lists. Delete is the tour
		// editor's Remove in this window, so, as Remove from Recently Viewed,
		// the action claims no sequence; the catalogue names the key.
		HomeIgnoreAction = new QAction{ home_ignore_text(), this };
		HomeIgnoreAction->setObjectName(QStringLiteral("homeIgnoreAction"));
		HomeIgnoreAction->setStatusTip(QStringLiteral("Moves this pull request to Ignored at the bottom of Home; nothing changes on GitHub"));
		HomeIgnoreAction->setEnabled(false);
		connect(HomeIgnoreAction, &QAction::triggered, this, [this] {
			if (const auto target = home_review_target(); target.has_value()) {
				if (const auto* const item = std::get_if<core::queue_item>(&*target)) {
					dispatch_home_ignore(*item);
				}
			}
		});
		HomeList->set_ignore_dispatch([this](const core::queue_item& aItem) { dispatch_home_ignore(aItem); });
		// [[BPR-1865]]. The filter decides nothing: the core ranks, the list
		// shows what it found, and these keys move through it.
		if (HomeFilter != nullptr) {
			HomeFilter->set_query_observer([this](const QString&) { apply_home_filter(); });
			HomeFilter->set_step_dispatch([this](const int aStep) { return HomeList->step_filter(aStep); });
			HomeFilter->set_open_dispatch([this](const bool aNewWindow) { HomeList->open_filter_result(aNewWindow); });
			HomeFilter->set_leave_dispatch([this] { HomeList->focus_filter_result(); });
			HomeList->set_filter_typing_dispatch([this](const QString& aText) {
				if (HomeFilter->isVisible()) {
					HomeFilter->take_typing(aText);
				}
			});
			HomeList->set_filter_result_observer([this](QAbstractItemView* const aView) {
				claim_home_selection(aView == HomeRecentList ? home_selection_list::recent : home_selection_list::queue);
			});
			HomeList->set_recent_open_dispatch([this](const int aRow) { dispatch_recent_review(aRow, false); });
		}
		OpenOnGithubAction->setShortcutVisibleInContextMenu(true);
		HomeRowMenu = new QMenu{ this };
		HomeRowMenu->setObjectName(QStringLiteral("homeRowMenu"));
		HomeRowMenu->setAccessibleName(QStringLiteral("Pull request row"));
		HomeRowMenu->addAction(HomeOpenReviewAction);
		HomeRowMenu->addAction(HomeOpenReviewWindowAction);
		if (HomeRecentResumeAction != nullptr) {
			HomeRowMenu->addAction(HomeRecentResumeAction);
		}
		HomeRowMenu->addSeparator();
		HomeRowMenu->addAction(OpenOnGithubAction);
		HomeRowMenu->addSeparator();
		HomeRowMenu->addAction(HomeIgnoreAction);
		if (HomeRecentRemoveAction != nullptr) {
			HomeRowMenu->addSeparator();
			HomeRowMenu->addAction(HomeRecentRemoveAction);
		}
		HomeRowMenu->setDefaultAction(HomeOpenReviewAction);
		connect(HomeRowMenu, &QMenu::aboutToHide, this, [this] {
			for (auto* const action : { HomeRecentResumeAction, HomeRecentRemoveAction }) {
				if (action != nullptr) {
					action->setVisible(true);
				}
			}
		});
		HomeList->set_row_menu_dispatch([this](const QPoint& aGlobal) {
			claim_home_selection(home_selection_list::queue);
			popup_home_row_menu(aGlobal);
		});
		// The empty lane's link names the saved repositories, so it opens their
		// page; observer rules stay reachable from Filter's Open Home & queue.
		HomeList->set_settings_dispatch([this] { open_settings(settings_page::repositories); });
		// [[BPR-1580]]. The filtered sentence's Clear filters.
		HomeList->set_clear_filters_dispatch([this] { restore_home_view_defaults(false); });
		// [[BPR-0783]]. Collapsing one of the panels that carry no name is part of
		// this window's view, so the whole view leaves for the host.
		HomeList->set_view_observer([this] {
			HomeView.Collapsed = HomeList->collapsed_panels();
			dispatch_home_view();
		});
		layout->addWidget(HomeList, 1);

		// [[BPR-0783]]. The six typed queue controls are built here and laid out
		// on Settings' Queue defaults (build_queue_settings): Home's Filter menu
		// holds the four that survive as filters, and what a new window starts
		// with is a setting. They are the same controls, not a mirror of them.
		QueueRoleFilter = new QComboBox{ surface };
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
								   core::attention_bucket::quiet,
								   core::attention_bucket::detail_unknown }) {
			QueueBucketFilter->addItem(
				tr("Bucket: %1").arg(QString::fromUtf8(core::name(bucket).data())),
				static_cast<int>(bucket));
		}
		QueueRepositoryFilter = new QComboBox{ surface };
		QueueRepositoryFilter->setObjectName(
			QStringLiteral("queueRepositoryFilter"));
		QueueRepositoryFilter->setAccessibleName(
			QStringLiteral("Queue repository filter"));
		QueueRepositoryFilter->setAccessibleDescription(QStringLiteral(
			"Show every repository or one exact repository in this queue"));
		QueueRepositoryFilter->addItem(tr("Repository: any"), QString{ });

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
				[this, selector] { dispatch_queue_preferences(selector); });
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
		// Home & queue, adopted there once the Settings window exists.
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

	void workbench_window::build_capability_settings()
	{
		auto* const agents      = new QWidget;
		auto* const column      = new QVBoxLayout{ agents };
		auto* const explanation = new QLabel{ QStringLiteral("The selected host determines which agent profiles are allowed. The request choices here apply to the next explicit request in the current review; they are not saved as global defaults. Private notes, the current draft and uncommitted work are included only when selected. Start the request from the review's agent panel."), agents };
		explanation->setWordWrap(true);
		column->addWidget(explanation);
		auto* const form    = new QFormLayout{ };
		auto* const profile = new QComboBox{ agents };
		profile->setObjectName(QStringLiteral("settingsAgentProfile"));
		profile->setAccessibleName(QStringLiteral("Allowed agent profile for this request"));
		profile->setModel(AgentPanel->profile_selector()->model());
		form->addRow(QStringLiteral("Allowed profile"), profile);
		auto* const notes = new QListView{ agents };
		notes->setObjectName(QStringLiteral("settingsAgentNotes"));
		notes->setAccessibleName(QStringLiteral("Private notes included with this request"));
		notes->setModel(AgentPanel->note_selector()->model());
		notes->setMaximumHeight(160);
		form->addRow(QStringLiteral("Private notes"), notes);
		auto* const draft = new QCheckBox{ agents };
		draft->setObjectName(QStringLiteral("settingsAgentDraft"));
		draft->setAccessibleName(QStringLiteral("Include current private draft"));
		form->addRow(QStringLiteral("Private draft"), draft);
		column->addLayout(form);
		auto* const checkout = new quiet_tool_button{ agents };
		checkout->setDefaultAction(AgentPanel->existing_checkout_action());
		checkout->setAccessibleName(QStringLiteral("Use existing working copy for this request"));
		column->addWidget(checkout);
		auto* const scope = new QLabel{ agents };
		scope->setTextFormat(Qt::PlainText);
		scope->setWordWrap(true);
		scope->setAccessibleName(QStringLiteral("Agent request scope"));
		column->addWidget(scope);
		const auto refresh = [page = QPointer<QWidget>{ agents }, panel = QPointer<agent_launch_panel>{ AgentPanel }, profile, notes, draft, scope] {
			if (!page || !panel) {
				return;
			}
			const auto profileBlock = QSignalBlocker{ profile };
			const auto draftBlock   = QSignalBlocker{ draft };
			profile->setCurrentIndex(panel->profile_selector()->currentIndex());
			profile->setEnabled(panel->profile_selector()->isEnabled() && profile->count() != 0);
			notes->setEnabled(panel->note_selector()->isEnabled());
			draft->setText(panel->draft_selector()->text());
			draft->setChecked(panel->draft_selector()->isChecked());
			draft->setEnabled(panel->draft_selector()->isEnabled());
			scope->setText(panel->disclosure_text().isEmpty() ? QStringLiteral("Open a review to see its allowed profiles and request scope.") : panel->disclosure_text());
		};
		connect(profile, &QComboBox::currentIndexChanged, AgentPanel->profile_selector(), &QComboBox::setCurrentIndex);
		connect(draft, &QCheckBox::toggled, AgentPanel->draft_selector(), &QCheckBox::setChecked);
		connect(AgentPanel->draft_selector(), &QCheckBox::toggled, agents, refresh);
		AgentPanel->set_configuration_observer(refresh);
		// [[BPR-1301]]. Saved for the profile, unlike the choices above, so
		// it sits apart under its own heading, beside the writing hooks.
		// [[BPR-1753]]. Agent setup's saved programs, beside the wrapper.
		AgentPrograms = new agent_programs_panel{ agents };
		column->addSpacing(12);
		column->addWidget(AgentPrograms);
		// [[BPR-1753]], owner decision 2026-10-23: the Claude process
		// wrapper and model rows are gone from Settings; their values moved
		// into the agent list. The panel stays unshown, for the profile's
		// saved values that move once.
		ClaudeWrapper = new claude_wrapper_panel{ this };
		ClaudeWrapper->hide();
		WritingHooks = new writing_hooks_panel{ agents };
		column->addSpacing(12);
		column->addWidget(WritingHooks);
		WritingHooks->present_label_helper(Behaviour.LabelHelper);
		// [[BPR-1917]]. The review skill is one of the built-in prompts: it is
		// listed with them, under the Agent tab, and shows its own editor.
		ReviewSkill = new review_skill_panel{ agents };
		WritingHooks->adopt_review_skill(ReviewSkill);
		SettingsWindow->adopt_panel(settings_page::agents_and_context, agents);
		// [[BPR-1856]], owner 2026-11-04: Add repository comes first on
		// Repositories; the clone folders and linked clones follow it.
		CloneFolders = new clone_folders_panel;
		SettingsWindow->adopt_panel_below(settings_page::repositories, CloneFolders, SavedRepositoriesEditor);

		auto* const storage       = new QWidget;
		auto* const storageColumn = new QVBoxLayout{ storage };
		auto* const heading       = new QLabel{ QStringLiteral("Local data location"), storage };
		storageColumn->addWidget(heading);
		StorageLocation = new QLineEdit{ storage };
		StorageLocation->setObjectName(QStringLiteral("settingsStorageLocation"));
		StorageLocation->setAccessibleName(QStringLiteral("Local application data location"));
		StorageLocation->setReadOnly(true);
		StorageLocation->setPlaceholderText(QStringLiteral("Location unavailable"));
		storageColumn->addWidget(StorageLocation);
		auto* const facts = new QLabel{ QStringLiteral("Local data includes repository caches, retained review history, drafts, notes, receipts and preferences. Retained epochs remain until explicitly pruned; maintenance can remove revisions needed for an older comparison. Moving this storage location and global purge are not available in this build."), storage };
		facts->setWordWrap(true);
		storageColumn->addWidget(facts);
		for (auto* action : { EpochSelectors->prune_action(), EpochSelectors->delete_cache_action(), EpochSelectors->cancel_maintenance_action() }) {
			QToolButton* const button = action == EpochSelectors->cancel_maintenance_action() ? static_cast<QToolButton*>(new quiet_tool_button{ storage }) : new QToolButton{ storage };
			button->setDefaultAction(action);
			button->setAccessibleName(action->text());
			storageColumn->addWidget(button);
		}
		auto* const privacy = new QLabel{ QStringLiteral("History maintenance above is limited to the selected review or repository cache and asks for confirmation. It does not change GitHub or your working copy. Diagnostic preview and local export are available in the profile controls. Removing the application does not remove its local data folder or system credential entries. Use Accounts & Hosts to sign out of remembered device sessions."), storage };
		privacy->setWordWrap(true);
		storageColumn->addWidget(privacy);
		// [[BPR-1602]]. The last group on the page, below diagnostics: four
		// resets of this profile's local data, each confirmed first.
		storageColumn->addSpacing(12);
		ProfileReset = new profile_reset_panel{ storage };
		ProfileReset->set_profile_label(ProfileLabel);
		storageColumn->addWidget(ProfileReset);
		SettingsWindow->adopt_panel(settings_page::storage_and_privacy, storage);
	}

	void workbench_window::set_profile_data_reset_dispatch(profile_reset_panel::dispatch aDispatch)
	{
		ProfileReset->set_dispatch(std::move(aDispatch));
	}

	void workbench_window::present_profile_data_reset(const core::profile_data_reset_outcome& aOutcome, const QString& aCopyFolder)
	{
		const auto notice = profile_data_reset_notice(aOutcome, aCopyFolder);
		statusBar()->showMessage(notice, 30000);
		ProfileReset->present_status(aOutcome.Reason == core::release_maintenance_reason::none ? QString{ } : notice);
	}

	void workbench_window::refuse_profile_data_reset(const QString& aReason)
	{
		ProfileReset->present_status(aReason);
	}

	auto workbench_window::profile_reset_panel_widget() const noexcept -> profile_reset_panel*
	{
		return ProfileReset;
	}

	void workbench_window::set_storage_location(const QString& aPath)
	{
		StorageLocation->setText(aPath);
		StorageLocation->setCursorPosition(0);
	}

	void workbench_window::build_queue_settings(QWidget* const aReviews, QVBoxLayout* const aColumn)
	{
		auto* const group   = new QGroupBox{ QStringLiteral("Queue defaults"), aReviews };
		auto* const form    = new QFormLayout{ group };
		const auto  sources = std::array{ QueueRoleFilter, QueueObserverFilter, QueueBucketFilter, QueueRepositoryFilter, QueueQuietFilter, QueueSort };
		const auto  labels  = std::array{ QStringLiteral("Role"), QStringLiteral("Observer reason"), QStringLiteral("Attention"), QStringLiteral("Repository"), QStringLiteral("Quiet reviews"), QStringLiteral("Sort") };
		// [[BPR-0783]]. The six controls themselves, in place of the selectors that
		// mirrored them. A mirror had to be kept in step by hand, which is a second
		// carrier for one choice; these are the same widgets Home used, with their
		// object names, their item data and their typed intents unchanged.
		for (auto i = std::size_t{ 0U }; i < sources.size(); ++i) {
			sources[i]->setParent(group);
			form->addRow(labels[i], sources[i]);
		}
		QueueDefaultsReset = new quiet_push_button{ QStringLiteral("Reset queue defaults"), group };
		form->addRow(QueueDefaultsReset);
		connect(QueueDefaultsReset, &QPushButton::clicked, this, [this] {
			if (!QueueReady || !QueuePreferencesDispatch) {
				return;
			}
			auto defaults          = application::queue_presentation_preferences{ };
			defaults.Notifications = QueueProjection.Preferences.Notifications;
			QueuePreferencesDispatch(application::queue_preferences_intent{ std::move(defaults), application::queue_preference_field::queue_filters });
		});
		aColumn->addWidget(group);
		SettingsQueueStatus = new QLabel{ aReviews };
		SettingsQueueStatus->setObjectName(QStringLiteral("settingsQueueStatus"));
		SettingsQueueStatus->setAccessibleName(QStringLiteral("Queue settings status"));
		SettingsQueueStatus->setWordWrap(true);
		aColumn->addWidget(SettingsQueueStatus);

		auto* const notifications = new QWidget;
		auto* const column        = new QVBoxLayout{ notifications };
		NotificationsEnabled      = new QCheckBox{ QStringLiteral("Enable review notifications"), notifications };
		NotificationsEnabled->setObjectName(QStringLiteral("notificationsEnabled"));
		NotificationsPaused = new QCheckBox{ QStringLiteral("Pause notifications"), notifications };
		NotificationsPaused->setObjectName(QStringLiteral("notificationsPaused"));
		column->addWidget(NotificationsEnabled);
		column->addWidget(NotificationsPaused);
		auto* const explanation = new QLabel{ QStringLiteral("Notifications contain the repository, review number, attention category and unread count. They never include titles, source code or comment text. Choosing a notification selects that review in Home.\n\nYour operating system controls permission and presentation. BetterPullReview cannot confirm every system permission setting."), notifications };
		explanation->setWordWrap(true);
		column->addWidget(explanation);
		NotificationPreview = new quiet_push_button{ QStringLiteral("Preview for selected Home review"), notifications };
		NotificationPreview->setObjectName(QStringLiteral("notificationPreview"));
		column->addWidget(NotificationPreview);
		NotificationReset = new quiet_push_button{ QStringLiteral("Reset notification preferences"), notifications };
		column->addWidget(NotificationReset);
		NotificationStatus = new QLabel{ notifications };
		NotificationStatus->setWordWrap(true);
		NotificationStatus->setAccessibleName(QStringLiteral("Notification status"));
		column->addWidget(NotificationStatus);
		// [[BPR-1850]]. Activity on my pull requests, from GitHub's notifications.
		ActivityNotifications = new QCheckBox{ QStringLiteral("Notify me about activity on my pull requests"), notifications };
		ActivityNotifications->setObjectName(QStringLiteral("activityNotifications"));
		ActivityNotifications->setChecked(true);
		ActivityNotifications->setEnabled(false);
		// The owner, 2026-11-03: less text, the detail behind an info button.
		// One row (the switch and its ⓘ), then one line saying what it is doing.
		auto* const activityRow    = new QWidget{ notifications };
		auto* const activityLayout = new QHBoxLayout{ activityRow };
		activityLayout->setContentsMargins(0, 0, 0, 0);
		activityLayout->setSpacing(4);
		activityLayout->addWidget(ActivityNotifications);
		ActivityInfo = new quiet_tool_button{ activityRow };
		ActivityInfo->setObjectName(QStringLiteral("activityNotificationsInfo"));
		ActivityInfo->setText(QStringLiteral("\u24D8"));
		ActivityInfo->setAutoRaise(true);
		ActivityInfo->setFocusPolicy(Qt::StrongFocus);
		ActivityInfo->setAccessibleName(QStringLiteral("About activity notifications"));
		const auto activityDetail = QStringLiteral(
			"New comments, reviews and mentions on pull requests you take part in, checked about once a minute. "
			"Nothing is announced for a pull request you have open in the window you are using. "
			"Notifications name the repository and number only, and nothing is marked read on GitHub. "
			"It needs a GitHub CLI sign-in.");
		ActivityInfo->setAccessibleDescription(activityDetail);
		// Hover is answered by the application's anchored tooltips, which wrap
		// long lines; a click or a key shows the same text anchored to the
		// button and held while it has focus.
		ActivityInfo->setToolTip(activityDetail);
		connect(ActivityInfo, &QToolButton::clicked, this, [this] {
			show_anchored_tooltip(*ActivityInfo, ActivityInfo->rect(), ActivityInfo->toolTip(), anchored_tooltip_hold::focus);
		});
		activityLayout->addWidget(ActivityInfo);
		activityLayout->addStretch(1);
		column->addSpacing(12);
		column->addWidget(activityRow);
		ActivityStatus = new QLabel{ notifications };
		ActivityStatus->setObjectName(QStringLiteral("activityNotificationsStatus"));
		ActivityStatus->setAccessibleName(QStringLiteral("Activity notification status"));
		// Under the switch's text, not its box.
		const auto indent = style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, ActivityNotifications) + style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, ActivityNotifications);
		ActivityStatus->setContentsMargins(indent, 0, 0, 0);
		column->addWidget(ActivityStatus);
		connect(ActivityNotifications, &QCheckBox::toggled, this, [this](const bool aChecked) {
			if (!PresentingActivity && ActivityDispatch) {
				ActivityDispatch(aChecked);
			}
		});
		const auto change = [this](const application::queue_preference_field aField) {
			if (ApplyingQueuePreferences || !QueueReady || !QueuePreferencesDispatch) {
				return;
			}
			auto next          = QueueProjection.Preferences;
			next.Notifications = { NotificationsEnabled->isChecked(), NotificationsPaused->isChecked() };
			apply_queue_preferences(QueueProjection.Preferences);
			QueuePreferencesDispatch(application::queue_preferences_intent{ std::move(next), aField });
		};
		connect(NotificationsEnabled, &QCheckBox::toggled, this, [change] { change(application::queue_preference_field::notifications_enabled); });
		connect(NotificationsPaused, &QCheckBox::toggled, this, [change] { change(application::queue_preference_field::notifications_paused); });
		connect(NotificationReset, &QPushButton::clicked, this, [this] {
			if (!QueueReady || !QueuePreferencesDispatch) {
				return;
			}
			auto next          = QueueProjection.Preferences;
			next.Notifications = { };
			QueuePreferencesDispatch(application::queue_preferences_intent{ std::move(next), application::queue_preference_field::notifications });
		});
		connect(NotificationPreview, &QPushButton::clicked, this, [this] {
			const auto item = selected_queue_item();
			if (!item) {
				NotificationStatus->setText(QStringLiteral("Select a review in Home first."));
				return;
			}
			const auto submitted = NotificationPreviewDispatch && NotificationPreviewDispatch(item->Ref);
			NotificationStatus->setText(submitted ? QStringLiteral("Preview submitted to the operating system.") : QStringLiteral("Preview was not submitted. Notifications may be paused, permission unavailable, or this review may have no new activity."));
		});
		SettingsWindow->adopt_panel(settings_page::notifications, notifications);
		update_notification_controls();
	}

	void workbench_window::set_activity_notifications_dispatch(std::function<void(bool)> aDispatch)
	{
		ActivityDispatch = std::move(aDispatch);
		if (ActivityNotifications != nullptr) {
			ActivityNotifications->setEnabled(static_cast<bool>(ActivityDispatch));
		}
	}

	void workbench_window::present_activity_notifications(const bool aEnabled, const QString& aStatus)
	{
		if (ActivityNotifications == nullptr) {
			return;
		}
		PresentingActivity = true;
		ActivityNotifications->setChecked(aEnabled);
		PresentingActivity = false;
		ActivityStatus->setText(aStatus);
	}

	auto workbench_window::activity_notifications_switch() const noexcept -> QCheckBox*
	{
		return ActivityNotifications;
	}

	auto workbench_window::activity_notifications_status() const noexcept -> QLabel*
	{
		return ActivityStatus;
	}

	auto workbench_window::activity_notifications_info() const noexcept -> QToolButton*
	{
		return ActivityInfo;
	}

	auto workbench_window::focused_pull_request() const -> std::optional<core::pull_request_ref>
	{
		if (!isActiveWindow()) {
			return std::nullopt;
		}
		return open_queue_ref();
	}

	auto workbench_window::shown_pull_request() const -> std::optional<core::pull_request_ref>
	{
		return showing_home() ? std::nullopt : open_queue_ref();
	}

	auto workbench_window::composing() const -> bool
	{
		auto* const focus = focusWidget();
		if (focus == nullptr || (focus != this && !isAncestorOf(focus))) {
			return false;
		}
		// A draft's editor is read-only for a moment while each edit is saved,
		// so a multi-line editor holding the focus counts whatever its state;
		// only a browser, which is never written in, does not.
		if (qobject_cast<const QTextBrowser*>(focus) != nullptr) {
			return false;
		}
		return qobject_cast<const QTextEdit*>(focus) != nullptr || qobject_cast<const QPlainTextEdit*>(focus) != nullptr;
	}

	void workbench_window::set_pull_request_return_dispatch(std::function<void(std::int64_t)> aDispatch)
	{
		PullRequestReturnDispatch = std::move(aDispatch);
	}

	auto workbench_window::open_notified_pull_request(const std::optional<core::pull_request_ref>& aRef) -> bool
	{
		if (aRef && open_queue_ref() != aRef) {
			if (!GithubDispatch || !QueueReady || QueueProjection.Session.Host != aRef->Host || QueueProjection.Session.Account != aRef->Account) {
				return false;
			}
			open_pull_request_item(core::queue_item{ .Ref = *aRef });
		}
		if (isMinimized()) {
			showNormal();
		}
		raise();
		activateWindow();
		return true;
	}

	void workbench_window::set_notification_preview_dispatch(std::function<bool(const core::pull_request_ref&)> aDispatch)
	{
		NotificationPreviewDispatch = std::move(aDispatch);
		update_notification_controls();
	}

	void workbench_window::update_notification_controls()
	{
		if (NotificationsEnabled == nullptr) {
			return;
		}
		const auto writable = QueueReady && static_cast<bool>(QueuePreferencesDispatch);
		NotificationsEnabled->setEnabled(writable);
		NotificationsPaused->setEnabled(writable);
		NotificationReset->setEnabled(writable);
		QueueDefaultsReset->setEnabled(writable);
		NotificationPreview->setEnabled(writable && static_cast<bool>(NotificationPreviewDispatch)
										&& QueueProjection.Preferences.Notifications.Enabled && !QueueProjection.Preferences.Notifications.Paused);
		NotificationStatus->setText(!writable                         ? QStringLiteral("Preferences are unavailable or could not be saved; the last accepted choices remain in effect.")
										: NotificationPreviewDispatch ? QStringLiteral("Notification delivery is available. System permission may still prevent presentation.")
																	  : QStringLiteral("Notification delivery is unavailable in this environment."));
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
		apply_queue_preferences(QueueProjection.Preferences);
	}

	void workbench_window::dispatch_queue_preferences(QComboBox* const aSource)
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
		const auto field          = aSource == QueueRoleFilter ? application::queue_preference_field::role
			: aSource == QueueObserverFilter                   ? application::queue_preference_field::observer
			: aSource == QueueBucketFilter                     ? application::queue_preference_field::bucket
			: aSource == QueueRepositoryFilter                 ? application::queue_preference_field::repository
			: aSource == QueueQuietFilter                      ? application::queue_preference_field::quiet
															   : application::queue_preference_field::sort;
		preferences.Notifications = QueueProjection.Preferences.Notifications;
		apply_queue_preferences(QueueProjection.Preferences);
		QueuePreferencesDispatch(application::queue_preferences_intent{
			.Preferences = std::move(preferences),
			.Field       = field,
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
		// [[BPR-0783]]. Home filters by what its own view says: the window's four
		// values once it owns them, otherwise the profile's, and never Repository
		// or Bucket. The controls above keep showing the profile's answer.
		QueueFilterModel->set_home_view(HomeView);
		if (!HomeView.FilterOwned) {
			// [[BPR-0783]] review finding. Until its first Filter gesture the
			// window follows the profile, so an accepted preference changes what
			// Home is filtered by without any view of its own changing. The Filter
			// control is re-read from it, exactly as the six combos above are.
			apply_home_filter_controls();
		}
		if (NotificationsEnabled != nullptr) {
			NotificationsEnabled->setChecked(aPreferences.Notifications.Enabled);
			NotificationsPaused->setChecked(aPreferences.Notifications.Paused);
		}
		ApplyingQueuePreferences = false;
		update_notification_controls();
	}
}
