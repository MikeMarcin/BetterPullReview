#include "review_cycle_desktop.hpp"
#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/overview_status_sections.hpp"
	#include "bpr/application/pull_request_overview.hpp"
	#include "bpr/application/review_mutation_application_host.hpp"
	#include "bpr/application/review_mutation_port.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/core/conversation_reactions.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/draft_state.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/reviewer_request.hpp"
	#include "bpr/qt/conversation_cards_view.hpp"
	#include "bpr/qt/conversation_finder.hpp"
	#include "bpr/qt/destructive_question.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/github_conversation_model.hpp"
	#include "bpr/qt/github_conversation_panel.hpp"
	#include "bpr/qt/markdown_body_view.hpp"
	#include "bpr/qt/pull_request_overview_page.hpp"
	#include "bpr/qt/review_draft_editor.hpp"
	#include "bpr/qt/reviewer_picker.hpp"
	#include "bpr/qt/scoped_prose_editor.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_strip.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/review_mutation_selection.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractButton>
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAction>
	#include <QAnyStringView>
	#include <QApplication>
	#include <QByteArray>
	#include <QChar>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QEvent>
	#include <QFile>
	#include <QFlags>
	#include <QIODevice>
	#include <QIODeviceBase>
	#include <QJsonDocument>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QJsonValueConstRef>
	#include <QKeyEvent>
	#include <QLabel>
	#include <QLatin1Char>
	#include <QLineEdit>
	#include <QList>
	#include <QListSpecialMethods>
	#include <QListView>
	#include <QListWidget>
	#include <QListWidgetItem>
	#include <QModelIndex>
	#include <QObject>
	#include <QPlainTextEdit>
	#include <QPushButton>
	#include <QScopeGuard>
	#include <QScrollArea>
	#include <QString>
	#include <QTextCursor>
	#include <QTextStream>
	#include <QToolButton>
	#include <QVariant>
	#include <QWidget>
	#include <QWidgetList>
	#include <algorithm>
	#include <chrono>
	#include <cstddef>
	#include <filesystem>
	#include <initializer_list>
	#include <string_view>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		/// [[BPR-1741]]. One post of the open thread, by identity.
		[[nodiscard]] auto post_view_of(const qt::conversation_cards_view& aCards, const std::string& aPost) -> QWidget*
		{
			if (aCards.open_card() == nullptr) {
				return nullptr;
			}
			for (auto* const post : aCards.open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
				if (post->property("postIdentity").toString().toStdString() == aPost) {
					return post;
				}
			}
			return nullptr;
		}

		/// [[BPR-1741]]. One reaction's chip under a post of the open thread.
		[[nodiscard]] auto reaction_chip_of(const qt::conversation_cards_view& aCards, const std::string& aPost, const core::reaction_content aContent) -> QAbstractButton*
		{
			auto* const post = post_view_of(aCards, aPost);
			if (post == nullptr) {
				return nullptr;
			}
			for (auto* const chip : post->findChildren<QAbstractButton*>(QStringLiteral("reactionChip"))) {
				if (chip->isVisible() && chip->property("reactionContent").toInt() == static_cast<int>(aContent)) {
					return chip;
				}
			}
			return nullptr;
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::exercise_threaded_reply(
		github_application& aApplication) -> detail::threaded_reply_probe_result
	{

		using result               = detail::threaded_reply_probe_result;
		using state_type           = threaded_reply_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ThreadedReplyProbe.has_value()) {
			implementation->ThreadedReplyProbe.emplace();
		}
		auto& probe = *implementation->ThreadedReplyProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr
			? window->conversation_panel()->cards()
			: nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr) {
			return result::pending;
		}
		// [[BPR-1804]]. Where each course ends: with reviewers served, it goes
		// on to request one from People.
		// [[BPR-1852]]. With the draft state served, every course ends there.
		const auto goOnToDraftState = [&probe, implementation] {
			if (implementation->ReviewMutationHost->capabilities().ChangesDraftState) {
				probe.Phase = state_type::phase::awaiting_draft_offered;
				return result::pending;
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		};
		const auto goOnToReviewers = [&probe, implementation, &goOnToDraftState] {
			if (implementation->ReviewMutationHost->capabilities().RequestsReviewers) {
				probe.Phase = state_type::phase::awaiting_reviewer_offered;
				return result::pending;
			}
			return goOnToDraftState();
		};
		const auto fail = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		// One line per phase, the latest facts of each: a stalled course says
		// where it stalled and what it last saw before.
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		// [[BPR-1841]]. The Overview's unresolved threads, over its files.
		const auto overviewUnresolved = [implementation]() {
			auto total = std::size_t{ 0U };
			if (implementation->PublishedOverview) {
				for (const auto& file : implementation->PublishedOverview->Files) {
					total += file.UnresolvedThreadCount;
				}
			}
			return total;
		};
		// [[BPR-1841]]. What resolving has cost since the Resolve press; a
		// refresh still running when GitHub's answer arrived counts against it.
		const auto measureResolveCost = [&probe, implementation, window, model, cards]() {
			if (!probe.ResolveBaseline.has_value()) {
				return;
			}
			const auto& base                = *probe.ResolveBaseline;
			auto&       evidence            = probe.Evidence;
			auto* const refresh             = window->github_refresh_action();
			evidence.ResolveCostMeasured    = true;
			evidence.ResolveReads           = implementation->Fixture->evidence().AccountReadCount - base.Reads;
			evidence.ResolveProviderCalls   = implementation->Fixture->review_mutation_evidence().Resolutions - base.Resolutions;
			evidence.ResolveModelResets     = model->statistics().ModelResets - base.ModelResets;
			evidence.ResolveRowsChanged     = model->statistics().RowsChanged - base.RowsChanged;
			evidence.ResolveFinderRebuilds  = static_cast<std::size_t>((window->finder() != nullptr ? window->finder()->rebuilds() : 0U) - base.FinderRebuilds);
			evidence.ResolveCardLayouts     = static_cast<std::size_t>(cards->layouts() - base.CardLayouts);
			evidence.ResolveRefreshInFlight = evidence.ResolveRefreshInFlight || (refresh != nullptr && !refresh->isEnabled());
			evidence.ResolveOverviewBuilds  = static_cast<std::size_t>(implementation->OverviewBuilds - base.OverviewBuilds);
			evidence.ResolveOverviewUpdates = implementation->OverviewResolutionUpdates - base.OverviewMoves;
		};
		// [[BPR-1849]]. Whether People's Waiting on lists a login, and if so
		// whether as pending; and the time since the reviewer choice.
		const auto reviewerRow = [window](const std::string_view aLogin) -> std::optional<bool> {
			for (const auto* const row : window->overview_page()->request_rows()) {
				if (row->request().Name == aLogin) {
					return row->request().Pending;
				}
			}
			return std::nullopt;
		};
		const auto microsSinceChoice = [&probe]() -> std::int64_t {
			return probe.ReviewerBaseline
				? std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - probe.ReviewerBaseline->Chosen).count()
				: -1;
		};
		const auto measureReviewerCost = [&probe, implementation, window]() {
			if (!probe.ReviewerBaseline) {
				return;
			}
			auto* const refresh                    = window->github_refresh_action();
			probe.Evidence.ReviewerCostMeasured    = true;
			probe.Evidence.ReviewerReads           = implementation->Fixture->evidence().AccountReadCount - probe.ReviewerBaseline->Reads;
			probe.Evidence.ReviewerOverviewBuilds  = static_cast<std::size_t>(implementation->OverviewBuilds - probe.ReviewerBaseline->OverviewBuilds);
			probe.Evidence.ReviewerOverviewUpdates = implementation->OverviewReviewRequestUpdates - probe.ReviewerBaseline->OverviewMoves;
			probe.Evidence.ReviewerRefreshInFlight = probe.Evidence.ReviewerRefreshInFlight || (refresh != nullptr && !refresh->isEnabled());
		};
		constexpr auto threadIdentity = std::string_view{ "review-thread:PRRT_fixture_1" };
		constexpr auto replyBody      = std::string_view{ "Agreed, will do." };
		constexpr auto discardedBody  = std::string_view{ "Local text that is discarded." };
		// The first current-head line of review.cpp that no thread is on.
		const auto firstThreadlessRightLine = [window, model]() -> std::optional<std::uint32_t> {
			for (auto line = std::uint32_t{ 0U }; line < 64U; ++line) {
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, line)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					return line;
				}
			}
			return std::nullopt;
		};
		auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
		auto* const loaded     = window->conversation_panel()->findChild<QWidget*>(QStringLiteral("githubConversationLoaded"));
		if (inlinePage == nullptr || loaded == nullptr) {
			return result::pending;
		}
		// The drawer's cards are on its loaded page, with the thread's card.
		const auto drawerShowsThread = [window, cards, loaded, threadIdentity] {
			return loaded->isVisibleTo(window->conversation_panel()) && cards->isVisibleTo(window->conversation_panel())
				&& std::ranges::find(cards->cards(), std::string{ threadIdentity }, &qt::thread_card::Id) != cards->cards().end();
		};
		// A press on one of the thread's actions, as a reviewer's: the thread
		// opens in place ([[BPR-1290]]) and its button is clicked.
		const auto press = [cards, threadIdentity](const qt::card_action aAction) {
			cards->open_thread(std::string{ threadIdentity });
			auto* const card = cards->open_card();
			if (card == nullptr) {
				return false;
			}
			const auto  name   = aAction == qt::card_action::reply ? QStringLiteral("threadReplyField") : QStringLiteral("threadResolve");
			auto* const button = card->findChild<QAbstractButton*>(name);
			if (button == nullptr || !button->isEnabled()) {
				return false;
			}
			button->click();
			return true;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_ready: {
			note("outstanding=" + std::to_string(implementation->WorkbenchHost->outstanding_requests())
				 + " diff=" + std::to_string(implementation->LastSelectedDiff.has_value() ? 1 : 0)
				 + " thread=" + std::to_string(model->index_for_id(threadIdentity).isValid() ? 1 : 0));
			if (implementation->WorkbenchHost->outstanding_requests() != 0U) {
				return result::pending;
			}
			auto* const view = window->files_drawer()->contents_view();
			if (view == nullptr || view->model() == nullptr || view->model()->rowCount() == 0) {
				return result::pending;
			}
			if (!view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
				return result::pending;
			}
			if (!implementation->LastSelectedDiff.has_value()
				|| !model->index_for_id(threadIdentity).isValid()) {
				return result::pending;
			}
			// [[BPR-1214]]. First a comment on a line no thread is on, which
			// is discarded, then Back: the drawer keeps the file's threads.
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_line_comparison;
			return result::pending;
		}
		case state_type::phase::awaiting_line_comparison: {
			note("comparison=" + std::to_string(window->comparison_surface()->current_result() != nullptr ? 1 : 0));
			if (window->comparison_surface()->current_result() == nullptr) {
				return result::pending;
			}
			probe.ThreadlessLine = firstThreadlessRightLine();
			if (!probe.ThreadlessLine) {
				return fail("no-threadless-line rows=" + std::to_string(model->rowCount()));
			}
			probe.Phase = state_type::phase::awaiting_line_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_line_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + std::to_string(draft.has_value() ? 1 : 0)
				 + " anchorLine=" + std::to_string(draft ? draft->Anchor.Line : 0U)
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0)
				 + " rows=" + std::to_string(model->rowCount()));
			if (!draft || draft->Anchor.Side != core::diff_side::right || draft->Anchor.Line != probe.ThreadlessLine
				|| draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			if (model->rowCount() != 0 || !drawerShowsThread()) {
				return fail("threadless-line-hid-drawer rows=" + std::to_string(model->rowCount()));
			}
			// [[BPR-1212]]. Comment opens the draft in the drawer at its line.
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (!cards->writing_draft() || cards->draft_editor() == nullptr || cards->draft_editor()->isReadOnly() || inlinePage->isVisibleTo(window)) {
				return fail("comment-drawer-draft-not-editable");
			}
			cards->draft_editor()->setPlainText(QString::fromUtf8(discardedBody.data(), static_cast<qsizetype>(discardedBody.size())));
			probe.Phase = state_type::phase::awaiting_line_body;
			return result::pending;
		}
		case state_type::phase::awaiting_line_body: {
			note("body=" + (implementation->ReviewDraft ? implementation->ReviewDraft->Body : std::string{ "-" }));
			if (!implementation->ReviewDraft || implementation->ReviewDraft->Body != discardedBody) {
				return result::pending;
			}
			// The draft's own Discard, in the drawer.
			auto* const discard = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadDeleteDraft")) : nullptr;
			if (discard == nullptr) {
				return fail("no-drawer-discard");
			}
			discard->click();
			QCoreApplication::processEvents();
			// [[BPR-1345]] v002: Delete asks first when the draft has words.
			auto* const question = window->delete_draft_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteDraftConfirmationConfirm")) : nullptr;
			if (yes == nullptr) {
				return fail("drawer-delete-asked-nothing");
			}
			yes->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_line_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_line_discarded: {
			const auto& draft = implementation->ReviewDraft;
			note("state=" + (draft ? std::string{ core::name(draft->State) } : std::string{ "-" }));
			if (draft && draft->State != core::review_draft_state::discarded && draft->Body == discardedBody) {
				return result::pending;
			}
			QCoreApplication::processEvents();
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			if (inlinePage->isVisibleTo(window) || cards->writing_draft() || !drawerShowsThread() || mutations.Submissions != 0U || mutations.Replies != 0U) {
				return fail("comments-drawer-blank-after-discard-and-back page=" + std::to_string(inlinePage->isVisibleTo(window) ? 1 : 0)
							+ " submissions=" + std::to_string(mutations.Submissions) + " replies=" + std::to_string(mutations.Replies));
			}
			window->choose_modality(qt::review_modality::discuss);
			QCoreApplication::processEvents();
			if (!press(qt::card_action::reply)) {
				return fail("no-reply-action");
			}
			probe.Phase = state_type::phase::awaiting_reply_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_draft: {
			note("draft=" + std::to_string(implementation->ReviewDraft.has_value() ? 1 : 0) + " reply="
				 + (implementation->ReviewDraft && implementation->ReviewDraft->ReplyToThread ? *implementation->ReviewDraft->ReplyToThread : std::string{ "-" })
				 + " anchorSide=" + std::to_string(implementation->ReviewDraft ? static_cast<int>(implementation->ReviewDraft->Anchor.Side) : -1)
				 + " anchorLine=" + std::to_string(implementation->ReviewDraft ? implementation->ReviewDraft->Anchor.Line : 0U)
				 + " surfaceSide=" + std::to_string(window->comparison_surface()->selection() ? static_cast<int>(window->comparison_surface()->selection()->PreferredLine.Side) : -1)
				 + " surfaceLine=" + std::to_string(window->comparison_surface()->selection() ? window->comparison_surface()->selection()->PreferredLine.Line : 0U)
				 + " availability=" + std::to_string(model->index_for_id(threadIdentity).data(qt::github_conversation_model::anchor_availability_role).toInt())
				 + " discuss=" + std::to_string(window->conversation_panel()->discuss() ? 1 : 0));
			if (!implementation->ReviewDraft.has_value()
				|| implementation->ReviewDraft->ReplyToThread
					!= std::optional<std::string>{ "PRRT_fixture_1" }) {
				return result::pending;
			}
			probe.Evidence.ReplyDrafted = !editor->reply_target_label()->isHidden();
			editor->body_editor()->setPlainText(QString::fromUtf8(replyBody.data(), static_cast<qsizetype>(replyBody.size())));
			probe.Phase = state_type::phase::awaiting_reply_body;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_body: {
			note("body=" + (implementation->ReviewDraft ? implementation->ReviewDraft->Body : std::string{ "-" })
				 + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0));
			if (!implementation->ReviewDraft.has_value() || implementation->ReviewDraft->Body != replyBody
				|| !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_reply_published;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_published: {
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0)
				 + " replies=" + std::to_string(implementation->Fixture->review_mutation_evidence().Replies)
				 + " submissions=" + std::to_string(implementation->Fixture->review_mutation_evidence().Submissions)
				 + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value()
				|| implementation->Fixture->review_mutation_evidence().Replies == 0U) {
				return result::pending;
			}
			probe.Evidence.ReplyPublished = implementation->Fixture->review_mutation_evidence().Replies == 1U;
			auto* const refresh           = window->github_refresh_action();
			if (refresh == nullptr) {
				return fail("refresh-unavailable");
			}
			// A refresh the application started after publishing holds the
			// action until it answers; the run budget bounds the wait.
			if (!refresh->isEnabled()) {
				note("refresh=busy");
				return result::pending;
			}
			refresh->trigger();
			probe.Phase = state_type::phase::awaiting_reply_seen;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_seen: {
			const auto reply = model->index_for_id("review-comment:fixture-comment-1");
			note("replyRow=" + std::to_string(reply.isValid() ? 1 : 0) + " rows=" + std::to_string(model->rowCount())
				 + " threadChildren=" + std::to_string(model->rowCount(model->index_for_id(threadIdentity)))
				 + " reads=" + std::to_string(implementation->Fixture->evidence().AccountReadCount)
				 + " lastRead=" + std::to_string(implementation->Fixture->evidence().LastReadOperation));
			if (!reply.isValid()) {
				return result::pending;
			}
			const auto parent = reply.data(qt::github_conversation_model::parent_identity_role).toString();
			probe.Evidence.ReplyUnderThread
				= (parent == QString::fromUtf8(threadIdentity.data(), static_cast<qsizetype>(threadIdentity.size()))
				   || parent == QStringLiteral("review-comment:PRRC_fixture_1"))
				&& reply.data(qt::github_conversation_model::body_role).toString()
					== QString::fromUtf8(replyBody.data(), static_cast<qsizetype>(replyBody.size()));
			if (!probe.Evidence.ReplyUnderThread) {
				return fail("reply-not-under-thread parent=" + parent.toStdString());
			}
			// [[BPR-1824]]. Resolve in Discuss with no file open: the diff
			// selection goes first, and GitHub is still asked. Before, the
			// grant waited on a selected diff and the thread "could not be
			// resolved: unavailable".
			implementation->clear_diff_selection();
			if (implementation->SelectedDiffPublication.has_value()) {
				return fail("diff-selection-kept");
			}
			window->choose_modality(qt::review_modality::discuss);
			QCoreApplication::processEvents();
			// The thread opens before the counters are taken: opening marks it
			// read, and the receipt that writes may project the Overview again.
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			probe.ResolveBaseline = state_type::resolve_baseline{
				.Reads          = implementation->Fixture->evidence().AccountReadCount,
				.Resolutions    = implementation->Fixture->review_mutation_evidence().Resolutions,
				.ModelResets    = model->statistics().ModelResets,
				.RowsChanged    = model->statistics().RowsChanged,
				.FinderRebuilds = window->finder() != nullptr ? window->finder()->rebuilds() : 0U,
				.CardLayouts    = cards->layouts(),
				.OverviewBuilds = implementation->OverviewBuilds,
				.OverviewMoves  = implementation->OverviewResolutionUpdates,
				.Unresolved     = overviewUnresolved(),
			};
			if (!press(qt::card_action::resolve)) {
				return fail("no-resolve-action refusal=" + std::to_string(static_cast<int>(cards->resolution_refusal())));
			}
			probe.Evidence.ResolveAsked = true;
			probe.Phase                 = state_type::phase::awaiting_resolved;
			return result::pending;
		}
		case state_type::phase::awaiting_resolved: {
			note("completions=" + std::to_string(implementation->ThreadResolutionCompletions)
				 + " refusals=" + std::to_string(implementation->ThreadResolutionRefusals)
				 + " failures=" + std::to_string(implementation->ThreadResolutionFailures)
				 + " resolutions=" + std::to_string(implementation->Fixture->review_mutation_evidence().Resolutions));
			if (implementation->ThreadResolutionFailures != 0U || implementation->ThreadResolutionRefusals != 0U) {
				return fail("resolution refused-or-failed notice=" + (window->conversation_panel() != nullptr ? window->conversation_panel()->notice_label()->text().toStdString() : std::string{ }));
			}
			if (implementation->ThreadResolutionCompletions == 0U) {
				return result::pending;
			}
			const auto thread = model->index_for_id(threadIdentity);
			if (!thread.isValid() || !thread.data(qt::github_conversation_model::resolved_role).toBool()) {
				return result::pending;
			}
			probe.Evidence.ThreadResolved = true;
			measureResolveCost();
			probe.UnresolvedWhenResolved = overviewUnresolved();
			// [[BPR-1741]]. With reactions served, the course goes on:
			// Unresolve conversation at the thread's end, then reactions.
			if (!implementation->ReviewMutationHost->capabilities().Reacts) {
				// [[BPR-1774]]. With the description served, it goes on there.
				if (implementation->ReviewMutationHost->capabilities().EditsDescription) {
					probe.Phase = state_type::phase::awaiting_description_offered;
					return result::pending;
				}
				// [[BPR-1775]]. With comments edited, it goes on to my reply.
				if (implementation->ReviewMutationHost->capabilities().EditsComments) {
					probe.Phase = state_type::phase::awaiting_comment_edit_offered;
					return result::pending;
				}
				return goOnToReviewers();
			}
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			auto* const reopen = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadResolveConversation")) : nullptr;
			if (reopen == nullptr || !reopen->isEnabled() || reopen->text() != QStringLiteral("Unresolve conversation")) {
				return fail("no-unresolve-conversation");
			}
			auto* const gate = implementation->ReviewMutationDecisionGate.get();
			if (gate == nullptr) {
				return fail("no-reopen-decision-gate");
			}
			gate->hold();
			const auto release = qScopeGuard([gate] { gate->release(); });
			reopen->click();
			// Observe the synchronous gesture before its native terminal can
			// clear Pending. The next phase still requires confirmed reopening.
			const auto shown                 = std::ranges::find(cards->cards(), std::string{ threadIdentity }, &qt::thread_card::Id);
			probe.Evidence.ReopenShownAtOnce = shown != cards->cards().end() && !shown->Resolved && shown->ResolutionPending;
			if (!probe.Evidence.ReopenShownAtOnce) {
				return fail("reopen-not-shown-at-once");
			}
			probe.Phase = state_type::phase::awaiting_reopened;
			return result::pending;
		}
		case state_type::phase::awaiting_reopened: {
			note("completions=" + std::to_string(implementation->ThreadResolutionCompletions) + " failures=" + std::to_string(implementation->ThreadResolutionFailures));
			if (implementation->ThreadResolutionFailures != 0U || implementation->ThreadResolutionRefusals != 0U) {
				return fail("reopen-refused-or-failed");
			}
			const auto thread = model->index_for_id(threadIdentity);
			const auto card   = std::ranges::find(cards->cards(), std::string{ threadIdentity }, &qt::thread_card::Id);
			if (implementation->ThreadResolutionCompletions < 2U || !thread.isValid() || thread.data(qt::github_conversation_model::resolved_role).toBool()
				|| card == cards->cards().end() || card->Resolved || card->ResolutionPending) {
				return result::pending;
			}
			probe.Evidence.Reopened = true;
			measureResolveCost();
			if (probe.ResolveBaseline && probe.UnresolvedWhenResolved) {
				const auto before                     = probe.ResolveBaseline->Unresolved;
				probe.Evidence.ResolveOverviewCounted = before > 0U && *probe.UnresolvedWhenResolved == before - 1U && overviewUnresolved() == before;
			}
			note("overviewUnresolved=" + std::to_string(probe.ResolveBaseline ? probe.ResolveBaseline->Unresolved : 0U) + "/"
				 + std::to_string(probe.UnresolvedWhenResolved.value_or(0U)) + "/" + std::to_string(overviewUnresolved()));
			probe.Evidence.ResolveMinimal = probe.Evidence.ResolveReads == 0 && probe.Evidence.ResolveProviderCalls == 2U
				&& probe.Evidence.ResolveModelResets == 0U && !probe.Evidence.ResolveRefreshInFlight && probe.Evidence.ResolveOverviewCounted;
			probe.Phase = state_type::phase::awaiting_reactions_read;
			return result::pending;
		}
		case state_type::phase::awaiting_reactions_read: {
			const auto post  = std::string{ "review-comment:PRRC_fixture_1" };
			const auto shown = cards->reactions_of(post);
			note("read=" + std::to_string(shown.has_value() ? 1 : 0) + " reads=" + std::to_string(implementation->Fixture->evidence().AccountReadCount));
			if (!shown.has_value()) {
				return result::pending;
			}
			const auto* const thumbs     = core::find_reaction_group(*shown, core::reaction_content::thumbs_up);
			probe.Evidence.ReactionsRead = thumbs != nullptr && thumbs->Count == 2U && !thumbs->ViewerReacted;
			if (!probe.Evidence.ReactionsRead) {
				return fail("reactions-not-as-recorded");
			}
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			auto* const chip = reaction_chip_of(*cards, post, core::reaction_content::thumbs_up);
			if (chip == nullptr || !chip->isEnabled()) {
				return fail("no-thumbs-up-chip");
			}
			auto* const gate = implementation->ReviewMutationDecisionGate.get();
			if (gate == nullptr) {
				return fail("no-reaction-decision-gate");
			}
			gate->hold();
			const auto release = qScopeGuard([gate] { gate->release(); });
			chip->click();
			// Keep the native answer behind this immediate observation. The
			// next phase still requires the confirmed reaction and no Pending.
			const auto        now        = cards->reactions_of(post);
			const auto* const pending    = now ? core::find_reaction_group(*now, core::reaction_content::thumbs_up) : nullptr;
			probe.Evidence.ReactionShown = pending != nullptr && pending->Count == 3U && pending->ViewerReacted && pending->Pending;
			if (!probe.Evidence.ReactionShown) {
				return fail("reaction-not-shown-at-once");
			}
			probe.Phase = state_type::phase::awaiting_reaction_confirmed;
			return result::pending;
		}
		case state_type::phase::awaiting_reaction_confirmed:
		case state_type::phase::awaiting_reaction_removed:
		case state_type::phase::awaiting_picker_reaction:    {
			const auto post = std::string{ "review-comment:PRRC_fixture_1" };
			note("completions=" + std::to_string(implementation->ReactionCompletions) + " failures=" + std::to_string(implementation->ReactionFailures)
				 + " asked=" + std::to_string(implementation->Fixture->review_mutation_evidence().Reactions));
			if (implementation->ReactionFailures != 0U) {
				return fail("reaction-failed");
			}
			const auto shown = cards->reactions_of(post);
			if (!shown.has_value()) {
				return result::pending;
			}
			const auto* const thumbs = core::find_reaction_group(*shown, core::reaction_content::thumbs_up);
			const auto* const heart  = core::find_reaction_group(*shown, core::reaction_content::heart);
			if (probe.Phase == state_type::phase::awaiting_reaction_confirmed) {
				if (implementation->ReactionCompletions < 1U || thumbs == nullptr || thumbs->Pending) {
					return result::pending;
				}
				probe.Evidence.ReactionConfirmed = thumbs->Count == 3U && thumbs->ViewerReacted;
				if (!probe.Evidence.ReactionConfirmed) {
					return fail("reaction-not-confirmed");
				}
				cards->open_thread(std::string{ threadIdentity });
				QCoreApplication::processEvents();
				auto* const chip = reaction_chip_of(*cards, post, core::reaction_content::thumbs_up);
				if (chip == nullptr) {
					return fail("no-chip-to-remove");
				}
				chip->click();
				probe.Phase = state_type::phase::awaiting_reaction_removed;
				return result::pending;
			}
			if (probe.Phase == state_type::phase::awaiting_reaction_removed) {
				if (implementation->ReactionCompletions < 2U || thumbs == nullptr || thumbs->Pending) {
					return result::pending;
				}
				probe.Evidence.ReactionRemoved = thumbs->Count == 2U && !thumbs->ViewerReacted;
				if (!probe.Evidence.ReactionRemoved) {
					return fail("reaction-not-removed");
				}
				// The picker, by keyboard alone: five steps right is heart.
				cards->open_thread(std::string{ threadIdentity });
				QCoreApplication::processEvents();
				auto* const view  = post_view_of(*cards, post);
				auto* const react = view != nullptr ? view->findChild<QAbstractButton*>(QStringLiteral("threadPostReact")) : nullptr;
				if (react == nullptr || react->isHidden()) {
					return fail("no-add-reaction");
				}
				react->click();
				QCoreApplication::processEvents();
				auto* picker = static_cast<QWidget*>(nullptr);
				for (auto* const widget : QApplication::topLevelWidgets()) {
					if (widget->objectName() == QStringLiteral("reactionPicker") && widget->isVisible()) {
						picker = widget;
					}
				}
				if (picker == nullptr || picker->focusWidget() == nullptr) {
					return fail("no-picker");
				}
				const auto typeKey = [picker](const int aKey) {
					auto event = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
					QCoreApplication::sendEvent(picker->focusWidget() != nullptr ? picker->focusWidget() : picker, &event);
				};
				for (auto step = 0; step < 5; ++step) {
					typeKey(Qt::Key_Right);
				}
				typeKey(Qt::Key_Return);
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::awaiting_picker_reaction;
				return result::pending;
			}
			if (implementation->ReactionCompletions < 3U || heart == nullptr || heart->Pending) {
				return result::pending;
			}
			probe.Evidence.PickerReacted = heart->Count == 1U && heart->ViewerReacted;
			if (!probe.Evidence.PickerReacted) {
				return fail("picker-reaction-not-heart");
			}
			probe.ReadsBeforeRefresh = implementation->Fixture->evidence().AccountReadCount;
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
				refresh->trigger();
			} else {
				return fail("no-refresh");
			}
			probe.Phase = state_type::phase::awaiting_reaction_reconciled;
			return result::pending;
		}
		case state_type::phase::awaiting_reaction_reconciled: {
			const auto post  = std::string{ "review-comment:PRRC_fixture_1" };
			const auto shown = cards->reactions_of(post);
			note("reads=" + std::to_string(implementation->Fixture->evidence().AccountReadCount) + " overlay=" + std::to_string(implementation->ConversationOverlay.Reactions.size()));
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh || !implementation->ConversationOverlay.Reactions.empty()
				|| !shown.has_value()) {
				return result::pending;
			}
			const auto* const thumbs          = core::find_reaction_group(*shown, core::reaction_content::thumbs_up);
			const auto* const heart           = core::find_reaction_group(*shown, core::reaction_content::heart);
			probe.Evidence.ReactionReconciled = thumbs != nullptr && thumbs->Count == 2U && !thumbs->ViewerReacted && heart != nullptr && heart->Count == 1U && heart->ViewerReacted
				&& !heart->Pending;
			if (!probe.Evidence.ReactionReconciled) {
				return fail("reaction-not-reconciled");
			}
			if (implementation->ReviewMutationHost->capabilities().EditsDescription) {
				probe.Phase = state_type::phase::awaiting_description_offered;
				return result::pending;
			}
			if (implementation->ReviewMutationHost->capabilities().EditsComments) {
				probe.Phase = state_type::phase::awaiting_comment_edit_offered;
				return result::pending;
			}
			return goOnToReviewers();
		}
		// [[BPR-1774]]. Overview offers the author Edit; Save performs exactly
		// one write, and the next read shows GitHub's body. A second edit
		// meets a body another client changed: nothing is written, the typed
		// text stays, and editing goes on from GitHub's newer text.
		case state_type::phase::awaiting_description_offered: {
			auto* const page = window->overview_page();
			window->choose_modality(qt::review_modality::overview);
			QCoreApplication::processEvents();
			note("editable=" + std::to_string(page->description_editable() ? 1 : 0) + " visible=" + std::to_string(page->description_edit_button()->isVisible() ? 1 : 0));
			if (!page->description_editable() || !page->projection().has_value()) {
				return result::pending;
			}
			probe.Evidence.DescriptionOffered = page->description_edit_button()->isVisible() || page->description_add_button()->isVisible();
			if (!probe.Evidence.DescriptionOffered) {
				return fail("edit-not-shown");
			}
			(page->description_edit_button()->isVisible() ? static_cast<QAbstractButton*>(page->description_edit_button()) : page->description_add_button())->click();
			QCoreApplication::processEvents();
			auto* const text = page->description_editor();
			if (!page->description_editing() || text->raw() != page->projection()->Body) {
				return fail("editor-not-on-body");
			}
			probe.ReadsBeforeRefresh = implementation->Fixture->evidence().AccountReadCount;
			text->selectAll();
			text->insertPlainText(QStringLiteral("Saved from the Overview editor.\n\n- one write"));
			QCoreApplication::processEvents();
			page->description_save_button()->click();
			probe.Phase = state_type::phase::awaiting_description_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_description_saved: {
			auto* const page     = window->overview_page();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->DescriptionEditCompletions) + " failures=" + std::to_string(implementation->DescriptionEditFailures)
				 + " refusals=" + std::to_string(implementation->DescriptionEditRefusals) + " edits=" + std::to_string(evidence.DescriptionEdits)
				 + " writes=" + std::to_string(evidence.DescriptionWrites) + " status=" + page->description_status()->text().toStdString());
			if (implementation->DescriptionEditFailures != 0U || implementation->DescriptionEditRefusals != 0U || implementation->DescriptionEditConflicts != 0U) {
				return fail("save-refused-or-failed");
			}
			if (implementation->DescriptionEditCompletions < 1U || page->description_editing()) {
				return result::pending;
			}
			probe.Evidence.DescriptionSavedOnce = evidence.DescriptionEdits == 1U && evidence.DescriptionWrites == 1U;
			if (!probe.Evidence.DescriptionSavedOnce) {
				return fail("not-exactly-one-write");
			}
			constexpr auto saved = std::string_view{ "Saved from the Overview editor.\n\n- one write" };
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh || !page->projection().has_value() || page->projection()->Body != saved) {
				return result::pending;
			}
			probe.Evidence.DescriptionShownAfterRead = page->body_view()->original_body() == saved;
			if (!probe.Evidence.DescriptionShownAfterRead) {
				return fail("read-not-shown");
			}
			// Another client changes the description while this one edits.
			if (!page->begin_description_edit()) {
				return fail("no-second-edit");
			}
			auto* const text = page->description_editor();
			auto        end  = text->textCursor();
			end.movePosition(QTextCursor::End);
			text->setTextCursor(end);
			text->insertPlainText(QStringLiteral("\n- mine, typed second"));
			QCoreApplication::processEvents();
			implementation->Fixture->stage_server_description("Changed on GitHub by someone else.");
			page->description_save_button()->click();
			probe.Phase = state_type::phase::awaiting_description_conflict;
			return result::pending;
		}
		case state_type::phase::awaiting_description_conflict: {
			auto* const page     = window->overview_page();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("conflicts=" + std::to_string(implementation->DescriptionEditConflicts) + " edits=" + std::to_string(evidence.DescriptionEdits)
				 + " writes=" + std::to_string(evidence.DescriptionWrites) + " status=" + page->description_status()->text().toStdString());
			if (implementation->DescriptionEditFailures != 0U || implementation->DescriptionEditCompletions != 1U) {
				return fail("conflict-written-or-failed");
			}
			if (implementation->DescriptionEditConflicts < 1U) {
				return result::pending;
			}
			auto* const text                          = page->description_editor();
			probe.Evidence.DescriptionConflictRefused = evidence.DescriptionEdits == 2U && evidence.DescriptionWrites == 1U && page->description_editing()
				&& text->raw().ends_with("- mine, typed second") && page->description_newer_button()->isVisible()
				&& page->description_status()->text().contains(QStringLiteral("changed the description on GitHub"));
			if (!probe.Evidence.DescriptionConflictRefused) {
				return fail("conflict-not-refused");
			}
			page->description_newer_button()->click();
			QCoreApplication::processEvents();
			probe.Evidence.DescriptionNewerLoaded = text->raw() == "Changed on GitHub by someone else." && page->description_original() == text->raw();
			if (!probe.Evidence.DescriptionNewerLoaded) {
				return fail("newer-not-loaded");
			}
			page->description_cancel_button()->click();
			QCoreApplication::processEvents();
			if (implementation->ReviewMutationHost->capabilities().EditsComments) {
				probe.Phase = state_type::phase::awaiting_comment_edit_offered;
				return result::pending;
			}
			return goOnToReviewers();
		}
		// [[BPR-1775]]. My published reply offers Edit in Discuss and another's
		// comment does not; Save performs exactly one write, and the next read
		// shows GitHub's body. A second edit meets a body another client
		// changed: nothing is written, the typed text stays, and editing goes
		// on from GitHub's newer text.
		case state_type::phase::awaiting_comment_edit_offered: {
			constexpr auto mine = std::string_view{ "review-comment:fixture-comment-1" };
			// Every post in the recording is the viewer's: the thread's first
			// post becomes another account's on the next read.
			if (!probe.CommentRootRestaged) {
				auto* const refresh = window->github_refresh_action();
				if (refresh == nullptr || !refresh->isEnabled()) {
					return result::pending;
				}
				implementation->Fixture->stage_server_root("PRRT_fixture_1", std::nullopt, std::nullopt, std::pair<std::string, std::string>{ "U_fixture_other", "octo-other" });
				probe.CommentRootRestaged = true;
				probe.ReadsBeforeRefresh  = implementation->Fixture->evidence().AccountReadCount;
				refresh->trigger();
				return result::pending;
			}
			// A refresh the window was already running may have answered from
			// before the change; ask again until a later read arrives.
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh) {
				if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
					refresh->trigger();
				}
				return result::pending;
			}
			window->choose_modality(qt::review_modality::discuss);
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			const auto others = cards->comment_editable("review-comment:PRRC_fixture_1");
			note("offered=" + std::to_string(cards->comment_edit_offered() ? 1 : 0) + " mine=" + std::to_string(cards->comment_editable(mine) ? 1 : 0)
				 + " others=" + std::to_string(others ? 1 : 0) + " open=" + cards->open_thread_id());
			// Until the read naming another author arrives, the first post is
			// still the viewer's.
			if (!cards->comment_edit_offered() || cards->open_card() == nullptr || others || !cards->comment_editable(mine)) {
				return result::pending;
			}
			for (auto* const post : cards->open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
				if (post->property("postIdentity").toString() == QStringLiteral("review-comment:PRRC_fixture_1")
					&& (post->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit")) == nullptr || !post->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit"))->isHidden())) {
					return fail("edit-shown-on-another's-comment");
				}
			}
			auto* button = static_cast<QAbstractButton*>(nullptr);
			for (auto* const post : cards->open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
				if (post->property("postIdentity").toString() == QString::fromUtf8(mine.data(), static_cast<qsizetype>(mine.size()))) {
					button = post->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit"));
				}
			}
			probe.Evidence.CommentEditOffered = button != nullptr && !button->isHidden();
			if (!probe.Evidence.CommentEditOffered) {
				return fail("edit-not-shown-on-my-reply");
			}
			button->click();
			QCoreApplication::processEvents();
			auto* const text = cards->comment_editor();
			if (text == nullptr || text->raw() != replyBody) {
				return fail("editor-not-on-reply");
			}
			probe.ReadsBeforeRefresh = implementation->Fixture->evidence().AccountReadCount;
			text->selectAll();
			text->insertPlainText(QStringLiteral("Edited from the conversation."));
			QCoreApplication::processEvents();
			cards->comment_edit_save_button()->click();
			probe.Phase = state_type::phase::awaiting_comment_edit_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_comment_edit_saved: {
			constexpr auto mine     = std::string_view{ "review-comment:fixture-comment-1" };
			constexpr auto saved    = std::string_view{ "Edited from the conversation." };
			const auto     evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->CommentEditCompletions) + " failures=" + std::to_string(implementation->CommentEditFailures)
				 + " refusals=" + std::to_string(implementation->CommentEditRefusals) + " edits=" + std::to_string(evidence.CommentEdits)
				 + " writes=" + std::to_string(evidence.CommentEditWrites) + " status=" + (cards->comment_edit_status() != nullptr ? cards->comment_edit_status()->text().toStdString() : std::string{ }));
			if (implementation->CommentEditFailures != 0U || implementation->CommentEditRefusals != 0U || implementation->CommentEditConflicts != 0U) {
				return fail("save-refused-or-failed");
			}
			if (implementation->CommentEditCompletions < 1U || cards->comment_editing().has_value()) {
				return result::pending;
			}
			probe.Evidence.CommentEditSavedOnce = evidence.CommentEdits == 1U && evidence.CommentEditWrites == 1U;
			if (!probe.Evidence.CommentEditSavedOnce) {
				return fail("not-exactly-one-write");
			}
			const auto reply = model->index_for_id(std::string{ mine });
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh || !reply.isValid()
				|| reply.data(qt::github_conversation_model::body_role).toString() != QString::fromUtf8(saved.data(), static_cast<qsizetype>(saved.size()))) {
				return result::pending;
			}
			probe.Evidence.CommentEditShownAfterRead = true;
			// Another client changes the reply while this one edits it.
			if (!cards->begin_comment_edit(std::string{ mine }) || cards->comment_editor() == nullptr) {
				return fail("no-second-edit");
			}
			auto* const text = cards->comment_editor();
			auto        end  = text->textCursor();
			end.movePosition(QTextCursor::End);
			text->setTextCursor(end);
			text->insertPlainText(QStringLiteral(" Typed second."));
			QCoreApplication::processEvents();
			implementation->Fixture->stage_server_comment("fixture-comment-1", "Changed on GitHub by someone else.");
			cards->comment_edit_save_button()->click();
			probe.Phase = state_type::phase::awaiting_comment_edit_conflict;
			return result::pending;
		}
		case state_type::phase::awaiting_comment_edit_conflict: {
			const auto evidence = implementation->Fixture->review_mutation_evidence();
			note("conflicts=" + std::to_string(implementation->CommentEditConflicts) + " edits=" + std::to_string(evidence.CommentEdits)
				 + " writes=" + std::to_string(evidence.CommentEditWrites));
			if (implementation->CommentEditFailures != 0U || implementation->CommentEditCompletions != 1U) {
				return fail("conflict-written-or-failed");
			}
			if (implementation->CommentEditConflicts < 1U) {
				return result::pending;
			}
			auto* const text                          = cards->comment_editor();
			probe.Evidence.CommentEditConflictRefused = evidence.CommentEdits == 2U && evidence.CommentEditWrites == 1U && text != nullptr
				&& text->raw().ends_with(" Typed second.") && cards->comment_edit_newer_button()->isVisible()
				&& cards->comment_edit_status()->text().contains(QStringLiteral("changed this comment on GitHub"));
			if (!probe.Evidence.CommentEditConflictRefused) {
				return fail("conflict-not-refused");
			}
			cards->comment_edit_newer_button()->click();
			QCoreApplication::processEvents();
			probe.Evidence.CommentEditNewerLoaded = text->raw() == "Changed on GitHub by someone else." && cards->comment_edit_original() == text->raw();
			if (!probe.Evidence.CommentEditNewerLoaded) {
				return fail("newer-not-loaded");
			}
			cards->comment_edit_cancel_button()->click();
			QCoreApplication::processEvents();
			return goOnToReviewers();
		}
		// [[BPR-1804]]. People offers Request review; the picker lists GitHub's
		// suggestion, a typed query narrows it to one person, and choosing them
		// by keyboard writes exactly once; choosing them again sends nothing.
		case state_type::phase::awaiting_reviewer_offered: {
			auto* const page = window->overview_page();
			window->choose_modality(qt::review_modality::overview);
			QCoreApplication::processEvents();
			if (!page->reviewer_requests_offered()) {
				return result::pending;
			}
			probe.Evidence.ReviewerOffered = page->request_review_button()->isVisibleTo(page);
			if (!probe.Evidence.ReviewerOffered) {
				return fail("request-review-not-shown");
			}
			page->request_review_button()->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_reviewer_listed;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_listed: {
			auto* const picker = window->overview_page()->reviewer_picker_popup();
			if (picker == nullptr || implementation->ReviewerPicker.Searching) {
				return result::pending;
			}
			if (picker->listed_logins() != QStringList{ "fixture-other-reviewer", "octo-maintainer", "octo-outsider" }) {
				return fail("picker-not-listed");
			}
			for (const auto character : QStringLiteral("maint")) {
				auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString{ character } };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			probe.Phase = state_type::phase::awaiting_reviewer_filtered;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_filtered: {
			auto* const picker   = window->overview_page()->reviewer_picker_popup();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("searches=" + std::to_string(evidence.ReviewerSearches) + " listed=" + picker->listed_logins().join(QLatin1Char{ ',' }).toStdString());
			if (implementation->ReviewerPicker.Searching || evidence.ReviewerSearches < 2U) {
				return result::pending;
			}
			probe.Evidence.ReviewerSearched = picker->listed_logins() == QStringList{ "octo-maintainer" } && implementation->ReviewerPicker.Query == "maint";
			if (!probe.Evidence.ReviewerSearched) {
				return fail("search-not-narrowed");
			}
			// [[BPR-1849]]. What requesting costs is counted from the choice.
			probe.ReviewerBaseline = state_type::reviewer_baseline{
				.Reads          = implementation->Fixture->evidence().AccountReadCount,
				.OverviewBuilds = implementation->OverviewBuilds,
				.OverviewMoves  = implementation->OverviewReviewRequestUpdates,
				.Chosen         = std::chrono::steady_clock::now(),
			};
			for (const auto key : { Qt::Key_Down, Qt::Key_Return }) {
				auto down = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			// The choice dispatched synchronously: People lists them now, pending.
			if (reviewerRow("octo-maintainer") == std::optional{ true }) {
				probe.Evidence.ReviewerShownAtOnce = true;
				probe.Evidence.ReviewerShownMicros = microsSinceChoice();
			}
			probe.Phase = state_type::phase::awaiting_reviewer_requested;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_requested: {
			auto* const picker   = window->overview_page()->reviewer_picker_popup();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->ReviewerRequestCompletions) + " failures=" + std::to_string(implementation->ReviewerRequestFailures)
				 + " writes=" + std::to_string(evidence.ReviewerWrites) + " status=" + picker->status()->text().toStdString());
			if (implementation->ReviewerRequestFailures != 0U) {
				return fail("request-failed");
			}
			if (implementation->ReviewerRequestCompletions < 1U) {
				return result::pending;
			}
			if (!probe.Evidence.ReviewerRequestedOnce) {
				probe.Evidence.ReviewerRequestedOnce = evidence.ReviewerWrites == 1U
					&& picker->status()->text() == QStringLiteral("Requested a review from octo-maintainer.");
				if (!probe.Evidence.ReviewerRequestedOnce) {
					return fail("not-requested-once");
				}
				// [[BPR-1849]]. GitHub's answer confirms the row People shows.
				probe.Evidence.ReviewerAnsweredMicros = microsSinceChoice();
				probe.Evidence.ReviewerConfirmed      = reviewerRow("octo-maintainer") == std::optional{ false };
				if (probe.Evidence.ReviewerShownMicros < 0 && reviewerRow("octo-maintainer").has_value()) {
					probe.Evidence.ReviewerShownMicros = probe.Evidence.ReviewerAnsweredMicros;
				}
				measureReviewerCost();
			}
			measureReviewerCost();
			// Whatever the answer began settles before the course goes on.
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && !refresh->isEnabled()) {
				return result::pending;
			}
			if (probe.Evidence.ReviewerShownMicros < 0 && reviewerRow("octo-maintainer").has_value()) {
				probe.Evidence.ReviewerShownMicros = microsSinceChoice();
			}
			// [[BPR-1818]]. A native popup may have been dismissed while the
			// first request and its refresh completed. A closed picker correctly
			// refuses a choice. Reopen through People and narrow the public list
			// before exercising the already-requested row again.
			if (!implementation->ReviewerPicker.Open || !picker->isVisible()) {
				window->overview_page()->request_review_button()->click();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (implementation->ReviewerPicker.Searching) {
				return result::pending;
			}
			if (implementation->ReviewerPicker.Query != "maint") {
				for (const auto character : QStringLiteral("maint")) {
					auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString{ character } };
					QCoreApplication::sendEvent(picker->search_field(), &down);
				}
				return result::pending;
			}
			if (picker->listed_logins() != QStringList{ "octo-maintainer" }) {
				return fail("repeat-reviewer-not-listed");
			}
			const auto asked   = implementation->ReviewerRequestsAsked;
			const auto context = [&] {
				const auto* const selected = picker->people()->currentItem();
				return "open=" + std::to_string(implementation->ReviewerPicker.Open)
					+ " visible=" + std::to_string(picker->isVisible())
					+ " selected=" + (selected != nullptr ? selected->data(Qt::AccessibleTextRole).toString().toStdString() : "<none>")
					+ " asked=" + std::to_string(implementation->ReviewerRequestsAsked)
					+ " writes=" + std::to_string(implementation->Fixture->review_mutation_evidence().ReviewerWrites)
					+ " status=" + picker->status()->text().toStdString();
			};
			note("repeat-before " + context());
			for (const auto key : { Qt::Key_Down, Qt::Key_Return }) {
				auto down = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			QCoreApplication::processEvents();
			note("repeat-after " + context());
			probe.Evidence.ReviewerNotRepeated = asked == 1U && implementation->ReviewerRequestsAsked == asked && implementation->Fixture->review_mutation_evidence().ReviewerWrites == 1U
				&& picker->status()->text() == QStringLiteral("octo-maintainer already has a review request.");
			if (!probe.Evidence.ReviewerNotRepeated) {
				return fail("request-repeated");
			}
			measureReviewerCost();
			// [[BPR-1849]]. GitHub refuses the next request: People shows the
			// person at once, then withdraws them, and the picker says why.
			implementation->Fixture->stage_reviewer_request_refusal();
			for (int erase = 0; erase < 5; ++erase) {
				auto back = QKeyEvent{ QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &back);
			}
			for (const auto character : QStringLiteral("other")) {
				auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString{ character } };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			probe.Phase = state_type::phase::awaiting_reviewer_refusal_listed;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_refusal_listed: {
			auto* const picker = window->overview_page()->reviewer_picker_popup();
			note("query=" + implementation->ReviewerPicker.Query + " listed=" + picker->listed_logins().join(QLatin1Char{ ',' }).toStdString());
			if (implementation->ReviewerPicker.Searching || implementation->ReviewerPicker.Query != "other") {
				return result::pending;
			}
			if (picker->listed_logins() != QStringList{ "fixture-other-reviewer" }) {
				return fail("refusal-reviewer-not-listed");
			}
			for (const auto key : { Qt::Key_Down, Qt::Key_Return }) {
				auto down = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			probe.Evidence.ReviewerRolledBack = reviewerRow("fixture-other-reviewer") == std::optional{ true };
			probe.Phase                       = state_type::phase::awaiting_reviewer_refused;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_refused: {
			auto* const picker = window->overview_page()->reviewer_picker_popup();
			note("failures=" + std::to_string(implementation->ReviewerRequestFailures) + " status=" + picker->status()->text().toStdString());
			if (implementation->ReviewerRequestFailures < 1U) {
				return result::pending;
			}
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && !refresh->isEnabled()) {
				return result::pending;
			}
			probe.Evidence.ReviewerRolledBack = probe.Evidence.ReviewerRolledBack && !reviewerRow("fixture-other-reviewer").has_value()
				&& reviewerRow("octo-maintainer") == std::optional{ false } && implementation->Fixture->review_mutation_evidence().ReviewerWrites == 1U
				&& picker->status()->text() == QStringLiteral("GitHub did not let you request a review from fixture-other-reviewer.");
			measureReviewerCost();
			const auto& evidence           = probe.Evidence;
			probe.Evidence.ReviewerMinimal = evidence.ReviewerReads == 0 && evidence.ReviewerOverviewBuilds == 0U && !evidence.ReviewerRefreshInFlight
				&& evidence.ReviewerShownAtOnce && evidence.ReviewerConfirmed && evidence.ReviewerRolledBack;
			picker->hide();
			QCoreApplication::processEvents();
			return goOnToDraftState();
		}
		// [[BPR-1852]]. The author of a draft sees it said in the title row and
		// in Overview, with Ready for review in the banner and the Pull
		// Request menu. Ready for review takes the chips away at once, writes
		// exactly once and reads nothing; GitHub's answer offers Convert to
		// draft. A refused Convert to draft is shown at once, then withdrawn,
		// with words beside the button.
		case state_type::phase::awaiting_draft_offered: {
			auto* const page = window->overview_page();
			window->choose_modality(qt::review_modality::overview);
			QCoreApplication::processEvents();
			note("offer=" + std::to_string(page->draft_state_offer().has_value() ? static_cast<int>(*page->draft_state_offer()) : -1) + " chip=" + std::to_string(page->draft_chip()->isVisible() ? 1 : 0));
			if (page->draft_state_offer() != core::draft_state_change::ready_for_review || !page->projection().has_value()) {
				return result::pending;
			}
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && !refresh->isEnabled()) {
				return result::pending;
			}
			// What the courses before began settles first: the Overview is
			// projected from the accepted read and has not been projected
			// again for a few turns of the event loop, so what follows is
			// counted from the gesture alone.
			if (!implementation->WindowRead || !implementation->WindowRead->accepted_snapshot()
				|| implementation->OverviewResolutionsSnapshot != std::optional{ implementation->WindowRead->accepted_snapshot()->Snapshot }) {
				return result::pending;
			}
			if (probe.DraftQuietBuilds != std::optional{ implementation->OverviewBuilds }) {
				probe.DraftQuietBuilds = implementation->OverviewBuilds;
				probe.DraftQuietTicks  = 0U;
				return result::pending;
			}
			if (++probe.DraftQuietTicks < 5U) {
				return result::pending;
			}
			auto* const action          = window->draft_state_action();
			probe.Evidence.DraftOffered = window->strip_draft_shown() && page->draft_chip()->isVisible()
				&& page->ready_for_review_button()->isVisible() && page->ready_for_review_button()->isEnabled() && action != nullptr && action->isVisible()
				&& action->text().remove(QLatin1Char{ '&' }) == QStringLiteral("Mark Ready for Review") && !page->convert_to_draft_button()->isVisible();
			if (!probe.Evidence.DraftOffered) {
				return fail("draft-not-offered strip=" + std::to_string(window->strip_draft_shown() ? 1 : 0) + " ready=" + std::to_string(page->ready_for_review_button()->isVisible() ? 1 : 0));
			}
			probe.DraftBaseline = state_type::reviewer_baseline{
				.Reads          = implementation->Fixture->evidence().AccountReadCount,
				.OverviewBuilds = implementation->OverviewBuilds,
				.OverviewMoves  = implementation->OverviewDraftStateUpdates,
				.Chosen         = std::chrono::steady_clock::now(),
			};
			page->ready_for_review_button()->click();
			// The gesture dispatched synchronously: the chips are gone now, and
			// the button says GitHub is being asked.
			probe.Evidence.DraftShownAtOnce = !window->strip_draft_shown() && !page->draft_chip()->isVisible() && page->ready_for_review_button()->isVisible()
				&& page->ready_for_review_button()->text() == QStringLiteral("Marking ready…") && !page->ready_for_review_button()->isEnabled()
				&& page->projection().has_value() && !page->projection()->Draft && page->projection()->DraftPending;
			note("shown strip=" + std::to_string(window->strip_draft_shown() ? 1 : 0) + " chip=" + std::to_string(page->draft_chip()->isVisible() ? 1 : 0)
				 + " button=" + page->ready_for_review_button()->text().toStdString()
				 + " enabled=" + std::to_string(page->ready_for_review_button()->isEnabled() ? 1 : 0)
				 + " draft=" + std::to_string(page->projection().has_value() && page->projection()->Draft ? 1 : 0)
				 + " pending=" + std::to_string(page->projection().has_value() && page->projection()->DraftPending ? 1 : 0));
			probe.Phase = state_type::phase::awaiting_draft_ready;
			return result::pending;
		}
		case state_type::phase::awaiting_draft_ready: {
			auto* const page     = window->overview_page();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->DraftStateCompletions) + " failures=" + std::to_string(implementation->DraftStateFailures)
				 + " writes=" + std::to_string(evidence.DraftStateWrites) + " status=" + page->draft_state_status(core::draft_state_change::ready_for_review)->text().toStdString());
			if (implementation->DraftStateFailures != 0U || implementation->DraftStateRefusals != 0U) {
				return fail("ready-refused-or-failed");
			}
			if (implementation->DraftStateCompletions < 1U) {
				return result::pending;
			}
			auto* const refresh = window->github_refresh_action();
			if (probe.Evidence.DraftAnsweredMicros < 0) {
				probe.Evidence.DraftAnsweredMicros  = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - probe.DraftBaseline->Chosen).count();
				probe.Evidence.DraftRefreshInFlight = refresh != nullptr && !refresh->isEnabled();
			}
			probe.Evidence.DraftReads           = implementation->Fixture->evidence().AccountReadCount - probe.DraftBaseline->Reads;
			probe.Evidence.DraftOverviewBuilds  = static_cast<std::size_t>(implementation->OverviewBuilds - probe.DraftBaseline->OverviewBuilds);
			probe.Evidence.DraftOverviewUpdates = implementation->OverviewDraftStateUpdates - probe.DraftBaseline->OverviewMoves;
			probe.Evidence.DraftReadyOnce       = evidence.DraftStateChanges == 1U && evidence.DraftStateWrites == 1U && !page->ready_for_review_button()->isVisible()
				&& !page->draft_chip()->isVisible() && !window->strip_draft_shown() && page->projection().has_value() && !page->projection()->Draft
				&& !page->projection()->DraftPending && page->draft_state_offer() == core::draft_state_change::convert_to_draft && page->convert_to_draft_button()->isVisible()
				&& window->draft_state_action()->text().remove(QLatin1Char{ '&' }) == QStringLiteral("Convert to Draft");
			if (!probe.Evidence.DraftReadyOnce) {
				return fail("ready-not-confirmed");
			}
			// GitHub refuses the next change, asked from the menu: the chips
			// come back at once, then go, and the words say why.
			implementation->Fixture->stage_draft_state_refusal();
			window->draft_state_action()->trigger();
			probe.Evidence.DraftRolledBack = window->strip_draft_shown() && page->draft_chip()->isVisible() && page->projection()->DraftPending;
			note("converting strip=" + std::to_string(window->strip_draft_shown() ? 1 : 0) + " chip=" + std::to_string(page->draft_chip()->isVisible() ? 1 : 0)
				 + " pending=" + std::to_string(page->projection()->DraftPending ? 1 : 0) + " draft=" + std::to_string(page->projection()->Draft ? 1 : 0));
			probe.Phase = state_type::phase::awaiting_draft_refused;
			return result::pending;
		}
		case state_type::phase::awaiting_draft_refused: {
			auto* const page   = window->overview_page();
			auto* const status = page->draft_state_status(core::draft_state_change::convert_to_draft);
			note("failures=" + std::to_string(implementation->DraftStateFailures) + " status=" + status->text().toStdString());
			if (implementation->DraftStateFailures < 1U) {
				return result::pending;
			}
			probe.Evidence.DraftRolledBack = probe.Evidence.DraftRolledBack && !window->strip_draft_shown() && !page->draft_chip()->isVisible()
				&& !page->projection()->Draft && implementation->Fixture->review_mutation_evidence().DraftStateWrites == 1U && status->isVisible()
				&& status->text() == QStringLiteral("GitHub did not let you change whether this pull request is a draft. Nothing changed.")
				&& page->convert_to_draft_button()->isVisible() && page->convert_to_draft_button()->isEnabled();
			const auto& evidence        = probe.Evidence;
			probe.Evidence.DraftMinimal = evidence.DraftReads == 0 && evidence.DraftOverviewBuilds == 0U && !evidence.DraftRefreshInFlight && evidence.DraftShownAtOnce
				&& evidence.DraftReadyOnce && evidence.DraftRolledBack;
			if (!probe.Evidence.DraftRolledBack) {
				return fail("refusal-not-withdrawn");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed:
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::mark_fixture_read_through(
		github_application& aApplication, const std::string& aThread, const std::string& aComment) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->mark_conversation_read_through(aThread, aComment);
	}

	auto detail::github_application_fixture_access::apply_fixture_read_state(github_application& aApplication, const std::string& aFixture)
		-> std::optional<std::size_t>
	{
		auto file = QFile{ QString::fromStdString((std::filesystem::path{ BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT } / aFixture / "read_state.json").string()) };
		if (!file.open(QIODevice::ReadOnly)) {
			return std::nullopt;
		}
		const auto threads = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("threads")).toObject();
		auto       marked  = std::size_t{ 0U };
		for (auto entry = threads.begin(); entry != threads.end(); ++entry) {
			if (entry.value().isNull()) {
				continue;
			}
			if (!mark_fixture_read_through(aApplication, entry.key().toStdString(), entry.value().toString().toStdString())) {
				return std::nullopt;
			}
			++marked;
		}
		return marked;
	}
#endif
}
