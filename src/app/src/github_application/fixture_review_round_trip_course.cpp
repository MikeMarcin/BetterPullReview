#include "github_application/fixture_course_support.hpp"
#include "review_cycle_desktop.hpp"
#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/git_smart_transport_port.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_host_authority.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/review_mutation_application_host.hpp"
	#include "bpr/application/review_mutation_port.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/revisioned_review_draft.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/github_conversation_model.hpp"
	#include "bpr/qt/review_draft_editor.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_strip.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/profile_hosts.hpp"
	#include "github_application/review_mutation_selection.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAbstractScrollArea>
	#include <QAction>
	#include <QByteArray>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QEvent>
	#include <QFlags>
	#include <QLabel>
	#include <QListView>
	#include <QModelIndex>
	#include <QMouseEvent>
	#include <QPlainTextEdit>
	#include <QPoint>
	#include <QPointF>
	#include <QRect>
	#include <QString>
	#include <QTextBlock>
	#include <QTextCursor>
	#include <QTextDocument>
	#include <QTextStream>
	#include <QTreeView>
	#include <QVariant>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <chrono>
	#include <cstddef>
	#include <string_view>
	#include <vector>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::exercise_review_round_trip_composition(
		github_application& aApplication)
		-> detail::review_round_trip_composition_probe_result
	{
		using result               = detail::review_round_trip_composition_probe_result;
		using state_type           = review_round_trip_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReviewRoundTripCompositionProbe.has_value()) {
			implementation->ReviewRoundTripCompositionProbe.emplace();
			// [[BPR-1735]]. This course's off-hunk line exercises RFC-BPR-0022's
			// native attempt and fallback, its subject, so it places as a build
			// before the hunk rule did; the BPR-1735 courses cover the rule.
			implementation->WholeDiffPlacementDisabled = true;
		}
		auto& probe = *implementation->ReviewRoundTripCompositionProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		// [[BPR-1037]]. A failure gives back the decision gate the course may
		// be holding. A held gate keeps the lost publication outstanding, so
		// the run's shutdown would wait for it until the harness killed the
		// process, and the verdict would never print.
		const auto fail = [&probe, implementation] {
			const auto counts = implementation->Fixture ? implementation->Fixture->review_mutation_evidence() : detail::review_mutation_fixture_evidence{ };
			QTextStream{ stdout } << "SEMANTIC_COMMENT_PROBE_FAILED phase=" << static_cast<int>(probe.Phase)
								  << " submissions=" << counts.Submissions << " native=" << counts.NativePosts << " file=" << counts.FilePosts
								  << " stored=" << counts.StoredComments << " duplicate=" << counts.DuplicateReturned << " durable=" << counts.DurableJournalReads
								  << " sessions=" << counts.SessionConstructions << " draft=" << (implementation->ReviewDraft ? static_cast<int>(implementation->ReviewDraft->State) : -1) << '\n';

			probe.Phase = state_type::phase::failed;
			if (implementation->ReviewMutationDecisionGate != nullptr) {
				implementation->ReviewMutationDecisionGate->release();
			}
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			// [[BPR-1854]]. Native Ubuntu exceeded five seconds in distinct actor waits.
			// This functional course keeps its outer 40s
			// budget and all semantic predicates; only its phase watchdog grows.
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts, std::chrono::milliseconds{ 15'000 }) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto draftState = [implementation] {
			if (!implementation->ReviewDraft.has_value()) {
				return QStringLiteral("absent");
			}
			const auto state = core::name(implementation->ReviewDraft->State);
			return QString::fromUtf8(state.data(), static_cast<qsizetype>(state.size()));
		};
		const auto phaseName = [](const state_type::phase aPhase) {
			switch (aPhase) {
			case state_type::phase::awaiting_edited_body:    return "awaiting_edited_body";
			case state_type::phase::awaiting_fallback_draft: return "awaiting_fallback_draft";
			case state_type::phase::awaiting_fallback_body:  return "awaiting_fallback_body";
			case state_type::phase::awaiting_editor:
				return "awaiting_editor";
			case state_type::phase::awaiting_stale_refusal:
				return "awaiting_stale_refusal";
			case state_type::phase::awaiting_lost_effect:
				return "awaiting_lost_effect";
			case state_type::phase::awaiting_deferred_terminal:
				return "awaiting_deferred_terminal";
			case state_type::phase::awaiting_retry:
				return "awaiting_retry";
			case state_type::phase::awaiting_native_conversation:
				return "awaiting_native_conversation";
			case state_type::phase::awaiting_fallback_conversation:
				return "awaiting_fallback_conversation";
			case state_type::phase::passed:
				return "passed";
			case state_type::phase::failed:
				return "failed";
			}
			return "unknown";
		};
		// The file gesture composes the editor's draft once the selected
		// diff is published; every phase needs both.
		const auto draftFacts = [&](QTextStream& aStream) {
			const auto* const view = implementation->Window == nullptr || implementation->Window->files_drawer() == nullptr
				? nullptr
				: implementation->Window->files_drawer()->contents_view();
			aStream << "phase=" << phaseName(probe.Phase)
					<< " fixture=" << present(implementation->Fixture != nullptr)
					<< " window=" << present(implementation->Window != nullptr)
					<< " workbench_host=" << present(implementation->WorkbenchHost != nullptr)
					<< " mutation_host=" << present(implementation->ReviewMutationHost != nullptr)
					<< " review_draft=" << draftState()
					<< " off_hunk_anchor=" << present(probe.OffHunkAnchor.has_value())
					<< " private_ready=" << present(implementation->PrivateDraftsReady)
					<< " private_selection_pending=" << present(implementation->PrivateDraftSelectionPending)
					<< " semantic_selection=" << present(implementation->SelectedSemanticSelection.has_value())
					<< " pending_reply=" << present(implementation->PendingPrivateReply.has_value())
					<< " accepted_handoff=" << present(implementation->LastAcceptedHandoff.has_value())
					<< " last_selected_diff=" << present(implementation->LastSelectedDiff.has_value())
					<< " review_editor=" << present(implementation->Window != nullptr && implementation->Window->review_editor() != nullptr)
					<< " file_rows=" << (view == nullptr || view->model() == nullptr ? 0 : view->model()->rowCount())
					<< " current_row=" << (view == nullptr ? -1 : view->currentIndex().row())
					<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
					<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures);
		};
		if (probe.Phase == state_type::phase::awaiting_editor && implementation->Window && implementation->LastSelectedDiff) {
			implementation->Window->choose_modality(qt::review_modality::review);
			auto* comparison = implementation->Window->comparison_surface();
			if (comparison->current_semantic_index() && !comparison->geometry_pending()
				&& (!comparison->selection() || comparison->selection()->PreferredLine.Side != core::diff_side::right)) {
				(void)comparison->select_conversation_line(core::diff_side::right, 0);
			}
		}
		// A successful retry can retire the selected draft while the server refresh
		// is still reaching this probe. Conversation checks use the accepted server
		// snapshot; they do not need the editor's former draft selection.
		const auto needsDraft = probe.Phase != state_type::phase::awaiting_retry
			&& probe.Phase != state_type::phase::awaiting_native_conversation
			&& probe.Phase != state_type::phase::awaiting_fallback_draft
			&& probe.Phase != state_type::phase::awaiting_fallback_conversation;
		if (implementation->Fixture != nullptr && implementation->Window != nullptr
			&& implementation->WorkbenchHost != nullptr
			&& implementation->ReviewMutationHost != nullptr
			&& needsDraft && !implementation->ReviewDraft.has_value()
			&& implementation->WorkbenchHost->outstanding_requests() == 0U) {
			auto* const view = implementation->Window->files_drawer()->contents_view();
			if (view != nullptr && view->model() != nullptr
				&& view->model()->rowCount() > 0
				&& !view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
			}
			return wait("REVIEW_ROUND_TRIP_DRAFT_STALLED", draftFacts);
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->ReviewMutationHost == nullptr
			|| (needsDraft && !implementation->ReviewDraft.has_value())
			|| !implementation->LastAcceptedHandoff.has_value()
			|| !implementation->LastSelectedDiff.has_value()
			|| implementation->Window->review_editor() == nullptr) {
			return wait("REVIEW_ROUND_TRIP_DRAFT_STALLED", draftFacts);
		}
		auto* const editor     = implementation->Window->review_editor();
		const auto  designated = github_application::implementation::review_mutation_target(
			*implementation->LastAcceptedHandoff);
		const auto clickRightLine = [implementation](const std::uint32_t aLine) {
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || comparison->right_pane() == nullptr) {
				return false;
			}
			auto* const pane  = comparison->right_pane();
			const auto  block = pane->document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid()) {
				return false;
			}
			pane->setTextCursor(QTextCursor{ block });
			pane->ensureCursorVisible();
			const auto point = pane->cursorRect(QTextCursor{ block }).center();
			auto       press = QMouseEvent{
				QEvent::MouseButtonPress,
				QPointF{ point },
				QPointF{ point },
				QPointF{ point },
				Qt::LeftButton,
				Qt::LeftButton,
				Qt::NoModifier,
			};
			auto release = QMouseEvent{
				QEvent::MouseButtonRelease,
				QPointF{ point },
				QPointF{ point },
				QPointF{ point },
				Qt::LeftButton,
				Qt::NoButton,
				Qt::NoModifier,
			};
			QCoreApplication::sendEvent(pane->viewport(), &press);
			QCoreApplication::sendEvent(pane->viewport(), &release);
			return true;
		};

		if (probe.Phase == state_type::phase::awaiting_native_conversation
			|| probe.Phase == state_type::phase::awaiting_fallback_conversation) {
			const bool fallback = probe.Phase == state_type::phase::awaiting_fallback_conversation;
			const auto id       = fallback ? "review-comment:PRRC_composed_2" : "review-comment:PRRC_composed_1";
			const auto index    = implementation->Window->conversation_model()->index_for_id(id);
			const auto evidence = implementation->Fixture->review_mutation_evidence();
			if (!index.isValid() || implementation->Window->conversation_model()->id_for_index(implementation->Window->conversation_view()->currentIndex()) != id
				|| implementation->PendingPublishedConversation) {
				return wait("SEMANTIC_COMMENT_SERVER_REFRESH_STALLED", [&](QTextStream& aStream) {
					aStream << "fallback=" << fallback << " post=" << index.isValid() << " pending_selection=" << implementation->PendingPublishedConversation.has_value()
							<< " native_posts=" << evidence.NativePosts << " file_posts=" << evidence.FilePosts << " stored=" << evidence.StoredComments
							<< " snapshot=" << implementation->WindowRead->accepted_snapshot().has_value();
				});
			}
			if (fallback) {
				const auto  anchor   = implementation->Window->conversation_model()->semantic_anchor(index);
				const auto& snapshot = *implementation->WindowRead->accepted_snapshot();
				const auto  root     = std::ranges::find_if(snapshot.ReviewThreads, [](const auto& aThread) { return aThread.Thread.value() == "PRRT_composed_2"; });
				QTextStream{ stdout } << "SEMANTIC_COMMENT_READBACK anchor=" << anchor.has_value() << " exact=" << (anchor == probe.OffHunkAnchor)
									  << " source=" << implementation->conversation_source().has_value() << " root=" << (root != snapshot.ReviewThreads.end())
									  << " body=" << (index.data(qt::github_conversation_model::body_role).toString() == QStringLiteral("BPR-0571 off-hunk publication.")) << '\n';
				if (evidence.NativePosts != 2U || evidence.FilePosts != 1U || evidence.StoredComments != 2U
					|| evidence.SessionConstructions != 2U || evidence.DurableJournalReads != 3U
					|| anchor != probe.OffHunkAnchor || root == snapshot.ReviewThreads.end()
					|| root->Subject != application::github_review_thread_subject::file || root->Line
					|| index.data(qt::github_conversation_model::body_role).toString() != QStringLiteral("BPR-0571 off-hunk publication.")) {
					return fail();
				}
				QTextStream{ stdout } << "SEMANTIC_COMMENT_COMPOSITION_OK:native-posts=2,file-posts=1,stored=2,journal-reopens=3,session-constructions=2\n";
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			if (evidence.NativePosts != 1U || evidence.FilePosts != 0U || evidence.DurableJournalReads != 2U
				|| evidence.SessionConstructions != 2U || implementation->Window->conversation_model()->semantic_anchor(index)) {
				return fail();
			}
			probe.Evidence.RetryIdempotent = evidence.Submissions == 2U && evidence.StoredComments == 1U && evidence.DuplicateReturned;
			const auto& observed           = *implementation->WindowRead->accepted_snapshot();
			const auto  native             = std::ranges::find_if(observed.ReviewThreads, [](const auto& aThread) { return aThread.Thread.value() == "PRRT_composed_1"; });
			probe.Evidence.ServerAuthority = native != observed.ReviewThreads.end()
				&& native->Subject == application::github_review_thread_subject::line && native->Comments.size() == 1U
				&& native->Comments.front().Body == "BPR-0229 fixture publication.";
			probe.Evidence.TypedQtProjection = index.data(qt::github_conversation_model::body_role).toString() == QStringLiteral("BPR-0229 fixture publication.")
				&& !implementation->Window->conversation_model()->semantic_anchor(index);
			if (!probe.Evidence.RetryIdempotent || !probe.Evidence.ServerAuthority || !probe.Evidence.TypedQtProjection) {
				return fail();
			}
			implementation->Window->choose_modality(qt::review_modality::review);
			auto* const       comparison = implementation->Window->comparison_surface();
			const auto* const left       = comparison->left_pane()->document();
			const auto* const right      = comparison->right_pane()->document();
			// An unchanged line with four unchanged neighbours on each side is
			// outside the synthetic provider's three-line patch context.
			for (int line = 4; line + 4 < std::min(left->blockCount(), right->blockCount()); ++line) {
				bool unchanged = true;
				for (int neighbour = line - 4; neighbour <= line + 4; ++neighbour) {
					unchanged = unchanged && left->findBlockByNumber(neighbour).text() == right->findBlockByNumber(neighbour).text();
				}
				if (!unchanged || right->findBlockByNumber(line).text().isEmpty()) {
					continue;
				}
				if (!clickRightLine(static_cast<std::uint32_t>(line))
					|| !comparison->select_conversation_line(core::diff_side::right, static_cast<std::uint32_t>(line))
					|| !comparison->selection()
					|| comparison->selection()->PreferredLine.Side != core::diff_side::right) {
					return fail();
				}
				probe.OffHunkAnchor = comparison->selection()->PreferredLine;
				break;
			}
			// A refreshed comparison can still be arriving after its conversation.
			// Stay here until the public gesture actually selected an off-hunk line.
			if (!probe.OffHunkAnchor) {
				return wait("REVIEW_ROUND_TRIP_OFF_HUNK_LINE_STALLED", [&](QTextStream& aStream) {
					aStream << "left_blocks=" << left->blockCount() << " right_blocks=" << right->blockCount()
							<< " result=" << present(comparison->current_result() != nullptr)
							<< " geometry_pending=" << comparison->geometry_pending();
				});
			}
			return advance(state_type::phase::awaiting_fallback_draft);
		}
		if (probe.Phase == state_type::phase::awaiting_fallback_draft) {
			auto* const files = implementation->Window->files_drawer()->contents_view();
			if (files && files->model() && files->model()->rowCount() > 0 && !files->currentIndex().isValid()) {
				files->setCurrentIndex(files->model()->index(0, 0));
				return wait("REVIEW_ROUND_TRIP_FALLBACK_DRAFT_STALLED", draftFacts);
			}
			auto* const comparison = implementation->Window->comparison_surface();
			if (probe.OffHunkAnchor && (!comparison->selection() || comparison->selection()->PreferredLine != *probe.OffHunkAnchor)) {
				(void)comparison->select_conversation_line(core::diff_side::right, probe.OffHunkAnchor->Line);
			}
			if (!probe.OffHunkAnchor || !editor->shown_draft() || editor->shown_draft()->Anchor != probe.OffHunkAnchor
				|| !implementation->ReviewDraft || implementation->ReviewDraft->State != core::review_draft_state::drafting) {
				return wait("REVIEW_ROUND_TRIP_FALLBACK_DRAFT_STALLED", draftFacts);
			}
			editor->body_editor()->setPlainText(QStringLiteral("BPR-0571 off-hunk publication."));
			return advance(state_type::phase::awaiting_fallback_body);
		}
		if (probe.Phase == state_type::phase::awaiting_fallback_body) {
			if (implementation->ReviewDraft->Body != "BPR-0571 off-hunk publication." || !editor->publish_action()->isEnabled()) {
				return wait("REVIEW_ROUND_TRIP_FALLBACK_BODY_STALLED", draftFacts);
			}
			// [[BPR-1277]]. An epoch refresh asks again for the selected file
			// while the reviewer writes; the draft, its body and its line stay.
			if (!probe.SameDiffAskedAgain) {
				probe.SameDiffAskedAgain   = true;
				probe.SameDiffBegins       = implementation->SelectedDiffBegins;
				probe.SameDiffPublications = implementation->SelectedDiffPublications;
				implementation->WorkbenchHost->refresh_selected_diff();
				return wait("REVIEW_ROUND_TRIP_SAME_DIFF_STALLED", draftFacts);
			}
			if (implementation->SelectedDiffBegins == probe.SameDiffBegins
				|| implementation->SelectedDiffPublications == probe.SameDiffPublications
				|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
				return wait("REVIEW_ROUND_TRIP_SAME_DIFF_STALLED", draftFacts);
			}
			const auto* const kept = implementation->Window->comparison_surface();
			if (!editor->shown_draft() || editor->shown_draft()->Anchor != probe.OffHunkAnchor
				|| !kept->selection() || kept->selection()->PreferredLine != *probe.OffHunkAnchor) {
				QTextStream{ stdout } << "REVIEW_ROUND_TRIP_SAME_DIFF_DROPPED_DRAFT\n";
				return fail();
			}
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_fallback_conversation);
		}

		// The draft is anchored where the comparison's selection is, and the
		// course needs it on the right side.
		const auto selectionFacts = [&](QTextStream& aStream) {
			const auto* const comparison = implementation->Window->comparison_surface();
			const auto        selection  = comparison == nullptr ? std::optional<core::semantic_selection>{ } : comparison->selection();
			aStream << "comparison=" << present(comparison != nullptr)
					<< " selection=" << present(selection.has_value())
					<< " selection_side="
					<< (!selection.has_value()                                        ? "none"
							: selection->PreferredLine.Side == core::diff_side::right ? "right"
																					  : "left")
					<< " right_pane=" << present(comparison != nullptr && comparison->right_pane() != nullptr)
					<< " shown_draft=" << present(editor->shown_draft().has_value())
					<< " review_draft=" << draftState();
		};

		if (probe.Phase == state_type::phase::awaiting_editor) {
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || !comparison->selection().has_value()) {
				return wait("REVIEW_ROUND_TRIP_EDITOR_SELECTION_STALLED", selectionFacts);
			}
			if (comparison->selection()->PreferredLine.Side
				!= core::diff_side::right) {
				auto* const pane  = comparison->right_pane();
				const auto  block = pane->document()->findBlockByNumber(0);
				const auto  point = pane->cursorRect(QTextCursor{ block }).center();
				auto        press = QMouseEvent{
					QEvent::MouseButtonPress,
					QPointF{ point },
					QPointF{ point },
					QPointF{ point },
					Qt::LeftButton,
					Qt::LeftButton,
					Qt::NoModifier,
				};
				auto release = QMouseEvent{
					QEvent::MouseButtonRelease,
					QPointF{ point },
					QPointF{ point },
					QPointF{ point },
					Qt::LeftButton,
					Qt::NoButton,
					Qt::NoModifier,
				};
				QCoreApplication::sendEvent(pane->viewport(), &press);
				QCoreApplication::sendEvent(pane->viewport(), &release);
				return wait("REVIEW_ROUND_TRIP_EDITOR_SELECTION_STALLED", selectionFacts);
			}
			if (!probe.NativeLineChosen) {
				const auto* left  = comparison->left_pane()->document();
				const auto* right = comparison->right_pane()->document();
				for (int line = 0; line < std::min(left->blockCount(), right->blockCount()); ++line) {
					if (left->findBlockByNumber(line).text() != right->findBlockByNumber(line).text()
						&& !right->findBlockByNumber(line).text().isEmpty()) {
						probe.NativeLineChosen = clickRightLine(static_cast<std::uint32_t>(line));
						break;
					}
				}
				if (!probe.NativeLineChosen) {
					return fail();
				}
			}
			if (!editor->shown_draft() || editor->shown_draft()->Anchor != comparison->selection()->PreferredLine) {
				return wait("REVIEW_ROUND_TRIP_SELECTED_DRAFT_STALLED", draftFacts);
			}
			probe.Evidence.ExactDesignatedTarget
				= implementation->ReviewMutationHost->designated() == designated;
			probe.Evidence.EditorComposed = editor->shown_draft().has_value()
				&& editor->shown_draft()->Anchor
					== comparison->selection()->PreferredLine
				&& editor->shown_draft()->Anchor.Side == core::diff_side::right
				&& editor->shown_draft()->Anchor.Comparison
					== core::comparison_digest(
						implementation->LastSelectedDiff->Key)
				&& editor->target_label()->text().contains(
					QString::number(implementation->LastAcceptedHandoff->Number.value()));
			probe.Evidence.CapabilityFallback
				= !implementation->ReviewMutationHost->capabilities().ResolvesThreads
				&& !implementation->ReviewMutationHost->capabilities()
						.RecordsViewedState
				&& editor->mutation_status_text().contains(
					QStringLiteral("unavailable"));
			if (!probe.Evidence.ExactDesignatedTarget
				|| !probe.Evidence.EditorComposed
				|| !probe.Evidence.CapabilityFallback) {
				return fail();
			}

			probe.DraftIntents       = editor->statistics().DraftIntents;
			probe.PriorDraftRevision = implementation->ReviewDraftRevision;
			editor->body_editor()->setPlainText(
				QStringLiteral("BPR-0229 fixture publication."));
			return advance(state_type::phase::awaiting_edited_body);
		}
		if (probe.Phase == state_type::phase::awaiting_edited_body) {
			if (implementation->ReviewDraft->Body != "BPR-0229 fixture publication." || !editor->publish_action()->isEnabled()) {
				return wait("REVIEW_ROUND_TRIP_EDIT_ACK_STALLED", draftFacts);
			}
			probe.Evidence.SemanticEdit
				= editor->statistics().DraftIntents == probe.DraftIntents + 1U
				&& implementation->ReviewDraft->Body
					== "BPR-0229 fixture publication.";
			if (!probe.Evidence.SemanticEdit) {
				return fail();
			}

			// Stale revision and foreign scope stop at draft admission, before
			// the real mutation host receives any publication request.
			const auto beforeDraftRefusals = implementation->ReviewMutationHost->statistics();
			const auto acceptedRevision    = implementation->ReviewDraftRevision;
			// [[BPR-0822]]. Named nested types, as in the shared draft edit.
			implementation->dispatch_review_draft(core::revisioned_review_draft_command{
				.Command          = core::review_draft_command{ .Id = implementation->ReviewDraft->Id, .Kind = core::review_draft_command_kind::begin_publish },
				.ExpectedRevision = probe.PriorDraftRevision,
				.Scope            = implementation->DraftScope });
			implementation->dispatch_review_draft(core::revisioned_review_draft_command{
				.Command          = core::review_draft_command{ .Id = implementation->ReviewDraft->Id, .Kind = core::review_draft_command_kind::begin_publish },
				.ExpectedRevision = acceptedRevision,
				.Scope            = core::review_draft_scope_id{ implementation->DraftScope.value() + 1U } });
			if (implementation->ReviewMutationHost->statistics() != beforeDraftRefusals
				|| implementation->ReviewDraftRevision != acceptedRevision
				|| implementation->ReviewDraft->State != core::review_draft_state::drafting
				|| implementation->ReviewDraft->Body != "BPR-0229 fixture publication.") {
				return fail();
			}

			auto       moved      = designated;
			const auto secondHead = core::parse_git_commit_oid(
				implementation->Fixture->evidence().HeadTwoOid);
			if (!secondHead.has_value() || *secondHead == designated.ExpectedHead) {
				return fail();
			}
			moved.ExpectedHead = *secondHead;
			implementation->Fixture->report_review_mutation_target(moved);
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_stale_refusal);
		}

		if (probe.Phase == state_type::phase::awaiting_stale_refusal) {
			if (implementation->PendingReviewMutation || implementation->PrivateDraftCommandPending) {
				return wait("REVIEW_ROUND_TRIP_STALE_TERMINAL_STALLED", draftFacts);
			}
			const auto provider = implementation->Fixture->review_mutation_evidence();
			probe.Evidence.StaleHeadRefused
				= implementation->ReviewDraft->State
					== core::review_draft_state::drafting
				&& provider.Submissions == 0U
				&& editor->mutation_status_text().contains(QStringLiteral("head moved"));
			if (!probe.Evidence.StaleHeadRefused) {
				return fail();
			}
			const auto* const comparison
				= implementation->Window->comparison_surface();
			if (comparison == nullptr || !comparison->selection().has_value()
				|| implementation->ReviewMutationDecisionGate == nullptr) {
				return fail();
			}
			probe.PublishingSelection = comparison->selection();
			probe.PublishingDraft     = implementation->ReviewDraft;
			implementation->Fixture->report_review_mutation_target(designated);
			implementation->ReviewMutationDecisionGate->hold();
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_lost_effect);
		}

		if (probe.Phase == state_type::phase::awaiting_lost_effect) {
			const auto lost = implementation->Fixture->review_mutation_evidence();
			if (!lost.LostResult || lost.StoredComments != 1U
				|| implementation->ReviewMutationDecisionGate == nullptr
				|| implementation->ReviewMutationDecisionGate->pending() == 0U) {
				// The provider stores the comment and loses its answer, and the
				// held decision gate keeps the terminal back.
				return wait("REVIEW_ROUND_TRIP_LOST_EFFECT_STALLED", [&](QTextStream& aStream) {
					const auto* const gate = implementation->ReviewMutationDecisionGate.get();
					aStream << "lost_result=" << (lost.LostResult ? "true" : "false")
							<< " stored_comments=" << static_cast<qulonglong>(lost.StoredComments)
							<< " expected_stored_comments=1"
							<< " submissions=" << static_cast<qulonglong>(lost.Submissions)
							<< " decision_gate=" << present(gate != nullptr)
							<< " gate_pending=" << static_cast<qulonglong>(gate == nullptr ? 0U : gate->pending())
							<< " review_draft=" << draftState()
							<< " pending_mutation=" << present(implementation->PendingReviewMutation.has_value())
							<< " mutation_outstanding=" << static_cast<qulonglong>(implementation->ReviewMutationHost->outstanding_requests());
				});
			}
			if (!probe.PublishingSelection.has_value()
				|| !implementation->PendingReviewMutation.has_value()
				|| !implementation->ReviewDraft.has_value()
				|| implementation->ReviewDraft->State
					!= core::review_draft_state::publishing) {
				return fail();
			}
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || comparison->right_pane() == nullptr) {
				return fail();
			}
			for (auto line = 0; line < comparison->right_pane()->document()->blockCount();
				 ++line) {
				if (static_cast<std::uint32_t>(line)
					== probe.PublishingSelection->PreferredLine.Line) {
					continue;
				}
				if (!clickRightLine(static_cast<std::uint32_t>(line))) {
					continue;
				}
				if (comparison->selection().has_value()
					&& comparison->selection()->PreferredLine.Side
						== core::diff_side::right
					&& comparison->selection()->PreferredLine
						!= probe.PublishingSelection->PreferredLine) {
					probe.DeferredSelection = comparison->selection();
					break;
				}
			}
			const auto intermediateDeferred
				= probe.DeferredSelection.has_value()
				&& implementation->DeferredReviewSelection.has_value()
				&& *implementation->DeferredReviewSelection
					== probe.DeferredSelection
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& editor->shown_draft() == implementation->ReviewDraft;
			if (!intermediateDeferred
				|| !clickRightLine(probe.PublishingSelection->PreferredLine.Line)) {
				return fail();
			}
			probe.Evidence.SelectionDeferred = true;
			probe.Evidence.LatestSelectionWins
				= comparison->selection() == probe.PublishingSelection
				&& implementation->DeferredReviewSelection.has_value()
				&& *implementation->DeferredReviewSelection
					== probe.PublishingSelection
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& implementation->PendingReviewMutation.has_value()
				&& probe.PublishingDraft.has_value()
				&& implementation->ReviewDraft->Id == probe.PublishingDraft->Id
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& implementation->ReviewDraft->State
					== core::review_draft_state::publishing;
			if (!probe.Evidence.LatestSelectionWins) {
				return fail();
			}
			implementation->ReviewMutationDecisionGate->release();
			return advance(state_type::phase::awaiting_deferred_terminal);
		}

		if (implementation->ReviewMutationHost->outstanding_requests() != 0U
			|| implementation->PendingReviewMutation || implementation->PrivateDraftCommandPending) {
			// The released or retried publication reaches its terminal.
			return wait(probe.Phase == state_type::phase::awaiting_retry
							? "REVIEW_ROUND_TRIP_RETRY_STALLED"
							: "REVIEW_ROUND_TRIP_DEFERRED_TERMINAL_STALLED",
						[&](QTextStream& aStream) {
							const auto provider   = implementation->Fixture->review_mutation_evidence();
							const auto statistics = implementation->ReviewMutationHost->statistics();
							aStream << "phase=" << phaseName(probe.Phase)
									<< " mutation_outstanding=" << static_cast<qulonglong>(implementation->ReviewMutationHost->outstanding_requests())
									<< " review_draft=" << draftState()
									<< " pending_mutation=" << present(implementation->PendingReviewMutation.has_value())
									<< " effects_queued=" << static_cast<qulonglong>(statistics.EffectsQueued)
									<< " terminals_published=" << static_cast<qulonglong>(statistics.TerminalsPublished)
									<< " submissions=" << static_cast<qulonglong>(provider.Submissions)
									<< " stored_comments=" << static_cast<qulonglong>(provider.StoredComments);
						});
		}

		if (probe.Phase == state_type::phase::awaiting_deferred_terminal) {
			const auto        lost = implementation->Fixture->review_mutation_evidence();
			const auto* const comparison
				= implementation->Window->comparison_surface();
			auto retryIsolation = false;
			if (probe.PublishingSelection.has_value()
				&& probe.DeferredSelection.has_value()
				&& implementation->RetryableReviewDraft.has_value()
				&& implementation->SelectedDiffPublication.has_value()) {
				auto foreign     = implementation->RetryableReviewDraft->Authority;
				foreign.Snapshot = core::provider_snapshot_id{
					foreign.Snapshot.value() + 1U
				};
				retryIsolation
					= !github_application::implementation::exact_retryable_review_draft(
						  *implementation->RetryableReviewDraft,
						  *probe.DeferredSelection,
						  implementation->SelectedDiffPublication->Request.WorkbenchAuthority)
					&& !github_application::implementation::exact_retryable_review_draft(
						*implementation->RetryableReviewDraft,
						*probe.PublishingSelection,
						foreign);
			}
			probe.Evidence.LostResultRefreshed = lost.LostResult
				&& lost.StoredComments == 1U
				&& implementation->ReviewProjection
					== implementation->Fixture->review_comments();
			probe.Evidence.PendingCleared
				= !implementation->PendingReviewMutation.has_value();
			probe.Evidence.DeferredDraftExact
				= comparison != nullptr && probe.PublishingDraft.has_value()
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& comparison->selection() == probe.PublishingSelection
				&& implementation->ReviewDraft->State
					== core::review_draft_state::publishing
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& implementation->ReviewDraft->Id == probe.PublishingDraft->Id
				&& implementation->ReviewDraft->Body == probe.PublishingDraft->Body
				&& editor->shown_draft() == implementation->ReviewDraft
				&& implementation->RetryableReviewDraft.has_value()
				&& implementation->ReviewDraft
					== std::optional{ implementation->RetryableReviewDraft->Draft }
				&& retryIsolation;
			probe.Evidence.StaleTerminalWithheld
				= editor->mutation_status_text().contains(
					  QStringLiteral("unavailable"))
				&& !editor->mutation_status_text().contains(
					QStringLiteral("retry is safe"))
				&& !editor->mutation_status_text().contains(
					QStringLiteral("cancelled"));
			if (!probe.Evidence.LostResultRefreshed
				|| !probe.Evidence.PendingCleared
				|| !probe.Evidence.DeferredDraftExact
				|| !probe.Evidence.StaleTerminalWithheld
				|| !probe.PublishingSelection.has_value()) {
				return fail();
			}
			if (comparison == nullptr || !comparison->selection().has_value()
				|| comparison->selection()->PreferredLine
					!= probe.PublishingSelection->PreferredLine
				|| !implementation->ReviewDraft.has_value()
				|| !implementation->RetryableReviewDraft.has_value()
				|| *implementation->ReviewDraft
					!= implementation->RetryableReviewDraft->Draft) {
				return fail();
			}
			editor->body_editor()->setPlainText(
				QStringLiteral("BPR-0229 fixture publication."));
			if (implementation->ReviewDraft->Body
				!= "BPR-0229 fixture publication.") {
				return fail();
			}
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_retry);
		}

		// The receipt initiates a server refresh. That refresh can retire the
		// editor's publication state before this observer runs, so the result
		// is checked on the selected accepted conversation below.
		const auto provider = implementation->Fixture->review_mutation_evidence();
		if (provider.Submissions != 2U || provider.StoredComments != 1U || !provider.DuplicateReturned
			|| provider.NativePosts != 1U || provider.FilePosts != 0U) {
			return fail();
		}

		return advance(state_type::phase::awaiting_native_conversation);
	}

	auto detail::github_application_fixture_access::dormant_live_provider_selection_evidence_for(
		const github_application& aApplication)
		-> detail::dormant_live_provider_selection_evidence
	{
		auto  evidence       = detail::dormant_live_provider_selection_evidence{ };
		auto* implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return evidence;
		}
		auto       handoff  = clone_without_smart_git(*implementation->LastAcceptedHandoff);
		auto       snapshot = application::github_pull_request_snapshot{ };
		const auto base     = core::parse_git_commit_oid(designated_review_base_oid);
		const auto head     = core::parse_git_commit_oid(designated_review_head_oid);
		const auto baseRef  = application::parse_git_base_branch_ref(
			designated_review_base_full_ref);
		if (!base.has_value() || !head.has_value() || !baseRef.has_value()) {
			return evidence;
		}

		handoff.Hostname = application::github_canonical_hostname{
			designated_review_hostname
		};
		handoff.ServerContract = application::github_server_contract::github_com_2022_11_28;
		handoff.Repository     = core::repository_id{ designated_review_repository_id };
		handoff.Locator        = {
			.Owner = std::string{ designated_review_owner },
			.Name  = std::string{ designated_review_name },
		};
		handoff.PullRequest  = core::pull_request_id{ designated_review_pull_request_id };
		handoff.Number       = core::pull_request_number{ designated_review_number };
		handoff.BaseRef      = *baseRef;
		handoff.Base         = *base;
		handoff.RecordedBase = *base;
		handoff.Head         = *head;

		snapshot.Profile = handoff.Profile;
		snapshot.Host    = handoff.Host;
		snapshot.Account = handoff.Account;
		snapshot.Viewer  = {
			.Actor = application::github_actor_id{ "MDQ6VXNlcjIy" },
			.Login = "fixture-reviewer",
		};
		snapshot.Repository.Repository   = handoff.Repository;
		snapshot.Repository.Node         = application::github_node_id{ std::string{ designated_review_repository_node } };
		snapshot.Repository.Owner        = std::string{ designated_review_owner };
		snapshot.Repository.Name         = std::string{ designated_review_name };
		snapshot.PullRequest.PullRequest = handoff.PullRequest;
		snapshot.PullRequest.Repository  = handoff.Repository;
		snapshot.PullRequest.Number      = handoff.Number;
		snapshot.PullRequest.Node        = application::github_node_id{ std::string{ designated_review_pull_request_node } };
		snapshot.PullRequest.State       = application::github_pull_request_state::open;
		snapshot.PullRequest.BaseOid     = application::commit_oid{ std::string{ designated_review_base_oid } };
		snapshot.PullRequest.HeadOid     = application::commit_oid{ std::string{ designated_review_head_oid } };
		snapshot.PullRequest.BaseRef     = std::string{ designated_review_base_ref };
		snapshot.PullRequest.HeadRef     = std::string{ designated_review_head_ref };

		const auto exactSelection = select_designated_review_mutation(
			handoff, snapshot);
		evidence.ExactTupleAccepted = exactSelection.has_value();
		auto rejects                = [&handoff, &snapshot, &evidence](auto aMutate) {
			auto changedHandoff  = clone_without_smart_git(handoff);
			auto changedSnapshot = snapshot;
			aMutate(changedHandoff, changedSnapshot);
			evidence.MismatchedTuplesRejected += static_cast<std::size_t>(
				!select_designated_review_mutation(
					 changedHandoff, changedSnapshot)
					 .has_value());
		};
		rejects([](auto& aHandoff, auto&) { aHandoff.Hostname = application::github_canonical_hostname{ "ghe.example.invalid" }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Locator.Owner = "Other"; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Locator.Name = "Other"; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Repository = core::repository_id{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.PullRequest = core::pull_request_id{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Number = core::pull_request_number{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Base = aHandoff.Head; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Head = aHandoff.Base; });
		rejects([](auto& aHandoff, auto&) { aHandoff.BaseRef = application::git_base_branch_ref{ }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Profile = core::host_profile_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Host = core::git_host_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Account = core::github_account_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Viewer.Actor = application::github_actor_id{ }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Viewer.Login.clear(); });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Repository = core::repository_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Node = application::github_node_id{ "other" }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Owner = "Other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Name = "Other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.PullRequest = core::pull_request_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Repository = core::repository_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Number = core::pull_request_number{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Node = application::github_node_id{ "other" }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.State = application::github_pull_request_state::closed; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.BaseOid = application::commit_oid{ std::string{ designated_review_head_oid } }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.HeadOid = application::commit_oid{ std::string{ designated_review_base_oid } }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.BaseRef = "other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.HeadRef = "other"; });
		evidence.ExpectedMismatchRejections = 28U;
		evidence.FixtureProviderSelected
			= implementation->FixtureReviewMutationHostComposed;
		evidence.LiveProviderAbsent = true;
		return evidence;
	}
#endif
}
