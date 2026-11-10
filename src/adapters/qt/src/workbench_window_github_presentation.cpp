#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/window_title.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/identity_avatar_pictures.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/usage_ring.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "github_presentation_text.hpp"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QAnyStringView>
#include <QBoxLayout>
#include <QByteArrayView>
#include <QChar>
#include <QComboBox>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QString>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// [[BPR-1575]]. What a refresh compares of one accepted read; defined below.
		[[nodiscard]] auto refresh_observation_of(const application::github_pull_request_projection& aPullRequest) -> core::pull_request_refresh_observation;

		/// [[BPR-1575]]. What a refresh compares of one accepted read.
		[[nodiscard]] auto refresh_observation_of(const application::github_pull_request_projection& aPullRequest) -> core::pull_request_refresh_observation
		{
			auto observation = core::pull_request_refresh_observation{ .Head = aPullRequest.HeadOid, .Commits = aPullRequest.CommitCount };
			for (const auto& conversation : aPullRequest.Conversations) {
				if (conversation.Deleted) {
					continue;
				}
				switch (conversation.Kind) {
				case application::github_conversation_kind::review_comment:
				case application::github_conversation_kind::issue_comment:
					observation.Comments.push_back(conversation.Id);
					break;
				case application::github_conversation_kind::review:
					observation.Reviews.push_back(conversation.Id);
					break;
				case application::github_conversation_kind::review_thread:
					break;
				}
			}
			switch (aPullRequest.State) {
			case application::github_pull_request_state_projection::merged: observation.Status = core::refresh_status::merged; break;
			case application::github_pull_request_state_projection::closed: observation.Status = core::refresh_status::closed; break;
			case application::github_pull_request_state_projection::open:
				observation.Status = aPullRequest.Draft ? core::refresh_status::draft : core::refresh_status::open;
				break;
			}
			return observation;
		}
	}

	void workbench_window::announce_github_presentation_update()
	{
		// One polite announcement per material change, not one per projection
		// field. A screen reader that is told the same thing twice has been told
		// nothing the second time.
		if (SessionPanel == nullptr) {
			return;
		}
		// [[BPR-0771]]. Attributed to where the change is said: a pull
		// request's status to Home, whose bar carries its refusal, and a
		// session's to its host's card.
		QObject* source = current_server_card();
		switch (SessionPanel->status()) {
		case core::github_status::loading_pull_request:
		case core::github_status::pull_request_ready:
		case core::github_status::pull_request_failed:
		case core::github_status::pull_request_cancelled:
			source = HomePage;
			break;
		case core::github_status::no_profile:
		case core::github_status::establishing_session:
		case core::github_status::session_ready:
		case core::github_status::session_failed:
		case core::github_status::session_cancelled:
			break;
		}
		// [[BPR-1357]]. An opening the application follows is announced in the
		// opening's own words, once, by `refresh_home_open`.
		if (source == HomePage && HomeOpen.has_value()) {
			return;
		}
		// [[BPR-1575]]. A refresh is announced by its own lifecycle, which
		// knows who asked and what it brought; the status says nothing more.
		if (RefreshReadSeen) {
			RefreshReadSeen = false;
			return;
		}
		// [[BPR-1200]]. A refusal with its own card is announced in the card's
		// words, so a reader hears what a sighted reviewer reads.
		const auto wording      = source == HomePage ? home_refusal_wording() : std::nullopt;
		const auto announcement = wording.has_value()
			? QString::fromStdString(wording->Heading) + QLatin1Char{ ' ' } + QString::fromStdString(wording->Explanation)
			: SessionPanel->summary_text();
		if (source == nullptr || announcement.isEmpty() || announcement == LastGithubAnnouncement) {
			return;
		}
		LastGithubAnnouncement = announcement;
		QAccessibleAnnouncementEvent event{ source, announcement };
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
		// [[BPR-0771]]. A command can change without a status: a card must not
		// go on offering a Connect the core has just refused.
		if (!aTransition.Commands.empty()) {
			update_server_cards();
		}
		if (aTransition.Identity.has_value()) {
			SessionPanel->apply_identity(*aTransition.Identity);
			HasCachedOverview = false;
			PullRequestRefusal.reset();
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
		// [[BPR-1200]]. Kept until the next read, which replaces or clears it.
		if (aTransition.Refusal.has_value()) {
			PullRequestRefusal = aTransition.Refusal;
		}
		// [[BPR-1575]]. A refresh's own lifecycle, as the core named its read.
		if (aTransition.Read.has_value()) {
			observe_refresh_read(*aTransition.Read);
		}
		if (aTransition.Status.has_value()) {
			ReadProgress.reset();
			LoadingOverviewShown = false;
			if (aTransition.Status->Status == core::github_status::loading_pull_request
				|| aTransition.Status->Status == core::github_status::pull_request_ready) {
				PullRequestRefusal.reset();
			}
			SessionPanel->apply_status(aTransition.Status->Status);
			if (aTransition.Status->Status == core::github_status::loading_pull_request && !HasGithubPullRequest) {
				HasCachedOverview = false;
				HomeRequested     = false;
				// [[BPR-1599]]. A load starts with no choice of its own.
				LoadingChoice.reset();
				OverviewPage->present_loading();
				enter_opening_modality(review_modality::overview);
			}
			if (aTransition.Status->Status != core::github_status::loading_pull_request && !HasGithubPullRequest) {
				// [[BPR-1842]]. A file chosen from the provisional list goes on
				// waiting for the accepted review's rows; any other end drops it.
				PendingReviewPath = aTransition.Status->Status == core::github_status::pull_request_ready ? OverviewPage->pending_open() : QString{ };
				// [[BPR-1842]]. A read that ended without a review names nothing.
				if (LoadingIdentity.has_value()) {
					LoadingIdentity.reset();
					apply_strip_width();
				}
				// [[BPR-1895]]. The accepted overview replaces these rows next;
				// keep the check being read through that handoff.
				if (aTransition.Status->Status != core::github_status::pull_request_ready || !OverviewPage->has_observed_status()) {
					OverviewPage->clear();
				}
			}
			// [[BPR-1599]]. A load that ends without its pull request leaves
			// no choice for the next one.
			if (aTransition.Status->Status == core::github_status::pull_request_failed
				|| aTransition.Status->Status == core::github_status::pull_request_cancelled) {
				LoadingChoice.reset();
			}
			apply_modality_surface();
		}
		for (const auto& diagnostic : aTransition.Diagnostics) {
			SessionPanel->apply_diagnostic(diagnostic.Code);
			++Statistics.GithubDiagnosticUpdates;
		}
	}

	void workbench_window::apply_github_pull_request(
		const application::github_pull_request_projection& aPullRequest, const bool aPreserveWindowNavigation)
	{
		const auto authority = std::to_string(aPullRequest.Profile.value()) + ":" + std::to_string(aPullRequest.Host.value()) + ":"
			+ std::to_string(aPullRequest.Account.value()) + ":" + std::to_string(aPullRequest.Repository.value()) + ":" + std::to_string(aPullRequest.PullRequest.value());
		if (!MarkdownAuthority.empty() && MarkdownAuthority != authority) {
			clear_github_pull_request();
		}
		MarkdownAuthority   = authority;
		PageContext         = aPullRequest.MarkdownContext;
		const auto dispatch = [this, context = aPullRequest.MarkdownContext](const std::string& aBody, const std::string& aDestination) {
			const auto result = context && MarkdownDispatch ? MarkdownDispatch({ *context, aBody, aDestination }) : core::markdown_link_result::unavailable;
			QString    message;
			switch (result) {
			case core::markdown_link_result::opened:      message = QStringLiteral("Opened in your browser."); break;
			case core::markdown_link_result::stale:       message = QStringLiteral("This body is no longer current. Reopen the link from the current content."); break;
			case core::markdown_link_result::unavailable: message = QStringLiteral("Opening links is unavailable until the pull request is accepted."); break;
			case core::markdown_link_result::refused:     message = QStringLiteral("This link cannot be opened safely. Its destination remains available in the source."); break;
			case core::markdown_link_result::failed:      message = QStringLiteral("The browser could not open this link."); break;
			}
			statusBar()->showMessage(message, 8000);
		};
		OverviewPage->body_view()->set_link_dispatch([dispatch](const std::string& aDestination) { dispatch({ }, aDestination); });
		// RFC-BPR-0084. Images ask under this publication's context; a new
		// context asks again for whatever is still loading.
		const auto images = [this, context = aPullRequest.MarkdownContext](const std::string& aBody, std::vector<core::markdown_image_reference> aReferences) {
			if (context && MarkdownImageDispatch) {
				MarkdownImageDispatch({ .Context = *context, .Body = aBody, .References = std::move(aReferences) });
			}
		};
		OverviewPage->body_view()->set_image_dispatch([images](std::vector<core::markdown_image_reference> aReferences) { images({ }, std::move(aReferences)); });
		ConversationPanel->set_image_dispatch(images);
		// [[BPR-1802]]. Permalinks are judged by this pull request's repository.
		auto repository = std::optional<core::markdown_repository>{ };
		if (!aPullRequest.Hostname.empty() && !aPullRequest.Owner.empty() && !aPullRequest.RepositoryName.empty()) {
			repository = core::markdown_repository{ .Host = aPullRequest.Hostname, .Owner = aPullRequest.Owner, .Repository = aPullRequest.RepositoryName };
		}
		OverviewPage->body_view()->set_permalink_repository(repository);
		ConversationPanel->set_permalink_repository(repository);
		OverviewPage->set_check_link_dispatch(dispatch);
		ConversationPanel->set_link_dispatch(dispatch);
		PullRequestPanel->adopt(aPullRequest);
		ReviewThreadTotal = aPullRequest.ReviewThreadCount;
		if (!aPreserveWindowNavigation) {
			HomeRequested = false;
		}
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_pull_request_author(
				QString::fromStdString(aPullRequest.AuthorLogin));
		}
		// [[BPR-1157]]. Another pull request arriving takes the review surface
		// back from a local cycle; the same one refreshed behind it does not.
		const auto arriving = IdentityOwner != QString::fromStdString(aPullRequest.Owner)
			|| IdentityRepository != QString::fromStdString(aPullRequest.RepositoryName)
			|| IdentityNumber != QString::number(aPullRequest.Number.value());
		if (arriving && LocalCycleShown) {
			LocalCycleShown = false;
			LocalCycleTitle.clear();
		}
		// [[BPR-1575]]. What a refresh compares. Another pull request starts
		// its own freshness from this read; the same one handed over by a
		// sibling window is a refresh made elsewhere.
		{
			const auto observation = refresh_observation_of(aPullRequest);
			if (arriving) {
				PullRefresh = core::refresh_state{ .UpdatedAt = refresh_clock() };
				RefreshEnded.reset();
			} else if (aPreserveWindowNavigation && PullRefresh.Phase != core::refresh_phase::refreshing && RefreshSnapshot != std::optional{ aPullRequest.Snapshot }) {
				ElsewhereChanges = core::compare_refresh(RefreshLatest, observation);
			}
			RefreshLatest   = observation;
			RefreshSnapshot = aPullRequest.Snapshot;
		}
		IdentityOwner      = QString::fromStdString(aPullRequest.Owner);
		IdentityRepository = QString::fromStdString(aPullRequest.RepositoryName);
		IdentityNumber     = QString::number(aPullRequest.Number.value());
		// [[BPR-0990]]. The open pull request as the queue names it.
		OpenQueueRepository    = aPullRequest.Owner + "/" + aPullRequest.RepositoryName;
		OpenQueueNumber        = static_cast<std::int64_t>(aPullRequest.Number.value());
		OpenQueueTitle         = aPullRequest.Title;
		OpenQueuePublicPreview = aPullRequest.PublicPreview;
		IdentityText           = QStringLiteral("%1/%2  #%3 %4")
									 .arg(IdentityOwner,
										  IdentityRepository,
										  IdentityNumber,
										  QString::fromStdString(aPullRequest.Title));
		// [[BPR-1852]]. The chip follows the read; the composition puts the
		// viewer's own change over it (present_strip_draft).
		IdentityDraft = aPullRequest.Draft && aPullRequest.State == application::github_pull_request_state_projection::open;
		LoadingIdentity.reset();
		// [[BPR-0981]]. The repositories tree keeps the open pull request's
		// row current, however it was opened, so a stale row cannot hold the
		// current index and swallow the gesture that would open it.
		if (RepositoriesTree != nullptr) {
			RepositoriesTree->follow_open_pull_request(
				aPullRequest.Owner + "/" + aPullRequest.RepositoryName,
				static_cast<std::int64_t>(aPullRequest.Number.value()));
		}
		// [[BPR-1418]]. Content changes invalidate the row even when both
		// widgets were hidden during loading. Measure from the new identity
		// and context rather than using the prior visibility as admission.
		apply_strip_width();
		// The gap BPR-0191 recorded closes here. In the two-window arrangement
		// the anchor could only follow a conversation projection, because nothing
		// observed a comparison context change. The shell composes both surfaces,
		// so it sets the anchor from the file the reviewer is actually reading.
		ConversationPanel->set_anchor_file(conversation_anchor_file());
		// [[BPR-1337]]. Discuss's counts say an ordinary thread count; a line
		// that says threads are unavailable, or public preview's, stays shown.
		ConversationPanel->set_availability_ordinary(!aPullRequest.PublicPreview && aPullRequest.ReviewThreadsAvailable);
		ApplyingConversationAnchor = true;
		ConversationPanel->adopt(
			aPullRequest.Conversations,
			aPullRequest.PublicPreview
				? (aPullRequest.ReviewCommentsAvailable
					   ? QStringLiteral("Review comments are read-only in public preview. Review threads and their resolution are unavailable.")
					   : QStringLiteral("Review comments were not observable."))
				: detail::review_thread_availability_text(
					  aPullRequest.ReviewThreadsAvailable,
					  aPullRequest.ReviewThreadCount,
					  aPullRequest.Capabilities.ReviewThreads));
		ApplyingConversationAnchor = false;
		if (Finder != nullptr) {
			Finder->adopt(aPullRequest.Conversations);
		}
		synchronize_selected_conversation(chosen_conversation());
		refresh_cards();
		update_defect_surface();
		HasGithubPullRequest = true;
		HasCachedOverview    = false;
		update_pull_request_queue_actions();
		if (!PendingReviewPath.isEmpty()) {
			QTimer::singleShot(0, this, [this] { open_pending_review_path(); });
		}
		// [[BPR-0651]]. A pull request chosen from the repositories tree
		// opens on its overview, as the design says it does.
		if (OpenOnOverview && !aPreserveWindowNavigation) {
			OpenOnOverview = false;
			// [[BPR-0661]]. The modality is the reviewer's preference: the
			// overview the design opens on, Review, or the one last used here.
			// [[BPR-1599]]: or the one chosen while it loaded.
			enter_opening_modality(opening_modality());
		}
		LoadingChoice.reset();
		// [[BPR-0661]]. The drawer's first filter and the comparison on return
		// are the reviewer's preferences too; the comparison waits until the
		// history offers it.
		if (!aPreserveWindowNavigation) {
			FirstFilterApplied    = false;
			ReturnComparisonAsked = false;
			apply_first_comments_filter();
			ReturnComparisonPending = Behaviour.ReturnComparison
				!= application::review_return_comparison::whole_pr;
			honour_return_comparison();
		}
		SessionPanel->apply_capabilities(aPullRequest.Capabilities);
		SessionPanel->apply_rate_limits(aPullRequest.RateLimits);
		// [[BPR-0657]]. Resolve is offered once the provider and the server
		// both resolve threads; a notice from an earlier gesture is stale.
		Capabilities = aPullRequest.Capabilities;
		present_thread_resolution_offer();
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->present_notice(QString{ });
		}
		update_github_presentation_state();
		// RFC-BPR-0084. Unchanged bodies keep their images; ones still loading
		// ask again under the new context, which a cached image answers at once.
		OverviewPage->body_view()->request_images();
		ConversationPanel->request_images();
		// [[BPR-0668]]. The line states what is open, so it is written when
		// what is open changes, not only when a workbench projection lands.
		// Guarded: this runs while the window is still being assembled.
		if (StatusLabel != nullptr && SessionPanel != nullptr && EpochSelectors != nullptr) {
			update_status_text();
		}
	}

	void workbench_window::apply_github_content_command(
		const core::github_command_projection& aCommand)
	{
		// Conversation loading is automatic; no focus-only reveal command remains.
		static_cast<void>(aCommand);
	}

	void workbench_window::apply_github_host_diagnostic(
		const application::github_host_diagnostic_projection& aDiagnostic)
	{
		SessionPanel->apply_rate_limits(aDiagnostic.RateLimits);
		// [[BPR-0644]]. Before the diagnostic, whose presentation change is
		// what redraws the cards.
		if (aDiagnostic.GrantKept) {
			SessionPanel->apply_kept_grant(aDiagnostic.Reason, aDiagnostic.AccountMismatch);
		}
		SessionPanel->apply_diagnostic(
			aDiagnostic.Reason,
			aDiagnostic.RetryAfterSeconds,
			aDiagnostic.RateLimitCause,
			true,
			aDiagnostic.BootstrapStage, aDiagnostic.CapabilityDetail, aDiagnostic.AccountMismatch);
	}

	void workbench_window::set_markdown_link_dispatch(std::function<core::markdown_link_result(const core::markdown_link_intent&)> aDispatch)
	{
		MarkdownDispatch = std::move(aDispatch);
	}

	void workbench_window::set_markdown_image_dispatch(std::function<void(const core::markdown_image_intent&)> aDispatch)
	{
		MarkdownImageDispatch = std::move(aDispatch);
	}

	void workbench_window::set_identity_avatar_dispatch(std::function<void(std::vector<core::identity_avatar_intent>)> aDispatch)
	{
		AvatarPictures->set_dispatch(std::move(aDispatch));
	}

	void workbench_window::adopt_avatar_style(const core::identity_avatar_style aStyle)
	{
		AvatarPictures->set_style(aStyle);
	}

	auto workbench_window::identity_avatar_pictures() const noexcept -> qt::identity_avatar_pictures*
	{
		return AvatarPictures;
	}

	void workbench_window::set_markdown_code_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		MarkdownCodeDispatch = std::move(aDispatch);
		const auto relay     = [this](std::vector<application::markdown_code_syntax_request> aRequests) {
			if (MarkdownCodeDispatch) {
				MarkdownCodeDispatch(std::move(aRequests));
			}
		};
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->set_code_syntax_dispatch(relay);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_code_syntax_dispatch(relay);
		}
		// [[BPR-1753]], owner look v003: the agent's answers' code too.
		if (AgentTab != nullptr) {
			AgentTab->set_code_syntax_dispatch(relay);
		} // [[BPR-1805]] repair: a tour stop's code as a comment's.
		if (TourEditor != nullptr) {
			TourEditor->set_code_syntax_dispatch(relay);
		}
	}

	void workbench_window::set_markdown_permalink_dispatch(std::function<void(std::vector<application::markdown_blob_snippet_request>)> aDispatch)
	{
		MarkdownPermalinkDispatch = std::move(aDispatch);
		const auto relay          = [this](std::vector<application::markdown_blob_snippet_request> aRequests) {
			if (MarkdownPermalinkDispatch) {
				MarkdownPermalinkDispatch(std::move(aRequests));
			}
		};
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->set_permalink_dispatch(relay);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_permalink_dispatch(relay);
		}
	}

	void workbench_window::apply_markdown_permalink_snippet(const application::markdown_blob_snippet_publication& aSnippet)
	{
		// A reader takes an answer only for a permalink it holds.
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->apply_permalink_snippet(aSnippet);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->apply_markdown_permalink_snippet(aSnippet);
		}
	}

	void workbench_window::request_markdown_permalinks()
	{
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->request_permalink_snippets();
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->request_permalink_snippets();
		}
	}

	void workbench_window::apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		// A reader takes an answer only for code it shows.
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->apply_code_syntax(aSpans);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->apply_markdown_code_syntax(aSpans);
		}
		if (AgentTab != nullptr) {
			AgentTab->apply_code_syntax(aSpans);
		}
		if (TourEditor != nullptr) {
			TourEditor->apply_code_syntax(aSpans);
		}
	}

	void workbench_window::set_pull_request_page_dispatch(std::function<core::markdown_link_result(const application::pull_request_page_intent&)> aDispatch)
	{
		PageDispatch = std::move(aDispatch);
		update_open_on_github_action();
	}

	void workbench_window::set_reference_page_dispatch(std::function<core::markdown_link_result(core::reference_page)> aDispatch)
	{
		ReferenceDispatch = std::move(aDispatch);
		const auto bound  = static_cast<bool>(ReferenceDispatch);
		// The composer's menu and every reply's ask the window, which asks
		// the application; neither holds the dispatch itself.
		auto fromMenu = std::function<void(core::reference_page)>{ };
		if (bound) {
			fromMenu = [this](const core::reference_page aPage) { open_reference_page(aPage, false); };
		}
		if (ReviewEditor != nullptr && ReviewEditor->label_controls() != nullptr) {
			ReviewEditor->label_controls()->set_reference_dispatch(fromMenu);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_reference_dispatch(fromMenu);
		}
		if (ConventionalCommentsRow != nullptr) {
			ConventionalCommentsRow->setVisible(bound);
		}
	}

	void workbench_window::open_reference_page(const core::reference_page aPage, const bool aFromSettings)
	{
		const auto result = ReferenceDispatch ? ReferenceDispatch(aPage) : core::markdown_link_result::unavailable;
		// One page today. Its name is the site's, which is what the browser
		// will show; the address itself never reaches this window.
		auto page = QString{ };
		switch (aPage) {
		case core::reference_page::conventional_comments: page = QStringLiteral("conventionalcomments.org"); break;
		// Help › About opens these itself ([[BPR-1720]]); named here so
		// the switch stays whole.
		case core::reference_page::game_core_tech:    page = QStringLiteral("gamecoretech.com"); break;
		case core::reference_page::source_repository: page = QStringLiteral("github.com/MikeMarcin/BetterPullReview"); break;
		case core::reference_page::product_site:      page = QStringLiteral("betterpullreview.com"); break;
		}
		QString message;
		switch (result) {
		case core::markdown_link_result::opened:      message = QStringLiteral("Opened %1 in your browser.").arg(page); break;
		case core::markdown_link_result::stale:
		case core::markdown_link_result::unavailable: message = QStringLiteral("This window has no browser to open %1 with.").arg(page); break;
		case core::markdown_link_result::refused:     message = QStringLiteral("%1 cannot be opened safely from here.").arg(page); break;
		case core::markdown_link_result::failed:      message = QStringLiteral("The browser could not open %1.").arg(page); break;
		}
		statusBar()->showMessage(message, 8000);
		// Settings is a window of its own, where this window's status bar is
		// out of sight: a page that did not open says so under its link.
		if (aFromSettings && ConventionalCommentsStatus != nullptr) {
			const auto shown = result != core::markdown_link_result::opened;
			ConventionalCommentsStatus->setText(shown ? message : QString{ });
			ConventionalCommentsStatus->setAccessibleName(shown ? message : QString{ });
			ConventionalCommentsStatus->setVisible(shown);
		}
	}

	auto workbench_window::open_on_github_action() const noexcept -> QAction*
	{
		return OpenOnGithubAction;
	}

	auto workbench_window::open_on_github_button() const noexcept -> QToolButton*
	{
		return OpenOnGithubButton;
	}

	auto workbench_window::home_row_menu() const noexcept -> QMenu*
	{
		return HomeRowMenu;
	}

	auto workbench_window::pull_request_page_target() const -> std::optional<application::pull_request_page_intent>
	{
		// [[BPR-1338]]. On Home the row the reviewer last made current, in
		// whichever list; over a review the open pull request, by the
		// publication on screen.
		if (showing_home()) {
			// [[BPR-1365]]. A review cycle has no page; with its row selected
			// there is no pull request on Home to name.
			if (HomeSelection == home_selection_list::cycles) {
				return std::nullopt;
			}
			if (HomeSelection == home_selection_list::recent && HomeRecentList != nullptr && HomeRecentList->currentRow() >= 0
				&& static_cast<std::size_t>(HomeRecentList->currentRow()) < RecentReviews.Entries.size()) {
				return application::recent_review_row{ static_cast<std::size_t>(HomeRecentList->currentRow()) };
			}
			if (const auto item = HomeList != nullptr ? HomeList->current_queue_item() : std::nullopt; item.has_value()) {
				return item->Ref;
			}
			return std::nullopt;
		}
		if (PageContext.has_value() && HasGithubPullRequest && !LocalCycleShown) {
			return *PageContext;
		}
		return std::nullopt;
	}

	void workbench_window::update_open_on_github_action()
	{
		if (OpenOnGithubAction != nullptr) {
			OpenOnGithubAction->setEnabled(PageDispatch && pull_request_page_target().has_value());
		}
	}

	auto workbench_window::home_open_review_action() const noexcept -> QAction*
	{
		return HomeOpenReviewAction;
	}

	auto workbench_window::home_open_review_window_action() const noexcept -> QAction*
	{
		return HomeOpenReviewWindowAction;
	}

	void workbench_window::set_review_window_dispatch(std::function<bool(const application::review_window_intent&)> aDispatch)
	{
		ReviewWindowDispatch = std::move(aDispatch);
		update_home_review_actions();
	}

	auto workbench_window::home_review_target() const -> std::optional<std::variant<core::queue_item, application::recent_review_row>>
	{
		if (!showing_home() || HomeSelection == home_selection_list::cycles) {
			return std::nullopt;
		}
		if (HomeSelection == home_selection_list::recent && HomeRecentList != nullptr && HomeRecentList->currentRow() >= 0
			&& static_cast<std::size_t>(HomeRecentList->currentRow()) < RecentReviews.Entries.size()) {
			return application::recent_review_row{ static_cast<std::size_t>(HomeRecentList->currentRow()) };
		}
		if (auto item = HomeList != nullptr ? HomeList->current_queue_item() : std::nullopt; item.has_value()) {
			return std::move(*item);
		}
		return std::nullopt;
	}

	void workbench_window::update_home_review_actions()
	{
		if (HomeOpenReviewAction == nullptr || HomeOpenReviewWindowAction == nullptr) {
			return;
		}
		const auto target = home_review_target();
		const auto recent = target.has_value() && std::holds_alternative<application::recent_review_row>(*target);
		HomeOpenReviewAction->setEnabled(target.has_value() && (recent ? static_cast<bool>(RecentReviewDispatch) : static_cast<bool>(GithubDispatch)));
		// A new window is the profile's New Window, so it is offered exactly
		// when that is.
		HomeOpenReviewWindowAction->setEnabled(target.has_value() && ReviewWindowDispatch && NewBprWindowAction != nullptr
											   && NewBprWindowAction->isEnabled());
		if (HomeIgnoreAction != nullptr) {
			// [[BPR-1683]]. The action names what it will do to this row.
			const auto* const item = target.has_value() ? std::get_if<core::queue_item>(&*target) : nullptr;
			HomeIgnoreAction->setText(item != nullptr && item->Ignored ? home_unignore_text() : home_ignore_text());
			HomeIgnoreAction->setStatusTip(item != nullptr && item->Ignored
											   ? QStringLiteral("Puts this pull request back in its section; nothing changes on GitHub")
											   : QStringLiteral("Moves this pull request to Ignored at the bottom of Home; nothing changes on GitHub"));
			HomeIgnoreAction->setEnabled(item != nullptr && static_cast<bool>(HomeIgnoreDispatch));
		}
	}

	void workbench_window::dispatch_home_ignore(const core::queue_item& aItem)
	{
		if (!HomeIgnoreDispatch) {
			return;
		}
		HomeIgnoreDispatch(core::home_ignore_intent{
			.Ref    = aItem.Ref,
			.Action = aItem.Ignored ? core::home_ignore_action::unignore : core::home_ignore_action::ignore,
		});
	}

	void workbench_window::set_home_ignore_dispatch(workbench_home_ignore_dispatch aDispatch)
	{
		HomeIgnoreDispatch = std::move(aDispatch);
		update_home_review_actions();
	}

	auto workbench_window::home_ignore_action() const noexcept -> QAction*
	{
		return HomeIgnoreAction;
	}

	void workbench_window::popup_home_row_menu(const QPoint& aGlobal)
	{
		update_open_on_github_action();
		update_home_review_actions();
		// Resume is a Recently viewed row's only. It is hidden just while the
		// menu is open on a queue row, because a hidden action is a disabled
		// one, and the row's hover button follows this action.
		const auto target = home_review_target();
		const auto recent = target.has_value() && std::holds_alternative<application::recent_review_row>(*target);
		for (auto* const action : { HomeRecentResumeAction, HomeRecentRemoveAction }) {
			if (action != nullptr) {
				action->setVisible(recent);
			}
		}
		// [[BPR-1683]]. Ignore is a queue row's; a Recently viewed row is
		// not ignored from there.
		HomeIgnoreAction->setVisible(!recent);
		HomeRowMenu->popup(aGlobal);
	}

	void workbench_window::open_home_review(const bool aNewWindow)
	{
		const auto target = home_review_target();
		if (!target.has_value()) {
			return;
		}
		if (const auto* const recent = std::get_if<application::recent_review_row>(&*target)) {
			if (aNewWindow) {
				request_review_window(*recent);
			} else {
				dispatch_recent_review(static_cast<int>(recent->Index), false);
			}
			return;
		}
		const auto& item = std::get<core::queue_item>(*target);
		if (aNewWindow) {
			request_review_window(item.Ref);
		} else {
			open_pull_request_item(item);
		}
	}

	void workbench_window::request_review_window(const application::review_window_intent& aIntent)
	{
		if (!ReviewWindowDispatch || NewBprWindowAction == nullptr || !NewBprWindowAction->isEnabled()) {
			statusBar()->showMessage(QStringLiteral("A new window is unavailable until this profile is open."), 8000);
			return;
		}
		if (!ReviewWindowDispatch(aIntent)) {
			statusBar()->showMessage(QStringLiteral("This pull request cannot open in a new window from here. Choose it again."), 8000);
		}
	}

	void workbench_window::open_pull_request_page(const application::pull_request_page_intent& aIntent)
	{
		const auto result = PageDispatch ? PageDispatch(aIntent) : core::markdown_link_result::unavailable;
		QString    message;
		switch (result) {
		case core::markdown_link_result::opened:      message = QStringLiteral("Opened the pull request on GitHub in your browser."); break;
		case core::markdown_link_result::stale:       message = QStringLiteral("This pull request is no longer the one shown. Choose it again to open it on GitHub."); break;
		case core::markdown_link_result::unavailable: message = QStringLiteral("Opening on GitHub is unavailable until this window has a session for the pull request's server."); break;
		case core::markdown_link_result::refused:     message = QStringLiteral("This pull request's page cannot be opened safely from here."); break;
		case core::markdown_link_result::failed:      message = QStringLiteral("The browser could not open the pull request's page."); break;
		}
		statusBar()->showMessage(message, 8000);
	}

	void workbench_window::clear_github_pull_request()
	{
		MarkdownAuthority.clear();
		PageContext.reset();
		OverviewPage->body_view()->set_link_dispatch({ });
		ConversationPanel->set_link_dispatch({ });
		OverviewPage->body_view()->set_permalink_repository(std::nullopt);
		ConversationPanel->set_permalink_repository(std::nullopt);
		RefreshConversationAnchor.reset();
		RefreshConversationContext.reset();
		LastWorkbenchContext.reset();
		HasGithubPullRequest = false;
		HasCachedOverview    = false;
		LoadingOverviewShown = false;
		ReviewThreadTotal    = 0U;
		IdentityText.clear();
		IdentityOwner.clear();
		IdentityRepository.clear();
		IdentityNumber.clear();
		LoadingIdentity.reset();
		PendingReviewPath.clear();
		OpenQueueRepository.clear();
		OpenQueueNumber = 0;
		OpenQueueTitle.clear();
		OpenQueuePublicPreview = false;
		update_pull_request_queue_actions();
		if (RepositoriesTree != nullptr) {
			RepositoriesTree->forget_open_pull_request();
		}
		apply_strip_width();
		update_rail_summaries();
		PullRequestPanel->clear();
		clear_overview();
		ConversationPanel->clear(QStringLiteral(
			"Review threads are unavailable until a pull request is open."));
		if (Finder != nullptr) {
			Finder->clear();
		}
		PendingPick.reset();
		refresh_cards();
		update_github_presentation_state();
		if (StatusLabel != nullptr && SessionPanel != nullptr && EpochSelectors != nullptr) {
			update_status_text();
		}
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
		update_window_title();
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
			update_server_cards();
			update_home_open_notice();
		}
	}

	void workbench_window::set_profile_label(const QString& aLabel)
	{
		ProfileLabel = aLabel;
		if (ProfileReset) {
			ProfileReset->set_profile_label(aLabel);
		}
		SettingsWindow->set_profile_label(aLabel);
		if (MountedSettings) {
			MountedSettings->set_profile_label(aLabel);
		}
		update_window_title();
	}

	void workbench_window::update_window_title()
	{
		// [[BPR-1864]]. The Dock's window list, the Window menu, Mission
		// Control and the taskbars name the window by its title (macOS hides
		// the title bar). The core words it from what the window shows: the
		// pull request, accepted or named by GitHub's first answer, unless
		// Home or a local cycle is in front of it.
		const auto loading  = IdentityText.isEmpty() && LoadingIdentity.has_value();
		const auto named    = !IdentityText.isEmpty() || loading;
		const auto shown    = named && !showing_home() && !LocalCycleShown;
		const auto owner    = (loading ? LoadingIdentity->Owner : IdentityOwner).toStdString();
		const auto repo     = (loading ? LoadingIdentity->Repository : IdentityRepository).toStdString();
		const auto number   = (loading ? LoadingIdentity->Number : IdentityNumber).toULongLong();
		const auto pullName = loading ? LoadingIdentity->Title.toStdString() : OpenQueueTitle;
		const auto profile  = ProfileLabel.toStdString();
		const auto mode     = PublicPreviewSignedOut ? public_preview_mode_text().toStdString() : std::string{ };
		auto       input    = core::window_title_input{ .Profile = profile, .Profiles = ProfileCount, .Mode = mode };
		if (shown) {
			input.PullRequest = core::window_title_pull_request{ .Owner = owner, .Repository = repo, .Number = number, .Title = pullName };
		}
		// Qt reads "[*]" in a title as the modified mark; doubled, it is
		// shown literally.
		auto title = QString::fromStdString(core::window_title(input));
		title.replace(QStringLiteral("[*]"), QStringLiteral("[*][*]"));
		if (windowTitle() != title || accessibleName() != title) {
			setWindowTitle(title);
			setAccessibleName(title);
		}
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
		refuseWidget(QueueRoleFilter, QStringLiteral("Queue role"));
		refuseWidget(ReviewEditor, QStringLiteral("Publishing a review"));
	}

	void workbench_window::apply_public_preview_conversation()
	{
		// Body content belongs to this window's accepted read projection. The
		// profile-wide preview status cannot replace it with a sibling's posts.
		if (PublicPreviewSignedOut && ConversationPanel != nullptr && !HasGithubPullRequest) {
			ConversationPanel->clear(QStringLiteral("Review comments were not observable."));
		}
	}

	auto workbench_window::usage_indicator() const noexcept -> usage_ring*
	{
		auto* const card = current_server_card();
		return card != nullptr ? card->usage_indicator() : nullptr;
	}

	auto workbench_window::public_preview_signed_out() const noexcept -> bool
	{
		return PublicPreviewSignedOut;
	}

	auto workbench_window::public_preview_refusal_text() const -> QString
	{
		return SessionPanel != nullptr ? SessionPanel->public_preview_refusal_text()
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
		// [[BPR-0773]]. Sign-in is a state of its server's card: the card
		// holding the device sign-in shows it as its account row while a flow
		// runs (`update_server_cards`), so nothing here shows or hides it. A
		// flow begins from the accounts page itself -- the strip's account
		// button opens it -- so nothing here presents a window either.
		update_account_action();
		update_server_cards();
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
						"Paste a pull request link, or owner/name#123, in the bar "
						"on Home."));
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
							  "Check the link in the bar and open it again.")
						: QString::fromStdString(SessionPanel->diagnostic_text()));
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request loading canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The link in the bar is unchanged and can be opened again."));
				break;
			}
		}
		PullRequestStack->setProperty("githubPresentationState", state);

		update_home_open_notice();
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

	auto workbench_window::server_cards() const -> std::vector<server_card*>
	{
		return ServerCards;
	}

	auto workbench_window::server_card_for(const core::host_profile_id aProfile) const
		-> server_card*
	{
		const auto found = std::ranges::find_if(ServerCards, [aProfile](const server_card* const aCard) {
			return aCard->holds(aProfile);
		});
		return found == ServerCards.end() ? nullptr : *found;
	}

	auto workbench_window::current_server_card() const -> server_card*
	{
		if (SessionPanel == nullptr) {
			return nullptr;
		}
		const auto active  = SessionPanel->active_profile();
		const auto current = active.has_value() ? active : SessionPanel->selected_profile();
		return current.has_value() ? server_card_for(*current) : nullptr;
	}

	void workbench_window::set_public_preview_dispatch(
		application::public_preview_gesture_dispatch aDispatch)
	{
		PublicPreviewDispatch = std::move(aDispatch);
		for (auto* const card : ServerCards) {
			card->set_public_preview_dispatch(PublicPreviewDispatch);
		}
	}

	void workbench_window::select_github_profile(const core::host_profile_id aProfile)
	{
		if (SessionPanel == nullptr) {
			return;
		}
		SessionPanel->select_profile(aProfile);
		rebind_device_authorization_profile();
		update_server_cards();
	}

	void workbench_window::add_server_card(const application::github_profile_option& aOption)
	{
		if (ServerCardColumn == nullptr || server_card_for(aOption.Profile) != nullptr) {
			return;
		}
		// [[BPR-0772]]. One card per configured server: another way in to a
		// hostname already on the page joins that server's card.
		if (std::ranges::any_of(ServerCards, [&aOption](server_card* const aCard) {
				return aCard->adopt_option(aOption);
			})) {
			place_device_sign_in();
			return;
		}
		// The card asks; the window decides. Connect is the host chosen, then
		// the existing connect intent. Sign in is the device flow where the
		// host has one, and otherwise the way in Connect already opens.
		auto* const card = new server_card{
			aOption,
			server_card_gestures{
				.Connect =
					[this](const core::host_profile_id aProfile) {
						// [[BPR-1023]]. Every Connect on a card, and its GitHub CLI
						// offer, answers to the one admission the card shows it by:
						// the core's host-activation command. A device-capable
						// card's Connect begins device sign-in rather than
						// dispatching the activation, so it asks here first
						// instead of leaving the device command to decide. A
						// Connect that is not shown does nothing, as the
						// enterprise card's and the GitHub CLI offer's already did.
						if (SessionPanel == nullptr || !SessionPanel->connect_enabled()) {
							return;
						}
						const auto* chosen = server_card_for(aProfile);
						const auto  cli    = chosen->cli_option() && chosen->cli_option()->Profile == aProfile;
						if (cli) {
							present_cli_account_choices({ aProfile });
						}
						open_sign_in(chosen->hostname(), cli);
						select_github_profile(aProfile);
						if (chosen->offers_device_sign_in() && chosen->native_option() && chosen->native_option()->Profile == aProfile) {
							begin_selected_sign_in();
						} else {
							GithubConnectAction->trigger();
						}
					},
				.SignIn =
					[this](const core::host_profile_id aProfile) {
						open_sign_in(server_card_for(aProfile)->hostname());
						select_github_profile(aProfile);
						if (const auto* const chosen = server_card_for(aProfile);
							chosen != nullptr && chosen->offers_device_sign_in()) {
							begin_selected_sign_in();
						} else {
							GithubConnectAction->trigger();
						}
					},
				.SignOut =
					[this] {
						if (GithubDispatch) {
							GithubDispatch(core::deactivate_github_identity_intent{ });
						}
					},
				.Remember =
					[this, hostname = aOption.Hostname] {
						remember_enterprise_server(hostname);
					},
				.Forget =
					[this](const application::saved_server& aServer) {
						confirm_forget_server(aServer);
					},
				.CheckCliAccounts =
					[this] {
						if (GithubCliCheckDispatch && GithubCliDiscoveryState != github_cli_discovery_state::discovering) {
							GithubCliCheckDispatch();
						}
					},
			},
			ServerCardColumn->parentWidget(),
		};
		if (PublicPreviewDispatch) {
			card->set_public_preview_dispatch(PublicPreviewDispatch);
		}
		ServerCards.push_back(card);
		ServerCardColumn->addWidget(card);
		place_device_sign_in();
	}

	void workbench_window::update_server_cards()
	{
		if (SessionPanel == nullptr || ServerCards.empty()) {
			return;
		}
		const auto  status   = SessionPanel->status();
		const auto  active   = SessionPanel->active_profile();
		const auto  current  = active.has_value() ? active : SessionPanel->selected_profile();
		const auto& session  = SessionPanel->session();
		const auto  reading  = SessionPanel->public_preview_signed_out();
		const auto  admitted = SessionPanel->connect_enabled();
		// [[BPR-0773]]. What the device sign-in shows decides its card's
		// account row and, while a flow runs, its header's word.
		const auto deviceFlow    = DevicePanel != nullptr && DevicePanel->shows_flow();
		const auto deviceSigning = DevicePanel != nullptr && DevicePanel->signing_in();
		for (auto* const card : ServerCards) {
			auto shown               = server_card_presentation{ };
			shown.Current            = current.has_value() && card->holds(*current);
			shown.Chosen             = shown.Current ? current : std::nullopt;
			shown.ConnectAdmitted    = admitted;
			shown.ConnectDescription = admitted
				? QStringLiteral("Chooses %1 and connects it.").arg(QString::fromStdString(card->hostname()))
				: SessionPanel->connect_description();
			shown.PublicPreview      = SessionPanel->public_preview();
			shown.PublicPreviewNow   = SessionPanel->public_preview_now();
			if (shown.Current) {
				switch (status) {
				case core::github_status::no_profile:
				case core::github_status::session_cancelled:
					shown.State = server_card_state::signed_out;
					break;
				case core::github_status::establishing_session:
					shown.State = server_card_state::connecting;
					break;
				case core::github_status::session_failed:
					shown.State       = server_card_state::refused;
					shown.LastProblem = SessionPanel->showing_diagnostic()
						? QString::fromStdString(SessionPanel->diagnostic_text())
						: QString::fromStdString(SessionPanel->status_text());
					// [[BPR-0644]]. The host kept the remembered grant through
					// this failure: one that answered as another account says
					// so as its error, a refused one needs signing in again,
					// and anything else is tried again at the next launch.
					if (const auto kept = SessionPanel->kept_grant_reason(); kept.has_value()) {
						shown.KeptGrant = SessionPanel->kept_grant_names_another_account() ? kept_grant::another_account
							: *kept == core::github_reason_code::authentication_failed     ? kept_grant::needs_sign_in
																						   : kept_grant::retried_next_launch;
					}
					break;
				case core::github_status::session_ready:
				case core::github_status::loading_pull_request:
				case core::github_status::pull_request_ready:
				case core::github_status::pull_request_failed:
				case core::github_status::pull_request_cancelled:
					// Signed out and reading public pull requests is not an account.
					shown.State         = reading ? server_card_state::signed_out : server_card_state::connected;
					shown.ReadingPublic = reading;
					// The refusal that ended the last signed-out read is said beside the
					// reading, in the card's Details, rather than under the Home bar.
					if (reading) {
						shown.LastProblem = SessionPanel->public_preview_refusal_text();
					} else if (status == core::github_status::pull_request_failed && SessionPanel->showing_diagnostic()) {
						shown.LastProblem = QString::fromStdString(SessionPanel->diagnostic_text());
					}
					break;
				}
				if (!reading && session.has_value() && card->holds(session->Profile)) {
					shown.Session = session;
				}
				shown.Capabilities = SessionPanel->capabilities();
				shown.RateLimits   = SessionPanel->rate_limits();
			}
			const auto holdsSignIn = DevicePanel != nullptr && card->device_sign_in() == DevicePanel;
			// A running flow's row stays with its card; a finished one is said
			// only on the host it was asked for, so choosing another host does not
			// carry a denial or an expiry to a card that never had the code.
			const auto renewal = DevicePanel != nullptr
				&& DevicePanel->refresh_observation().State == application::device_authorization_refresh_state::reenrollment_required;
			const auto flowHere = deviceSigning || renewal || !DeviceFlowHost.has_value() || card->hostname() == *DeviceFlowHost;
			shown.DeviceSignIn  = holdsSignIn && deviceFlow && flowHere;
			// Sign in is shown only while its way in is admitted: the device
			// flow's begin for a host with one, otherwise Connect.
			shown.SignInAdmitted = admitted && (!card->offers_device_sign_in() || (DevicePanel != nullptr && DevicePanel->begin_button()->isEnabled()));
			const auto signedIn  = shown.State == server_card_state::connected && shown.Session.has_value();
			if (holdsSignIn && deviceSigning && !signedIn) {
				shown.State = server_card_state::signing_in;
			}
			// [[BPR-1373]]. Every gh account on this host from the last check.
			// A refused session on an account gh has signed out is said as
			// that, not as a failure to connect: gh fell back to its active
			// account and the viewer check refused it.
			if (const auto listed = CliAccountsByHost.find(card->hostname()); listed != CliAccountsByHost.end()) {
				shown.CliAccounts = listed->second;
				if (shown.State == server_card_state::refused && shown.Chosen.has_value()) {
					const auto chosen = std::ranges::find_if(shown.CliAccounts, [&](const server_card_cli_account& aAccount) {
						return aAccount.Profile == shown.Chosen && aAccount.State != server_card_cli_account_state::signed_in;
					});
					if (chosen != shown.CliAccounts.end()) {
						shown.State       = server_card_state::signed_out;
						shown.KeptGrant   = kept_grant::none;
						shown.LastProblem = QStringLiteral("%1 is signed out in gh, so this profile is signed out too. Sign it in with gh auth login, then check gh again.")
												.arg(QString::fromStdString(chosen->Login));
					}
				}
			}
			shown.CliCheckAdmitted = static_cast<bool>(GithubCliCheckDispatch) && GithubCliDiscoveryState != github_cli_discovery_state::discovering;
			// [[BPR-0774]]. An enterprise server's card remembers exactly the
			// record the queue keeps for its hostname.
			shown.RememberAdmitted = card->enterprise() && QueueReady && static_cast<bool>(SavedServerDispatch);
			if (card->enterprise()) {
				const auto kept = std::ranges::find(QueueProjection.SavedServers, card->hostname(), &application::saved_server::Hostname);
				if (kept != QueueProjection.SavedServers.end()) {
					shown.Remembered = *kept;
				}
			}
			card->apply(shown);
		}
		update_sign_in_surface();
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

	void workbench_window::begin_selected_sign_in()
	{
		const auto* card = current_server_card();
		open_sign_in(card ? card->hostname() : std::string{ });
		if (DevicePanel == nullptr) {
			return;
		}
		rebind_device_authorization_profile();
		// The same command-projected button owns admission and profile binding
		// for Home, the strip and Settings. A running flow cannot begin twice.
		DevicePanel->begin_button()->setFocus(Qt::OtherFocusReason);
		DevicePanel->begin_button()->click();
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
		place_device_sign_in();
	}

	void workbench_window::place_device_sign_in()
	{
		if (DevicePanel == nullptr || ServerCards.empty()) {
			return;
		}
		auto* target = server_card_for(DevicePanel->bound_profile());
		if (target == nullptr) {
			target = current_server_card();
		}
		if (target == nullptr) {
			target = ServerCards.front();
		}
		if (target->device_sign_in() == DevicePanel) {
			return;
		}
		// A live code stays on the card of the host it was asked for. The core
		// forbids a new flow while one runs, so the row follows a newly chosen
		// host once the running one has settled, and the observer places it
		// then.
		if (DevicePanel->signing_in()
			&& std::ranges::any_of(ServerCards, [this](const server_card* const aCard) {
				   return aCard->device_sign_in() == DevicePanel;
			   })) {
			return;
		}
		for (auto* const card : ServerCards) {
			if (card != target && card->device_sign_in() == DevicePanel) {
				card->release_device_sign_in();
			}
		}
		target->adopt_device_sign_in(DevicePanel);
		update_server_cards();
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

	auto workbench_window::queue_refresh_action() const noexcept -> QAction*
	{
		return QueueRefreshAction;
	}

	auto workbench_window::home_discovery_status() const noexcept -> QLabel*
	{
		return HomeDiscoveryStatus;
	}

	auto workbench_window::pull_request_menu() const noexcept -> QMenu*
	{
		return PullRequestMenu;
	}

	auto workbench_window::follow_pull_request_action() const noexcept -> QAction*
	{
		return FollowPullRequest;
	}

	auto workbench_window::save_pull_request_repository_action() const noexcept -> QAction*
	{
		return SavePullRequestRepository;
	}

	void workbench_window::set_queue_follow_dispatch(workbench_queue_follow_dispatch aDispatch)
	{
		QueueFollowDispatch = std::move(aDispatch);
		update_pull_request_queue_actions();
	}

	void workbench_window::set_queue_discovery_dispatch(workbench_queue_discovery_dispatch aDispatch)
	{
		QueueDiscoveryDispatch = std::move(aDispatch);
		update_discovery_controls();
	}

	void workbench_window::set_home_return_dispatch(std::function<void(const core::pull_request_ref&)> aDispatch)
	{
		HomeReturnDispatch = std::move(aDispatch);
	}

	void workbench_window::set_home_clock(std::function<std::optional<std::int64_t>()> aClock)
	{
		HomeClock = std::move(aClock);
		QueueLanding->set_now(HomeClock ? HomeClock() : std::nullopt);
	}

	auto workbench_window::open_queue_ref() const -> std::optional<core::pull_request_ref>
	{
		// Only a pull request read under this window's account session can be
		// kept for that account: the queue session names the server and login.
		if (!HasGithubPullRequest || OpenQueuePublicPreview || OpenQueueRepository.empty() || OpenQueueNumber <= 0 || !QueueReady
			|| QueueProjection.Session.Host.empty() || QueueProjection.Session.Account.empty()) {
			return std::nullopt;
		}
		return core::pull_request_ref{
			.Host       = QueueProjection.Session.Host,
			.Account    = QueueProjection.Session.Account,
			.Repository = OpenQueueRepository,
			.Number     = OpenQueueNumber,
		};
	}

	void workbench_window::update_discovery_controls()
	{
		const auto& discovery = QueueProjection.Discovery;
		const auto  session   = !discovery.Account.empty();
		if (QueueRefreshAction != nullptr) {
			const auto refreshing = discovery.State == application::queue_discovery_state::refreshing;
			QueueRefreshAction->setEnabled(static_cast<bool>(QueueDiscoveryDispatch) && HomeAccount && session && !refreshing);
			const auto tip = !QueueDiscoveryDispatch || !HomeAccount || !session
				? QStringLiteral("Sign in to find my pull requests in saved repositories.")
				: refreshing ? QStringLiteral("Finding my pull requests in saved repositories…")
							 : QStringLiteral("Find my pull requests in saved repositories again; this never changes GitHub.");
			QueueRefreshAction->setToolTip(tip);
			QueueRefreshAction->setStatusTip(tip);
			if (auto* const glyph = PrimaryToolbar != nullptr ? PrimaryToolbar->widgetForAction(QueueRefreshAction) : nullptr; glyph != nullptr) {
				glyph->setAccessibleDescription(tip);
			}
		}
		update_home_repository_onboarding();
		if (HomeRepositoryRow != nullptr) {
			if (auto* row = qobject_cast<QBoxLayout*>(HomeRepositoryRow->layout())) {
				row->setDirection(width() < 640 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
			}
		}
		if (HomeDiscoveryStatus != nullptr) {
			const auto hasSaved = std::ranges::any_of(QueueProjection.SavedRepositories, [this](const auto& aSaved) {
				return aSaved.Host == QueueProjection.Session.Host && aSaved.Account == QueueProjection.Session.Account;
			});
			const auto sentence = !HomeAccount ? QString{ } : !hasSaved ? tr("Add a repository to find my pull requests and review requests.")
				: session                                               ? home_discovery_sentence(discovery)
																		: tr("Personal discovery is unavailable. Saved repositories are kept.");
			if (HomeDiscoveryStatus->text() != sentence) {
				HomeDiscoveryStatus->setText(sentence);
			}
			HomeDiscoveryStatus->setVisible(!sentence.isEmpty());
		}
	}

	void workbench_window::update_home_repository_onboarding()
	{
		if (HomeOnboarding == nullptr || HomeRepositoryRow == nullptr) {
			return;
		}
		// Only a ready queue knows that nothing is saved: while it restores, the
		// ordinary row stands, so a returning reviewer never sees onboarding flash.
		const auto session    = QueueReady && !QueueProjection.Session.Host.empty() && !QueueProjection.Session.Account.empty();
		const auto hasSaved   = std::ranges::any_of(QueueProjection.SavedRepositories, [this](const auto& aSaved) {
			return aSaved.Host == QueueProjection.Session.Host && aSaved.Account == QueueProjection.Session.Account;
		});
		const auto onboarding = HomeAccount && session && !hasSaved;
		const auto handFocus  = !onboarding && HomeOnboardingButton->hasFocus();
		HomeOnboarding->setVisible(onboarding);
		HomeRepositoryRow->setVisible(HomeAccount && !onboarding);
		// With nothing saved, an empty queue's Needs me lane could only ever say
		// that nothing needs me; rows enrolled by an observer rule or a follow
		// still show, as does Recently viewed and Review Cycles.
		if (HomeList != nullptr && HomeList->onboarding() != onboarding) {
			HomeList->set_onboarding(onboarding);
			HomeList->refresh();
		}
		HomeOnboardingButton->setEnabled(AddSavedRepository != nullptr && AddSavedRepository->isEnabled());
		// The first save retires the call to action under the keyboard; its
		// ordinary Add repository takes the focus rather than nothing.
		if (handFocus && HomeAddRepositoryButton->isVisibleTo(this)) {
			HomeAddRepositoryButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::update_pull_request_queue_actions()
	{
		// [[BPR-1365]]. The queue row of the open pull request says so, by the
		// same exact identity Follow names.
		if (QueueLanding != nullptr) {
			QueueLanding->set_reviewing(open_queue_ref());
		}
		if (FollowPullRequest == nullptr || SavePullRequestRepository == nullptr) {
			return;
		}
		const auto ref      = open_queue_ref();
		const auto item     = ref ? QueueLanding->item_for(*ref) : std::nullopt;
		const auto followed = item && item->Membership.Followed;
		FollowPullRequest->setEnabled(ref.has_value() && static_cast<bool>(QueueFollowDispatch));
		FollowPullRequest->setChecked(followed);
		const auto followTip = ref.has_value()
			? (followed ? QStringLiteral("Stop following this pull request in my queue; nothing changes on GitHub.")
						: QStringLiteral("Keep this pull request in my queue; nothing changes on GitHub."))
			: QStringLiteral("Open a pull request signed in to follow it.");
		FollowPullRequest->setToolTip(followTip);
		FollowPullRequest->setStatusTip(followTip);
		const auto saved = ref.has_value() && std::ranges::any_of(QueueProjection.SavedRepositories, [&ref](const application::saved_repository& aSaved) {
							   return aSaved.Host == ref->Host && aSaved.Account == ref->Account && aSaved.Repository == ref->Repository;
						   });
		SavePullRequestRepository->setEnabled(ref.has_value() && !saved && !RepositorySaving && static_cast<bool>(RepositoryAdditionDispatch));
		const auto saveTip = !ref.has_value() ? QStringLiteral("Open a pull request signed in to save its repository.")
			: saved                           ? QStringLiteral("This pull request's repository is saved.")
											  : QStringLiteral("Keep this repository so my pull requests in it are found; nothing changes on GitHub.");
		SavePullRequestRepository->setToolTip(saveTip);
		SavePullRequestRepository->setStatusTip(saveTip);
	}

	auto workbench_window::show_review_threads_action() const noexcept -> QAction*
	{
		return nullptr;
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

	auto workbench_window::private_notes() const noexcept -> private_notes_panel* { return PrivateNotes; }

	void workbench_window::withdraw_agent_surfaces()
	{
		for (auto* const widget : std::initializer_list<QWidget*>{ AgentPanel, SideChat, FindingQueue, ClaudeWrapper }) {
			if (widget != nullptr) {
				widget->hide();
			}
		}
		// A button whose default action is invisible hides itself, so the
		// drawer's chat and finding buttons and the workshop's controls follow
		// their actions wherever they are placed.
		auto actions = std::vector<QAction*>{ };
		if (SideChat != nullptr) {
			actions.insert(actions.end(), { SideChat->send_action(), SideChat->add_context_action(), SideChat->new_session_action(), SideChat->continue_old_revision_action() });
		}
		if (FindingQueue != nullptr) {
			for (auto index = std::size_t{ 0U }; index < finding_queue_action_count; ++index) {
				actions.push_back(FindingQueue->action(static_cast<finding_queue_action>(index)));
			}
		}
		if (ReviewEditor != nullptr) {
			actions.insert(actions.end(), { ReviewEditor->accept_revision_action(), ReviewEditor->reject_revision_action() });
			for (auto index = std::size_t{ 0U }; index < core::workshop_operation_count; ++index) {
				actions.push_back(ReviewEditor->workshop_action(static_cast<core::workshop_operation>(index)));
			}
		}
		for (auto* const action : actions) {
			if (action != nullptr) {
				action->setVisible(false);
			}
		}
		for (const auto* name : { "reviewWorkshopOriginal", "reviewWorkshopProposal", "reviewWorkshopOperation", "reviewWorkshopCandidate",
								  "tourProblemMagic", "tourSolutionMagic", "tourStopMagic", "tourSuggestOutline", "tourSuggestedOutline", "tourInsertOutline",
								  "privateAgentProfile", "privateRequestedModel", "privateRequestedEffort", "privateAskAgent", "privateCompositionAsk" }) {
			for (auto* const widget : findChildren<QWidget*>(QString::fromLatin1(name))) {
				widget->hide();
			}
		}
		// The composition editor's profile is a labelled form row; the row goes
		// with it, and the agent-assisted mode can no longer be chosen.
		for (auto* const profile : findChildren<QWidget*>(QStringLiteral("privateCompositionProfile"))) {
			for (auto* const form : profile->parentWidget()->findChildren<QFormLayout*>()) {
				if (form->indexOf(profile) >= 0) {
					form->setRowVisible(profile, false);
				}
			}
		}
		for (auto* const mode : findChildren<QComboBox*>(QStringLiteral("privateCompositionMode"))) {
			if (auto* const items = qobject_cast<QStandardItemModel*>(mode->model()); items != nullptr && items->rowCount() > 2) {
				items->item(2)->setEnabled(false);
			}
		}
		if (auto* const tabs = findChild<QTabWidget*>(QStringLiteral("privateConversationTabs")); tabs != nullptr && ConversationScroll != nullptr) {
			tabs->setTabText(tabs->indexOf(ConversationScroll), tr("Private notes"));
			tabs->setAccessibleName(tr("Private history and notes"));
		}
		if (auto* const purpose = findChild<QLabel*>(QStringLiteral("workbenchNotesPurpose")); purpose != nullptr) {
			purpose->setText(tr("Private to you and never published. Write a note about this pull request, then promote what you want to say to a draft in Comments."));
			purpose->setAccessibleDescription(purpose->text());
		}
		if (SettingsWindow != nullptr) {
			SettingsWindow->withdraw_page(settings_page::agents_and_context);
		}
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
}
