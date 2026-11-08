#include "github_application/fixture_course_support.hpp"

#include <QScopeGuard>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/diff_runtime_ports.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/application/review_ledger_port.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/agent_context_supplement.hpp"
	#include "bpr/core/defect_workflow.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_transition.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/review_ledger.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/defect_identity.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application/workbench_composition.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractButton>
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAction>
	#include <QAnyStringView>
	#include <QByteArray>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QEvent>
	#include <QFlags>
	#include <QKeySequence>
	#include <QListView>
	#include <QModelIndex>
	#include <QObject>
	#include <QString>
	#include <QTextStream>
	#include <QToolButton>
	#include <QWidget>
	#include <array>
	#include <cstddef>
	#include <expected>
	#include <functional>
	#include <initializer_list>
	#include <qpa/qwindowsysteminterface.h>
	#include <span>
	#include <string_view>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		[[nodiscard]] auto review_ledger_probe_note(
			const core::file_identity          aFile,
			const core::file_comparison_digest aComparison)
			-> application::local_note_record
		{
			return {
				.Id         = application::local_note_id{ 228U },
				.Attachment = application::local_note_attachment{
					.File       = aFile,
					.Comparison = aComparison,
				},
				.Note = { .Text = "BPR-0228 host-only composition probe." },
			};
		}

		[[nodiscard]] constexpr auto review_ledger_probe_thread_identity() noexcept
			-> std::string_view
		{
			return "review-thread:PRRT_fixture_1";
		}

		[[nodiscard]] auto review_ledger_probe_thread_matches_file(
			const core::file_identity                       aFile,
			const application::github_review_thread_record& aThread) -> bool
		{
			const auto acceptedIdentity
				= "review-thread:" + std::string{ aThread.Thread.value() };
			return acceptedIdentity == review_ledger_probe_thread_identity()
				&& aThread.Subject
				== application::github_review_thread_subject::line
				&& !aThread.Outdated
				&& (aThread.DiffSide == application::github_diff_side::left
					|| aThread.DiffSide == application::github_diff_side::right)
				&& aThread.Line.has_value() && *aThread.Line != 0U
				&& !aThread.Path.empty()
				&& core::file_identity_of(aThread.Path) == aFile;
		}

		[[nodiscard]] auto review_ledger_probe_defect(
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread)
			-> std::optional<application::durable_defect_record>
		{
			if (!review_ledger_probe_thread_matches_file(aFile, aThread)) {
				return std::nullopt;
			}
			const auto identity = defect_identity(review_ledger_probe_thread_identity());
			const auto side     = aThread.DiffSide == application::github_diff_side::left
				? core::diff_side::left
				: core::diff_side::right;
			const auto anchor   = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = side,
				.Line       = *aThread.Line - 1U,
			};
			return application::durable_defect_record{
				.File         = aFile,
				.Conversation = core::review_conversation_id{ identity.Conversation },
				.History      = {
					.Defect = {
						.Id       = core::defect_id{ identity.Defect },
						.Anchor   = anchor,
						.Severity = core::defect_severity::major,
						.Category = core::defect_category::correctness,
						.State    = core::defect_state::needs_verification,
					},
					.OriginalAnchor = anchor,
					.Availability   = core::defect_anchor_availability::available,
				},
			};
		}

		[[nodiscard]] auto review_ledger_probe_defect_matches_thread(
			const application::durable_defect_record&       aDefect,
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread) -> bool
		{
			if (!review_ledger_probe_thread_matches_file(aFile, aThread)) {
				return false;
			}
			const auto identity = defect_identity(review_ledger_probe_thread_identity());
			const auto side     = aThread.DiffSide == application::github_diff_side::left
				? core::diff_side::left
				: core::diff_side::right;
			const auto anchor   = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = side,
				.Line       = *aThread.Line - 1U,
			};
			return aDefect.File == aFile
				&& aDefect.Conversation.value() == identity.Conversation
				&& aDefect.History.Defect.Id.value() == identity.Defect
				&& aDefect.History.Defect.Anchor == anchor
				&& aDefect.History.OriginalAnchor == anchor;
		}

		[[nodiscard]] auto review_ledger_probe_defect_match_is_non_vacuous(
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread) -> bool
		{
			const auto expected = review_ledger_probe_defect(
				aFile, aComparison, aThread);
			if (!expected.has_value()) {
				return false;
			}
			auto       wrongLine = *expected;
			const auto counterexampleLine
				= wrongLine.History.Defect.Anchor.Line == 1U ? 2U : 1U;
			wrongLine.History.Defect.Anchor.Line  = counterexampleLine;
			wrongLine.History.OriginalAnchor.Line = counterexampleLine;
			auto wrongSide                        = *expected;
			wrongSide.History.Defect.Anchor.Side  = aThread.DiffSide
					== application::github_diff_side::left
				? core::diff_side::right
				: core::diff_side::left;
			wrongSide.History.OriginalAnchor.Side
				= wrongSide.History.Defect.Anchor.Side;
			return review_ledger_probe_defect_matches_thread(
					   *expected, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongLine, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongSide, aFile, aComparison, aThread);
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::exercise_review_ledger_composition(
		github_application&                   aApplication,
		const review_ledger_composition_probe aCourse)
		-> detail::review_ledger_composition_probe_result
	{
		using result               = detail::review_ledger_composition_probe_result;
		using state_type           = review_ledger_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return result::failed;
		}

		if (!implementation->ReviewLedgerCompositionProbe.has_value()) {
			implementation->ReviewLedgerCompositionProbe.emplace();
			implementation->ReviewLedgerCompositionProbe->Course = aCourse;
		}
		auto& probe = *implementation->ReviewLedgerCompositionProbe;
		if (probe.Course != aCourse
			|| probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		auto* const window = implementation->Window.get();
		auto* const host   = implementation->WorkbenchHost.get();
		auto* const ledger = implementation->ReviewLedgerHost.get();
		const auto  fail   = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		// [[BPR-1331]]. What the review bar's private progress button says.
		const auto progressMark = [window] {
			const auto* const button = window->findChild<QWidget*>(QStringLiteral("workbenchReviewControlsButton"));
			return button != nullptr ? button->accessibleName().toStdString() : std::string{ "absent" };
		};
		// What the ledger's own counters say, for the phases that wait on it.
		const auto ledgerFacts = [&](QTextStream& aStream) {
			const auto statistics = ledger->statistics();
			aStream << "ledger_commands=" << static_cast<qulonglong>(statistics.Commands)
					<< " ledger_persistence_requests=" << static_cast<qulonglong>(statistics.PersistenceRequests)
					<< " ledger_completions=" << static_cast<qulonglong>(statistics.Completions)
					<< " ledger_store_failures=" << static_cast<qulonglong>(statistics.StoreFailures)
					<< " ledger_material_observations=" << static_cast<qulonglong>(statistics.MaterialObservations);
		};

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& handoff  = *implementation->LastAcceptedHandoff;
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| host->actionable().empty()) {
				// The accepted pull request's files reach the workbench host as
				// an observation, and the ledger's gestures need one of them.
				return wait("REVIEW_LEDGER_OBSERVATION_STALLED", [&](QTextStream& aStream) {
					aStream << "snapshot=" << present(snapshot.has_value())
							<< " snapshot_files=" << static_cast<qulonglong>(snapshot.has_value() ? snapshot->Files.size() : 0U)
							<< " window_read_quiescent=" << truth(implementation->WindowRead->is_quiescent())
							<< " actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< ' ';
					ledgerFacts(aStream);
				});
			}
			probe.Evidence.ExactAuthority
				= implementation->ReviewLedgerAuthority
				== std::optional{ workbench_authority(handoff) };
			probe.Evidence.HostComposed = ledger != nullptr;
			probe.Evidence.PublicPathAvailable
				= window->github_status_text() == "Pull request ready.";
			probe.Evidence.HostOnlyProbe = true;
			probe.Evidence.ReadStateObserved
				= ledger != nullptr && ledger->statistics().ConversationLoads >= 1U;
			probe.InitialStatistics = ledger->statistics();
			const auto modality     = enter_course_modality(aApplication, probe.Overview);
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			auto* const view = window->files_drawer()->contents_view();
			if (!probe.Evidence.ExactAuthority || !probe.Evidence.HostComposed
				|| !probe.Evidence.PublicPathAvailable || view == nullptr
				|| view->model() == nullptr || view->model()->rowCount() == 0) {
				return fail();
			}
			view->setCurrentIndex(view->model()->index(0, 0));
			return advance(state_type::phase::awaiting_hidden_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_hidden_diff) {
			const auto* surface = window->comparison_surface();
			if (!surface->current_result() || !implementation->SelectedDiffPublication || !ledger->is_quiescent()) {
				return wait("REVIEW_LEDGER_HIDDEN_DIFF_STALLED", ledgerFacts);
			}
			// Loading a selected file underneath Overview must not claim it was viewed.
			probe.SeenOnEntry = ledger->retained().Read == core::file_read_state::seen;
			if (!window->showing_overview() || surface->isVisibleTo(window)
				|| ledger->statistics().Commands != 0U
				|| (aCourse != review_ledger_composition_probe::unavailable
					&& ledger->retained().Read != core::file_read_state::unread
					&& !(aCourse == review_ledger_composition_probe::reopen && probe.SeenOnEntry))) {
				return fail();
			}
			return advance(state_type::phase::awaiting_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_diff) {
			const auto modality = enter_course_modality(aApplication, probe.Review);
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			const auto* const comparison = window->comparison_surface();
			if (comparison == nullptr || comparison->current_result() == nullptr
				|| !implementation->LastSelectedDiff.has_value() || !ledger->is_quiescent()) {
				// The file gesture's diff is computed through the Git and cache
				// actors and published to the comparison.
				return wait("REVIEW_LEDGER_SELECTED_DIFF_STALLED", [&](QTextStream& aStream) {
					const auto work = diff_work_evidence(aApplication);
					aStream << "file_selections=" << static_cast<qulonglong>(window->statistics().FileSelections)
							<< " selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
							<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
							<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
							<< " last_selected_diff=" << present(implementation->LastSelectedDiff.has_value())
							<< " comparison_result=" << present(comparison != nullptr && comparison->current_result() != nullptr)
							<< " host_selected=" << present(host->selected().has_value())
							<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
							<< " diff_pending=" << truth(work.Pending)
							<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs)
							<< " ledger_outstanding=" << static_cast<qulonglong>(ledger->outstanding());
				});
			}
			const auto& selected   = *implementation->LastSelectedDiff;
			const auto  material   = core::file_material(selected.Key);
			const auto  retained   = ledger->retained();
			const auto  statistics = ledger->statistics();
			probe.File             = selected.Request.File.File;
			probe.Material         = material;
			probe.Comparison       = core::comparison_digest(selected.Key);
			probe.Evidence.RealFileGesture
				= window->statistics().FileSelections > 0U;
			probe.Evidence.RealSelectedDiff
				= selected.Request.File.File.valid()
				&& selected.Key == comparison->current_result()->Alignment.Key;
			probe.Evidence.ExactMaterial = material.valid()
				&& (aCourse == review_ledger_composition_probe::unavailable
					|| (retained.File == selected.Request.File.File
						&& retained.Material == material));
			probe.Evidence.ExpectedPreGesturePersistence
				= aCourse == review_ledger_composition_probe::seed
				? statistics.Commands == 1U
					&& statistics.PersistenceRequests == 1U
					&& statistics.Completions == 1U
				: aCourse == review_ledger_composition_probe::reopen
				? statistics.Commands == (probe.SeenOnEntry ? 0U : 1U)
					&& statistics.PersistenceRequests == (probe.SeenOnEntry ? 0U : 2U)
					&& statistics.Completions == (probe.SeenOnEntry ? 0U : 2U)
				: statistics.Commands == 0U
					&& statistics.PersistenceRequests == 0U
					&& statistics.Completions == 0U
					&& statistics.StoreFailures > 0U;
			// [[BPR-0824]]. Not `ReceiptLoadHits`: this window's queued refresh_shared_ledger re-reads its own carry-over commit, racing this tick.
			probe.Evidence.ReceiptLoadedOnEntry
				= statistics.MaterialObservations == 1U
				&& statistics.MaterialReceiptHits == 1U;
			implementation->ReviewLedgerWasPresentOnEntry
				= probe.Evidence.ReceiptLoadedOnEntry;

			if (!probe.Evidence.RealFileGesture
				|| !probe.Evidence.RealSelectedDiff
				|| !probe.Evidence.ExactMaterial
				|| !probe.Evidence.ExpectedPreGesturePersistence) {
				return fail();
			}

			if (aCourse == review_ledger_composition_probe::unavailable) {
				probe.Evidence.UnavailableVisible
					= window->status_text().contains(
						  QStringLiteral("Private review progress is unavailable."))
					&& !window->seen_action()->isEnabled()
					&& !window->read_action()->isEnabled()
					&& !window->accept_action()->isEnabled()
					&& !window->exclude_action()->isEnabled()
					&& statistics.StoreFailures > 0U;
				probe.Evidence.DurableReceipt          = true;
				probe.Evidence.PrivateRecords          = true;
				probe.Evidence.GestureSequence         = true;
				probe.Evidence.ActionabilityProjection = true;
				if (!probe.Evidence.UnavailableVisible) {
					return fail();
				}
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			// [[BPR-1506]]. The ordinary file gesture made Seen durable before
			// any receipt action. A late receipt for another file, material or
			// request generation must not submit a command, even if unread.
			// [[BPR-1701]]. Seen reached is checked and offers Mark unseen.
			const auto durable = ledger->cached_receipt(probe.File);
			if (retained.Read != core::file_read_state::seen || retained.Accepted || retained.Excluded
				|| !window->seen_action()->isChecked() || progressMark() != "Private progress: seen"
				|| !durable || !*durable || **durable != retained) {
				return fail();
			}
			auto unread = retained;
			unread.Read = core::file_read_state::unread;
			implementation->AutoSeenPresentedRequest.reset();
			auto stale                       = selected;
			stale.Request.Context.Generation = core::request_generation{ selected.Request.Context.Generation.value() + 1U };
			implementation->mark_presented_file_seen(stale, unread);
			stale                   = selected;
			stale.Request.File.File = core::file_identity_of("not-the-presented-file.cpp");
			implementation->mark_presented_file_seen(stale, unread);
			auto wrongMaterial     = unread;
			wrongMaterial.Material = core::file_material_id{ };
			implementation->mark_presented_file_seen(selected, wrongMaterial);
			implementation->observe_selected_material(application::diff_publication{ .Context = *comparison->current_context() });
			implementation->mark_presented_file_seen(selected, retained);
			if (ledger->outstanding() != 0U || ledger->statistics().Commands != statistics.Commands) {
				return fail();
			}

			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			if (defectThread == nullptr
				|| !review_ledger_probe_thread_matches_file(
					probe.File, *defectThread)) {
				return fail();
			}

			if (aCourse == review_ledger_composition_probe::seed) {
				// [[BPR-1211]]. Each gesture is decided and made durable off the
				// decision lane; the course gives each one a quiescent ledger,
				// as a reviewer waiting for the button to change would.
				probe.GesturesBefore = ledger->statistics();
				probe.Gesture        = 0U;
				return advance(state_type::phase::awaiting_gestures);
			} else {
				const auto receipt      = ledger->retained();
				const auto privateState = ledger->private_observation();
				probe.Evidence.GestureSequence
					= ledger->statistics().Commands == (probe.SeenOnEntry ? 0U : 1U);
				probe.Evidence.DurableReceipt
					= probe.Evidence.ReceiptLoadedOnEntry
					&& receipt.File == probe.File
					&& receipt.Material == probe.Material
					&& receipt.Read == core::file_read_state::seen
					&& !receipt.Accepted && !receipt.Excluded;
				probe.Evidence.ProgressMark = progressMark();
				const auto priorHead        = implementation->Fixture != nullptr
					? core::parse_git_commit_oid(
						  implementation->Fixture->evidence().HeadOneOid)
					: std::optional<core::git_commit_oid>{ };
				if (privateState.has_value() && privateState->Notes.size() == 1U
					&& privateState->Notes.front().Attachment.has_value()) {
					const auto storedComparison
						= privateState->Notes.front().Attachment->Comparison;
					auto expectedNote     = review_ledger_probe_note(probe.File, storedComparison);
					expectedNote.Revision = application::local_note_revision{ 1U };
					auto defect           = review_ledger_probe_defect(
						probe.File, storedComparison, *defectThread);
					// The reuse course advances to head two. The conversation-backed
					// defect from head one must survive, but its old comparison can no
					// longer prove an exact placement.
					if (defect.has_value()) {
						defect->History.Availability
							= core::defect_anchor_availability::outdated;
					}
					probe.Evidence.PrivateRecords
						= defect.has_value()
						&& review_ledger_probe_defect_match_is_non_vacuous(
							  probe.File, storedComparison, *defectThread)
						&& storedComparison.valid()
						&& storedComparison != probe.Comparison
						&& privateState->Notes
							== std::vector{ expectedNote }
						&& priorHead.has_value()
						&& privateState->LastReviewedHead == priorHead
						&& privateState->Defects
							== std::vector{ *defect }
						&& review_ledger_probe_defect_matches_thread(
							  privateState->Defects.front(),
							  probe.File,
							  storedComparison,
							  *defectThread)
						&& ledger->statistics().PrivateWrites == (probe.SeenOnEntry ? 0U : 1U);
				}
				implementation->ReviewLedgerRoundTripPassed
					= probe.Evidence.DurableReceipt;
			}
			if (!probe.Evidence.DurableReceipt
				|| !probe.Evidence.PrivateRecords) {
				return fail();
			}
			return advance(state_type::phase::awaiting_actionability);
		}

		if (probe.Phase == state_type::phase::awaiting_gestures) {
			if (!ledger->is_quiescent()) {
				return wait("REVIEW_LEDGER_GESTURE_STALLED", [&](QTextStream& aStream) {
					aStream << "gesture=" << static_cast<qulonglong>(probe.Gesture)
							<< " ledger_outstanding=" << static_cast<qulonglong>(ledger->outstanding()) << ' ';
					ledgerFacts(aStream);
				});
			}
			const auto actions = std::array{
				window->read_action(),
				window->accept_action(),
				window->accept_action(),
				window->exclude_action(),
				window->exclude_action(),
				window->accept_action(),
			};
			if (probe.Gesture < actions.size()) {
				auto* const action = actions[probe.Gesture];
				if (action == nullptr || !action->isEnabled()) {
					return fail();
				}
				++probe.Gesture;
				if (action == window->accept_action()) {
					// [[BPR-1507]]. Exercise the normal one-click presenter through
					// the same exact command, durable host and published receipt.
					auto* const button = window->findChild<QToolButton*>(QStringLiteral("workbenchAcceptFileButton"));
					if (button == nullptr || !button->isVisible() || !button->isEnabled()
						|| button->defaultAction() != action || button->menu() != nullptr) {
						return fail();
					}
					// [[BPR-1515]]. Accept uses the actual composed window's key;
					// the inverse stays a deliberate one-click gesture.
					// [[BPR-1531]]. The key is Option-A (Alt+A). On macOS Cocoa
					// offers it to the shortcut map as the A key with Option and
					// the text it types, "å", before the text input system.
					window->activateWindow();
					button->setFocus();
					QCoreApplication::processEvents();
					const auto acceptKey = [window] {
	#if defined(Q_OS_MACOS)
						const auto typed = QStringLiteral("å");
						if (QWindowSystemInterface::handleShortcutEvent(window->windowHandle(), 0UL, Qt::Key_A, Qt::AltModifier, 0U, 0U, 0U, typed)) {
							QCoreApplication::processEvents();
							return;
						}
	#else
						const auto typed = QString{ };
	#endif
						for (const auto type : { QEvent::KeyPress, QEvent::KeyRelease }) {
							QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(
								window->windowHandle(), type, Qt::Key_A, Qt::AltModifier, typed);
							QCoreApplication::processEvents();
						}
					};
					if (ledger->retained().Accepted) {
						const auto commands = ledger->statistics().Commands;
						acceptKey();
						acceptKey();
						if (ledger->statistics().Commands != commands || !ledger->retained().Accepted || !action->shortcut().isEmpty()) {
							return fail();
						}
						QTextStream{ stdout } << "REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK\n";
						button->click();
					} else {
						if (action->shortcut() != QKeySequence{ QStringLiteral("Alt+A") }) {
							return fail();
						}
						acceptKey();
					}
				} else {
					action->trigger();
				}
				return result::pending;
			}
			const auto  after   = ledger->statistics();
			const auto  receipt = ledger->retained();
			const auto& before  = probe.GesturesBefore;
			probe.Evidence.GestureSequence
				= after.Commands == before.Commands + actions.size()
				&& after.PersistenceRequests
					== before.PersistenceRequests + actions.size()
				&& after.Completions == before.Completions + actions.size()
				&& after.StoreFailures == before.StoreFailures
				&& receipt.File == probe.File
				&& receipt.Material == probe.Material
				&& receipt.Read == core::file_read_state::read
				&& receipt.Accepted && !receipt.Excluded;
			probe.Evidence.ProgressMark    = progressMark();
			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			if (!probe.Evidence.GestureSequence || defectThread == nullptr) {
				return fail();
			}
			const auto note = review_ledger_probe_note(
				probe.File, probe.Comparison);
			probe.PrivateDefect = review_ledger_probe_defect(
				probe.File, probe.Comparison, *defectThread);
			if (!probe.PrivateDefect.has_value()
				|| !review_ledger_probe_defect_match_is_non_vacuous(
					probe.File, probe.Comparison, *defectThread)) {
				return fail();
			}
			const auto answered = [&probe](const bool aAccepted) {
				++probe.PrivateAnswers;
				probe.PrivateAccepted = probe.PrivateAccepted && aAccepted;
			};
			ledger->persist(note, std::nullopt, implementation->ledger_answer([&probe, answered](const std::expected<application::local_note_record, application::local_note_failure>& aStored) {
				if (aStored.has_value()) {
					probe.AcceptedNote = *aStored;
				}
				answered(aStored.has_value());
			}));
			ledger->persist_last_reviewed_head(implementation->LastAcceptedHandoff->Head, implementation->ledger_answer([answered](const std::expected<void, application::ledger_store_failure>& aStored) {
				answered(aStored.has_value());
			}));
			ledger->persist(*probe.PrivateDefect, implementation->ledger_answer([answered](const std::expected<void, application::ledger_store_failure>& aStored) {
				answered(aStored.has_value());
			}));
			return advance(state_type::phase::awaiting_private_records);
		}

		if (probe.Phase == state_type::phase::awaiting_private_records) {
			if (probe.PrivateAnswers < 3U || !ledger->is_quiescent()) {
				return wait("REVIEW_LEDGER_PRIVATE_RECORDS_STALLED", [&](QTextStream& aStream) {
					aStream << "private_answers=" << static_cast<qulonglong>(probe.PrivateAnswers) << ' ';
					ledgerFacts(aStream);
				});
			}
			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			const auto& head              = implementation->LastAcceptedHandoff->Head;
			const auto  privateState      = ledger->private_observation();
			probe.Evidence.PrivateRecords = probe.PrivateAccepted && probe.AcceptedNote.has_value()
				&& defectThread != nullptr && privateState.has_value()
				&& privateState->Notes == std::vector{ *probe.AcceptedNote }
				&& privateState->LastReviewedHead == std::optional{ head }
				&& privateState->Defects == std::vector{ *probe.PrivateDefect }
				&& review_ledger_probe_defect_matches_thread(
												privateState->Defects.front(),
												probe.File,
												probe.Comparison,
												*defectThread);
			probe.Evidence.DurableReceipt               = true;
			implementation->ReviewLedgerRoundTripPassed = true;
			if (!probe.Evidence.PrivateRecords) {
				return fail();
			}
			return advance(state_type::phase::awaiting_actionability);
		}

		if (probe.Phase == state_type::phase::awaiting_actionability) {
			if (host->outstanding_requests() != 0U || !ledger->is_quiescent()) {
				// The gestures' persistence and the actionability it projects
				// reach the host as requests; the tail prints how many remain.
				return wait("REVIEW_LEDGER_ACTIONABILITY_STALLED", [&](QTextStream& aStream) {
					aStream << "actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " host_selected=" << present(host->selected().has_value())
							<< ' ';
					ledgerFacts(aStream);
				});
			}
			const auto&       actionable        = host->actionable();
			const auto* const comparison        = window->comparison_surface();
			const auto        selectionRetained = host->selected() == probe.File
				&& implementation->LastSelectedDiff.has_value()
				&& comparison != nullptr && comparison->current_result() != nullptr
				&& comparison->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key;
			if (aCourse == review_ledger_composition_probe::seed) {
				if (!actionable.empty() || !selectionRetained) {
					return fail();
				}
				auto materialBytes   = probe.Material.bytes();
				materialBytes.back() = static_cast<std::uint8_t>(
					materialBytes.back() ^ 0x01U);
				probe.ReopenedMaterial = core::file_material_id{ materialBytes };
				ledger->observe_material(core::file_material_observation{
					.File     = probe.File,
					.Material = probe.ReopenedMaterial,
				});
				return advance(state_type::phase::awaiting_material_actionability);
			}
			probe.Evidence.ActionabilityProjection = actionable.size() == 1U
				&& actionable.front() == probe.File && selectionRetained;
			if (!probe.Evidence.ActionabilityProjection) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		if (probe.Phase == state_type::phase::awaiting_material_actionability) {
			if (host->outstanding_requests() != 0U || !ledger->is_quiescent()) {
				// The new material's observation makes the file actionable
				// again through the host.
				return wait("REVIEW_LEDGER_MATERIAL_ACTIONABILITY_STALLED", [&](QTextStream& aStream) {
					const auto& receipt = ledger->retained();
					aStream << "actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " host_selected=" << present(host->selected().has_value())
							<< " receipt_material_reopened=" << truth(receipt.Material == probe.ReopenedMaterial)
							<< ' ';
					ledgerFacts(aStream);
				});
			}
			const auto& actionable = host->actionable();
			const auto& receipt    = ledger->retained();
			const auto* comparison = window->comparison_surface();
			probe.Evidence.ActionabilityProjection
				= actionable.size() == 1U && actionable.front() == probe.File
				&& host->selected() == probe.File
				&& implementation->LastSelectedDiff.has_value()
				&& comparison != nullptr && comparison->current_result() != nullptr
				&& comparison->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key
				&& receipt.File == probe.File
				&& receipt.Material == probe.ReopenedMaterial
				&& receipt.Read == core::file_read_state::unread
				&& !receipt.Accepted && !receipt.Excluded;
			if (!probe.Evidence.ActionabilityProjection) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::review_ledger_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_ledger_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->ReviewLedgerCompositionProbe.has_value()
			? implementation->ReviewLedgerCompositionProbe->Evidence
			: detail::review_ledger_composition_probe_evidence{ };
	}
#endif
}
