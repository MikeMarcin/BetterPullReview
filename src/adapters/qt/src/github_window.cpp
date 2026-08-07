#include "bpr/qt/github_window.hpp"
#include "bpr/qt/github_conversation_delegate.hpp"

#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>

#include "github_presentation_text.hpp"

namespace bpr::qt {
	// Extracted by BPR-0195 so the session panel can say the same words.
	using namespace detail;

	github_window::github_window(
		github_view_config                  aConfig,
		application::github_intent_dispatch aDispatch,
		QWidget* const                      aParent)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		std::unordered_set<std::uint64_t> profileIds;
		profileIds.reserve(aConfig.Profiles.size());
		for (auto& profile : aConfig.Profiles) {
			if (
				valid(profile)
				&& profileIds.insert(profile.Profile.value()).second) {
				Profiles.push_back(std::move(profile));
			}
		}

		setObjectName(QStringLiteral("githubWindow"));
		setWindowTitle(QStringLiteral("BetterPullReview — GitHub"));
		setMinimumSize(640, 420);
		resize(1100, 720);

		ConnectAction = new QAction{ QStringLiteral("&Connect"), this };
		ConnectAction->setObjectName(QStringLiteral("connectGithubAction"));
		ConnectAction->setShortcut(
			QKeySequence{ QStringLiteral("Ctrl+Shift+C") });
		ConnectAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(ConnectAction);
		OpenAction = new QAction{ QStringLiteral("&Open pull request"), this };
		OpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
		OpenAction->setShortcut(QKeySequence::Open);
		OpenAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(OpenAction);
		RefreshAction = new QAction{ QStringLiteral("&Refresh"), this };
		RefreshAction->setObjectName(QStringLiteral("refreshPullRequestAction"));
		RefreshAction->setShortcut(QKeySequence::Refresh);
		RefreshAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(RefreshAction);
		ShowReviewThreadsAction = new QAction{
			QStringLiteral("Show review &threads"),
			this,
		};
		ShowReviewThreadsAction->setObjectName(
			QStringLiteral("showReviewThreadsAction"));
		addAction(ShowReviewThreadsAction);

		auto* const presentationScroll = new QScrollArea{ this };
		presentationScroll->setObjectName(
			QStringLiteral("githubPresentationScroll"));
		presentationScroll->setAccessibleName(
			QStringLiteral("GitHub pull request presentation"));
		presentationScroll->setWidgetResizable(true);
		presentationScroll->setHorizontalScrollBarPolicy(
			Qt::ScrollBarAlwaysOff);
		presentationScroll->setFrameShape(QFrame::NoFrame);

		auto* const central = new QWidget{ presentationScroll };
		central->setObjectName(QStringLiteral("githubPresentation"));
		central->setMinimumHeight(500);
		auto* const centralLayout    = new QVBoxLayout{ central };
		const auto  horizontalMargin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, central);
		const auto  verticalMargin   = style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, central);
		centralLayout->setContentsMargins(
			horizontalMargin,
			verticalMargin,
			horizontalMargin,
			verticalMargin);
		centralLayout->setSpacing(
			style()->pixelMetric(
				QStyle::PM_LayoutVerticalSpacing,
				nullptr,
				central));

		// BPR-0195: the session chrome is a composable panel now, so BPR-0191
		// can move it into the workbench shell without moving the review
		// content in the same commit. Every accessor this window published
		// before still exists and now delegates, which is what lets the
		// window's tests stay untouched.
		SessionPanel = new github_session_panel{
			Profiles,
			Dispatch,
			ConnectAction,
			OpenAction,
			RefreshAction,
			github_session_environment{
				.HasPullRequest = [this] { return HasPullRequest; },
				.PresentationChanged = [this] { update_presentation_state(); },
			},
			central,
		};
		centralLayout->addWidget(SessionPanel);

		PullRequestStack = new QStackedWidget{ central };
		PullRequestStack->setObjectName(QStringLiteral("githubViewState"));
		PullRequestStack->setAccessibleName(
			QStringLiteral("GitHub pull request state"));

		auto* const emptyState = new QWidget{ PullRequestStack };
		emptyState->setObjectName(QStringLiteral("githubEmptyState"));
		auto* const emptyLayout = new QVBoxLayout{ emptyState };
		emptyLayout->addStretch(1);
		EmptyStateTitleLabel = new QLabel{ emptyState };
		EmptyStateTitleLabel->setObjectName(
			QStringLiteral("githubEmptyStateTitle"));
		EmptyStateTitleLabel->setAccessibleName(
			QStringLiteral("GitHub view state"));
		EmptyStateTitleLabel->setAlignment(Qt::AlignCenter);
		EmptyStateTitleLabel->setWordWrap(true);
		set_plain_text_label(EmptyStateTitleLabel);
		emptyLayout->addWidget(EmptyStateTitleLabel);
		EmptyStateBodyLabel = new QLabel{ emptyState };
		EmptyStateBodyLabel->setObjectName(
			QStringLiteral("githubEmptyStateBody"));
		EmptyStateBodyLabel->setAccessibleName(
			QStringLiteral("GitHub view guidance"));
		EmptyStateBodyLabel->setAlignment(Qt::AlignCenter);
		EmptyStateBodyLabel->setWordWrap(true);
		set_plain_text_label(EmptyStateBodyLabel);
		emptyLayout->addWidget(EmptyStateBodyLabel);
		emptyLayout->addStretch(2);
		PullRequestStack->addWidget(emptyState);

		auto* const pullRequestContent = new QWidget{ PullRequestStack };
		pullRequestContent->setObjectName(
			QStringLiteral("githubPullRequestContent"));
		auto* const pullRequestLayout = new QVBoxLayout{ pullRequestContent };
		pullRequestLayout->setContentsMargins(0, 0, 0, 0);

		auto* const metadataGroup = new QFrame{ pullRequestContent };
		metadataGroup->setFrameShape(QFrame::NoFrame);
		metadataGroup->setObjectName(QStringLiteral("pullRequestMetadataGroup"));
		metadataGroup->setAccessibleName(QStringLiteral("Pull request metadata"));
		auto* const metadataLayout = new QGridLayout{ metadataGroup };
		metadataLayout->setContentsMargins(0, 0, 0, 0);
		metadataLayout->setColumnStretch(0, 1);

		PullRequestLabel = new QLabel{ metadataGroup };
		PullRequestLabel->setObjectName(QStringLiteral("pullRequestIdentity"));
		PullRequestLabel->setAccessibleName(
			QStringLiteral("Pull request identity"));
		set_plain_text_label(PullRequestLabel);
		TitleLabel = new QLabel{ metadataGroup };
		TitleLabel->setObjectName(QStringLiteral("pullRequestTitle"));
		TitleLabel->setAccessibleName(QStringLiteral("Pull request title"));
		TitleLabel->setWordWrap(true);
		set_plain_text_label(TitleLabel);
		StateLabel = new QLabel{ metadataGroup };
		StateLabel->setObjectName(QStringLiteral("pullRequestState"));
		StateLabel->setAccessibleName(QStringLiteral("Pull request state"));
		set_plain_text_label(StateLabel);
		AuthorLabel = new QLabel{ metadataGroup };
		AuthorLabel->setObjectName(QStringLiteral("pullRequestAuthor"));
		AuthorLabel->setAccessibleName(QStringLiteral("Pull request author"));
		set_plain_text_label(AuthorLabel);
		BranchLabel = new QLabel{ metadataGroup };
		BranchLabel->setObjectName(QStringLiteral("pullRequestBranches"));
		BranchLabel->setAccessibleName(QStringLiteral("Pull request branches"));
		BranchLabel->setWordWrap(true);
		set_plain_text_label(BranchLabel);
		OidLabel = new QLabel{ metadataGroup };
		OidLabel->setObjectName(QStringLiteral("pullRequestCommitOids"));
		OidLabel->setAccessibleName(
			QStringLiteral("Pull request commit identities"));
		OidLabel->setWordWrap(true);
		set_plain_text_label(OidLabel);
		CountLabel = new QLabel{ metadataGroup };
		CountLabel->setObjectName(QStringLiteral("pullRequestCounts"));
		CountLabel->setAccessibleName(QStringLiteral("Pull request record counts"));
		CountLabel->setWordWrap(true);
		set_plain_text_label(CountLabel);
		BodyEditor = new QPlainTextEdit{ metadataGroup };
		BodyEditor->setObjectName(QStringLiteral("pullRequestBody"));
		BodyEditor->setAccessibleName(QStringLiteral("Pull request description"));
		BodyEditor->setReadOnly(true);
		BodyEditor->setTabChangesFocus(true);
		BodyEditor->setMinimumHeight(120);

		TechnicalDetailsButton = new QToolButton{ metadataGroup };
		TechnicalDetailsButton->setObjectName(
			QStringLiteral("githubTechnicalDetailsButton"));
		TechnicalDetailsButton->setText(QStringLiteral("Technical details"));
		TechnicalDetailsButton->setAccessibleName(
			QStringLiteral("Show pull request technical details"));
		TechnicalDetailsButton->setCheckable(true);
		TechnicalDetailsButton->setArrowType(Qt::RightArrow);
		TechnicalDetailsButton->setToolButtonStyle(
			Qt::ToolButtonTextBesideIcon);
		TechnicalDetails = new QWidget{ metadataGroup };
		TechnicalDetails->setObjectName(
			QStringLiteral("githubTechnicalDetails"));
		auto* const technicalLayout = new QHBoxLayout{ TechnicalDetails };
		technicalLayout->setContentsMargins(0, 0, 0, 0);
		auto* const commitsLabel = new QLabel{ QStringLiteral("Commit identities:"), TechnicalDetails };
		technicalLayout->addWidget(commitsLabel);
		technicalLayout->addWidget(OidLabel, 1);
		TechnicalDetails->hide();

		metadataLayout->addWidget(PullRequestLabel, 0, 0);
		metadataLayout->addWidget(
			StateLabel,
			0,
			1,
			Qt::AlignRight | Qt::AlignVCenter);
		metadataLayout->addWidget(TitleLabel, 1, 0, 1, 2);
		metadataLayout->addWidget(AuthorLabel, 2, 0);
		metadataLayout->addWidget(BranchLabel, 3, 0, 1, 2);
		metadataLayout->addWidget(CountLabel, 4, 0);
		metadataLayout->addWidget(
			TechnicalDetailsButton,
			4,
			1,
			Qt::AlignRight | Qt::AlignVCenter);
		metadataLayout->addWidget(TechnicalDetails, 5, 0, 1, 2);
		pullRequestLayout->addWidget(metadataGroup);

		auto* const comparisonGroup = new QGroupBox{ QStringLiteral("Review comparison"), pullRequestContent };
		comparisonGroup->setObjectName(
			QStringLiteral("epochComparisonGroup"));
		comparisonGroup->setAccessibleName(
			QStringLiteral("Review epoch comparison controls"));
		auto* const comparisonLayout = new QVBoxLayout{ comparisonGroup };
		EpochControl                 = new epoch_comparison_control{ { }, comparisonGroup };
		comparisonLayout->addWidget(EpochControl);
		pullRequestLayout->addWidget(comparisonGroup);

		auto* const diffGroup = new QGroupBox{
			QStringLiteral("Selected file comparison"), pullRequestContent
		};
		diffGroup->setObjectName(QStringLiteral("diffWorkbenchGroup"));
		diffGroup->setAccessibleName(QStringLiteral("Selected file comparison"));
		auto* const diffLayout = new QVBoxLayout{ diffGroup };
		DiffWorkbench          = new diff_workbench{ diffGroup };
		DiffWorkbench->setMinimumHeight(430);
		diffLayout->addWidget(DiffWorkbench);
		pullRequestLayout->addWidget(diffGroup, 1);

		ContentSplitter = new QSplitter{
			Qt::Horizontal,
			pullRequestContent,
		};
		ContentSplitter->setObjectName(QStringLiteral("githubContentSplitter"));
		ContentSplitter->setAccessibleName(
			QStringLiteral("Pull request content"));
		ContentSplitter->setChildrenCollapsible(false);

		auto* const descriptionGroup = new QGroupBox{ QStringLiteral("Description"), ContentSplitter };
		descriptionGroup->setObjectName(
			QStringLiteral("pullRequestDescriptionGroup"));
		descriptionGroup->setAccessibleName(
			QStringLiteral("Pull request description"));
		auto* const descriptionLayout = new QVBoxLayout{ descriptionGroup };
		BodyEditor->setParent(descriptionGroup);
		descriptionLayout->addWidget(BodyEditor);

		auto* const conversationGroup = new QGroupBox{ QStringLiteral("Conversations"), ContentSplitter };
		conversationGroup->setObjectName(
			QStringLiteral("pullRequestConversationGroup"));
		conversationGroup->setAccessibleName(
			QStringLiteral("Read-only GitHub conversations"));
		auto* const conversationLayout = new QVBoxLayout{ conversationGroup };
		ConversationAvailabilityLabel  = new QLabel{ conversationGroup };
		ConversationAvailabilityLabel->setObjectName(
			QStringLiteral("reviewThreadAvailability"));
		ConversationAvailabilityLabel->setAccessibleName(
			QStringLiteral("Review-thread availability"));
		ConversationAvailabilityLabel->setWordWrap(true);
		set_plain_text_label(ConversationAvailabilityLabel);
		auto* const conversationHeader = new QHBoxLayout;
		conversationHeader->addWidget(ConversationAvailabilityLabel, 1);
		ShowReviewThreadsButton = new QToolButton{ conversationGroup };
		ShowReviewThreadsButton->setDefaultAction(ShowReviewThreadsAction);
		ShowReviewThreadsButton->setObjectName(
			QStringLiteral("showReviewThreadsButton"));
		ShowReviewThreadsButton->setAccessibleName(
			QStringLiteral("Show review threads"));
		ShowReviewThreadsButton->setFocusPolicy(Qt::StrongFocus);
		ShowReviewThreadsButton->setToolButtonStyle(
			Qt::ToolButtonTextBesideIcon);
		conversationHeader->addWidget(ShowReviewThreadsButton);
		conversationLayout->addLayout(conversationHeader);

		ConversationStack = new QStackedWidget{ conversationGroup };
		ConversationStack->setObjectName(
			QStringLiteral("githubConversationState"));
		auto* const conversationEmpty = new QWidget{ ConversationStack };
		conversationEmpty->setObjectName(
			QStringLiteral("githubConversationEmptyState"));
		ConversationStack->addWidget(conversationEmpty);
		ConversationModel = new github_conversation_model{ this };
		ConversationView  = new QTreeView{ ConversationStack };
		ConversationView->setObjectName(QStringLiteral("githubConversationView"));
		ConversationView->setAccessibleName(
			QStringLiteral("Read-only GitHub conversations"));
		ConversationView->setAccessibleDescription(
			QStringLiteral(
				"Reviews, review threads, and pull request comments"));
		ConversationView->setModel(ConversationModel);
		ConversationView->setSelectionBehavior(QAbstractItemView::SelectRows);
		ConversationView->setSelectionMode(QAbstractItemView::SingleSelection);
		ConversationView->setEditTriggers(QAbstractItemView::NoEditTriggers);
		ConversationView->setRootIsDecorated(true);
		ConversationView->setItemsExpandable(true);
		ConversationView->setWordWrap(true);
		ConversationView->setUniformRowHeights(false);
		ConversationView->setTextElideMode(Qt::ElideNone);
		ConversationView->header()->hide();
		ConversationView->setItemDelegate(
			new github_conversation_delegate{ ConversationView });
		ConversationStack->addWidget(ConversationView);
		conversationLayout->addWidget(ConversationStack, 1);

		ContentSplitter->addWidget(descriptionGroup);
		ContentSplitter->addWidget(conversationGroup);
		ContentSplitter->setStretchFactor(0, 2);
		ContentSplitter->setStretchFactor(1, 3);
		ContentSplitter->setMinimumHeight(240);
		pullRequestLayout->addWidget(ContentSplitter, 1);
		PullRequestStack->addWidget(pullRequestContent);
		centralLayout->addWidget(PullRequestStack, 1);

		// The settings surface lives with the presentation it governs rather
		// than behind a modal dialog, so a reviewer sees a font or mode change
		// take effect on the very comparison they are reading. It translates
		// gestures only: the composition wires its dispatch to the appearance
		// host, and until it does, every gesture goes nowhere visible - which
		// is the honest state for an unwired surface.
		auto* const appearanceGroup = new QGroupBox{
			QStringLiteral("Appearance"),
			central,
		};
		appearanceGroup->setObjectName(QStringLiteral("appearanceSettingsGroup"));
		auto* const appearanceLayout = new QVBoxLayout{ appearanceGroup };
		AppearanceSettings           = new appearance_settings_panel{ { }, appearanceGroup };
		AppearanceSettings->setObjectName(
			QStringLiteral("appearanceSettingsPanel"));
		appearanceLayout->addWidget(AppearanceSettings);
		centralLayout->addWidget(appearanceGroup);

		presentationScroll->setWidget(central);
		setCentralWidget(presentationScroll);
		statusBar()->hide();

		connect(
			TechnicalDetailsButton,
			&QToolButton::toggled,
			this,
			[this](const bool aVisible) {
				TechnicalDetails->setVisible(aVisible);
				TechnicalDetailsButton->setArrowType(
					aVisible ? Qt::DownArrow : Qt::RightArrow);
				TechnicalDetailsButton->setAccessibleName(
					aVisible
						? QStringLiteral("Hide pull request technical details")
						: QStringLiteral("Show pull request technical details"));
			});
		connect(ShowReviewThreadsAction,
				&QAction::triggered,
				this,
				[this] { focus_review_threads(); });

		// Set on the window, because tab order is a property of the assembled
		// window rather than of either piece. The chrome stops come from the
		// panel so that the sequence is unchanged by the extraction.
		setTabOrder(SessionPanel->profile_selector(), SessionPanel->connect_button());
		setTabOrder(
			SessionPanel->connect_button(),
			SessionPanel->connection_details_button());
		setTabOrder(
			SessionPanel->connection_details_button(),
			SessionPanel->owner_editor());
		setTabOrder(SessionPanel->owner_editor(), SessionPanel->repository_editor());
		setTabOrder(
			SessionPanel->repository_editor(), SessionPanel->pull_request_editor());
		setTabOrder(SessionPanel->pull_request_editor(), SessionPanel->open_button());
		setTabOrder(SessionPanel->open_button(), SessionPanel->refresh_button());
		setTabOrder(SessionPanel->refresh_button(), TechnicalDetailsButton);
		setTabOrder(TechnicalDetailsButton, BodyEditor);
		setTabOrder(BodyEditor, ShowReviewThreadsButton);
		setTabOrder(ShowReviewThreadsButton, ConversationView);

		SessionPanel->apply_capabilities({ });
		SessionPanel->apply_rate_limits({ });
		clear_pull_request();
		SessionPanel->update_selected_profile();
		SessionPanel->update_connect_enabled();
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
		apply_action_state(
			ShowReviewThreadsAction,
			ShowReviewThreadsButton,
			false,
			core::github_reason_code::no_active_pull_request,
			QStringLiteral("Focus the review-thread conversations"));
		refresh_native_appearance();
		update_presentation_state();
		update_splitter_orientation();
	}

	void github_window::publish(
		const application::github_presentation_update& aUpdate)
	{
		if (aUpdate.Transition.has_value()) {
			apply_transition(*aUpdate.Transition);
		}
		if (aUpdate.Session.has_value()) {
			SessionPanel->apply_session(*aUpdate.Session);
		}
		if (aUpdate.PullRequest.has_value()) {
			apply_pull_request(*aUpdate.PullRequest);
		}
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			apply_host_diagnostic(diagnostic);
		}
		announce_presentation_update();
	}

	auto github_window::connect_action() const noexcept -> QAction*
	{
		return ConnectAction;
	}

	auto github_window::open_action() const noexcept -> QAction*
	{
		return OpenAction;
	}

	auto github_window::refresh_action() const noexcept -> QAction*
	{
		return RefreshAction;
	}

	auto github_window::show_review_threads_action() const noexcept -> QAction*
	{
		return ShowReviewThreadsAction;
	}

	auto github_window::profile_selector() const noexcept -> QComboBox*
	{
		return SessionPanel->profile_selector();
	}

	auto github_window::owner_editor() const noexcept -> QLineEdit*
	{
		return SessionPanel->owner_editor();
	}

	auto github_window::repository_editor() const noexcept -> QLineEdit*
	{
		return SessionPanel->repository_editor();
	}

	auto github_window::pull_request_editor() const noexcept -> QSpinBox*
	{
		return SessionPanel->pull_request_editor();
	}

	auto github_window::conversation_view() const noexcept -> QTreeView*
	{
		return ConversationView;
	}

	auto github_window::conversation_model() const noexcept
		-> github_conversation_model*
	{
		return ConversationModel;
	}

	auto github_window::epoch_control() const noexcept
		-> epoch_comparison_control*
	{
		return EpochControl;
	}

	auto github_window::diff_comparison_workbench() const noexcept
		-> diff_workbench*
	{
		return DiffWorkbench;
	}

	auto github_window::appearance_settings() const noexcept
		-> appearance_settings_panel*
	{
		return AppearanceSettings;
	}

	auto github_window::status_text() const -> std::string
	{
		return SessionPanel->status_text();
	}

	auto github_window::diagnostic_text() const -> std::string
	{
		return SessionPanel->diagnostic_text();
	}

	auto github_window::active_host_text() const -> std::string
	{
		return SessionPanel->active_host_text();
	}

	auto github_window::active_account_text() const -> std::string
	{
		return SessionPanel->active_account_text();
	}

	auto github_window::statistics() const noexcept -> github_window_statistics
	{
		// Summed rather than moved. Every counter the window published before
		// BPR-0195 still counts the same events; some of those events are now
		// applied by the session panel, and a caller cannot tell - which is the
		// whole point of a behaviour-preserving extraction.
		const auto session = SessionPanel->statistics();
		auto       total   = Statistics;
		total.CommandUpdates += session.CommandUpdates;
		total.StatusUpdates += session.StatusUpdates;
		total.IdentityUpdates += session.IdentityUpdates;
		total.CapabilityUpdates += session.CapabilityUpdates;
		total.SessionUpdates += session.SessionUpdates;
		total.DiagnosticUpdates += session.DiagnosticUpdates;
		return total;
	}

	void github_window::changeEvent(QEvent* const aEvent)
	{
		QMainWindow::changeEvent(aEvent);
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

	void github_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QMainWindow::resizeEvent(aEvent);
		update_splitter_orientation();
	}

	void github_window::apply_transition(
		const application::github_transition_projection& aTransition)
	{
		for (const auto& command : aTransition.Commands) {
			if (!SessionPanel->apply_command(command)) {
				apply_content_command(command);
			}
		}
		if (aTransition.Identity.has_value()) {
			SessionPanel->apply_identity(*aTransition.Identity);
		}
		if (aTransition.Capabilities.has_value()) {
			SessionPanel->apply_capabilities(
				aTransition.Capabilities->Capabilities);
		}
		if (
			aTransition.PullRequest.has_value()
			&& !aTransition.PullRequest->Summary.has_value()) {
			clear_pull_request();
		}
		if (aTransition.Status.has_value()) {
			SessionPanel->apply_status(aTransition.Status->Status);
		}
		for (const auto& diagnostic : aTransition.Diagnostics) {
			SessionPanel->apply_diagnostic(diagnostic.Code);
		}
	}

	void github_window::apply_pull_request(
		const application::github_pull_request_projection& aPullRequest)
	{
		const auto selectedIdentity = ConversationModel->id_for_index(ConversationView->currentIndex());

		PullRequestLabel->setText(
			QStringLiteral("%1/%2 #%3")
				.arg(
					text(aPullRequest.Owner),
					text(aPullRequest.RepositoryName),
					QString::number(aPullRequest.Number.value())));
		TitleLabel->setText(text(aPullRequest.Title));
		StateLabel->setText(
			pull_request_state_text(aPullRequest.State, aPullRequest.Draft));
		AuthorLabel->setText(
			aPullRequest.AuthorLogin.empty()
				? QStringLiteral("Unknown author")
				: QStringLiteral("@%1").arg(text(aPullRequest.AuthorLogin)));
		BranchLabel->setText(
			QStringLiteral("%1 → %2")
				.arg(text(aPullRequest.BaseRef), text(aPullRequest.HeadRef)));
		OidLabel->setText(
			QStringLiteral("Base %1\nHead %2")
				.arg(text(aPullRequest.BaseOid), text(aPullRequest.HeadOid)));
		CountLabel->setText(
			QStringLiteral(
				"%1 files · %2 commits · %3 reviews · %4 review threads · "
				"%5 pull request comments")
				.arg(
					QString::number(aPullRequest.FileCount),
					QString::number(aPullRequest.CommitCount),
					QString::number(aPullRequest.ReviewCount),
					QString::number(aPullRequest.ReviewThreadCount),
					QString::number(aPullRequest.IssueCommentCount)));
		BodyEditor->setPlainText(text(aPullRequest.Body));
		ConversationAvailabilityLabel->setText(
			review_thread_availability_text(
				aPullRequest.ReviewThreadsAvailable,
				aPullRequest.ReviewThreadCount,
				aPullRequest.Capabilities.ReviewThreads));
		ConversationModel->apply(aPullRequest.Conversations);
		HasPullRequest = true;
		ConversationStack->setCurrentIndex(
			aPullRequest.Conversations.empty() ? 0 : 1);
		ConversationView->doItemsLayout();
		if (
			ExpandConversationsOnNextProjection
			&& !aPullRequest.Conversations.empty()) {
			ConversationView->expandAll();
			ExpandConversationsOnNextProjection = false;
		}
		if (!selectedIdentity.empty()) {
			const auto restored = ConversationModel->index_for_id(selectedIdentity);
			if (restored.isValid()) {
				const QSignalBlocker blocker{ ConversationView->selectionModel() };
				ConversationView->setCurrentIndex(restored);
			}
		}
		SessionPanel->apply_capabilities(aPullRequest.Capabilities);
		SessionPanel->apply_rate_limits(aPullRequest.RateLimits);
		update_presentation_state();
		++Statistics.PullRequestUpdates;
	}

	/// \brief Applies the one command projection the session panel does not own.
	void github_window::apply_content_command(
		const core::github_command_projection& aCommand)
	{
		if (aCommand.Id != core::github_command_id::show_review_threads) {
			return;
		}
		apply_action_state(
			ShowReviewThreadsAction,
			ShowReviewThreadsButton,
			aCommand.Enabled,
			aCommand.DisabledReason,
			QStringLiteral("Focus the review-thread conversations"));
		++Statistics.CommandUpdates;
	}

	void github_window::apply_host_diagnostic(
		const application::github_host_diagnostic_projection& aDiagnostic)
	{
		SessionPanel->apply_rate_limits(aDiagnostic.RateLimits);
		SessionPanel->apply_diagnostic(
			aDiagnostic.Reason,
			aDiagnostic.RetryAfterSeconds,
			aDiagnostic.RateLimitCause,
			true);
	}

	void github_window::clear_pull_request()
	{
		HasPullRequest                      = false;
		ExpandConversationsOnNextProjection = true;
		PullRequestLabel->setText(QStringLiteral("No pull request is open"));
		TitleLabel->setText(QStringLiteral("Not available"));
		StateLabel->setText(QStringLiteral("Not available"));
		AuthorLabel->setText(QStringLiteral("Not available"));
		BranchLabel->setText(QStringLiteral("Not available"));
		OidLabel->setText(QStringLiteral("Not available"));
		CountLabel->setText(QStringLiteral("No records"));
		BodyEditor->clear();
		ConversationAvailabilityLabel->setText(
			QStringLiteral(
				"Review threads are unavailable until a pull request is open."));
		ConversationModel->clear();
		ConversationStack->setCurrentIndex(0);
		TechnicalDetailsButton->setChecked(false);
		update_presentation_state();
	}

	void github_window::update_presentation_state()
	{
		PullRequestStack->setCurrentIndex(HasPullRequest ? 1 : 0);

		QString state;
		if (HasPullRequest) {
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
					Profiles.empty()
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

		SessionPanel->status_banner()->setAccessibleDescription(
			presentation_summary_text());
	}

	void github_window::update_splitter_orientation()
	{
		const auto narrow = width() < 820;
		SessionPanel->apply_compact_layout(narrow);

		if (ContentSplitter == nullptr) {
			return;
		}
		const auto desired = narrow ? Qt::Vertical : Qt::Horizontal;
		if (ContentSplitter->orientation() == desired) {
			return;
		}
		ContentSplitter->setOrientation(desired);
		const auto extent = desired == Qt::Horizontal
			? std::max(ContentSplitter->width(), 500)
			: std::max(ContentSplitter->height(), 400);
		ContentSplitter->setSizes({ extent * 2 / 5, extent * 3 / 5 });
	}

	void github_window::refresh_native_appearance()
	{
		auto titleFont = font();
		titleFont.setWeight(QFont::DemiBold);
		if (titleFont.pointSizeF() > 0.0) {
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.35);
		}
		TitleLabel->setFont(titleFont);

		auto emptyTitleFont = font();
		emptyTitleFont.setWeight(QFont::DemiBold);
		if (emptyTitleFont.pointSizeF() > 0.0) {
			emptyTitleFont.setPointSizeF(
				emptyTitleFont.pointSizeF() * 1.2);
		}
		EmptyStateTitleLabel->setFont(emptyTitleFont);

		auto emphasisFont = font();
		emphasisFont.setWeight(QFont::DemiBold);
		PullRequestLabel->setFont(emphasisFont);
		StateLabel->setFont(emphasisFont);
		SessionPanel->refresh_native_appearance();

		ShowReviewThreadsAction->setIcon(
			style()->standardIcon(QStyle::SP_FileDialogListView));

		ConversationView->doItemsLayout();
		ConversationView->viewport()->update();
		PullRequestStack->update();
	}

	void github_window::announce_presentation_update()
	{
		const auto announcement = presentation_summary_text();
		if (announcement.isEmpty() || announcement == LastAnnouncement) {
			return;
		}
		LastAnnouncement = announcement;
		QAccessibleAnnouncementEvent event{ SessionPanel->status_banner(),
											announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto github_window::presentation_summary_text() const -> QString
	{
		return SessionPanel->summary_text();
	}

	void github_window::focus_review_threads()
	{
		for (auto row = 0; row < ConversationModel->rowCount(); ++row) {
			const auto modelIndex = ConversationModel->index(row, 0);
			if (
				modelIndex.data(github_conversation_model::kind_role).toInt()
				== static_cast<int>(
					application::github_conversation_kind::review_thread)) {
				ConversationView->expand(modelIndex);
				ConversationView->setCurrentIndex(modelIndex);
				ConversationView->scrollTo(
					modelIndex, QAbstractItemView::PositionAtCenter);
				break;
			}
		}
		ConversationView->setFocus(Qt::ShortcutFocusReason);
	}

}
