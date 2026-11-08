#include "github_application/fixture_course_support.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/appearance_application_host.hpp"
	#include "bpr/application/appearance_runtime_ports.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/application/review_ledger_port.hpp"
	#include "bpr/application/window_review_restoration_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/defect_workflow.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/profile_window.hpp"
	#include "bpr/core/review_ledger.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "bpr/qt/conversation_cards_view.hpp"
	#include "bpr/qt/defect_card_view.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/epoch_comparison_control.hpp"
	#include "bpr/qt/github_conversation_model.hpp"
	#include "bpr/qt/github_conversation_panel.hpp"
	#include "bpr/qt/review_draft_editor.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/defect_identity.hpp"
	#include "github_application/profile_runtime.hpp"
	#include "github_application/review_mutation_selection.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAction>
	#include <QAnyStringView>
	#include <QComboBox>
	#include <QDesktopServices>
	#include <QLabel>
	#include <QListView>
	#include <QMenu>
	#include <QModelIndex>
	#include <QObject>
	#include <QPlainTextEdit>
	#include <QScrollArea>
	#include <QString>
	#include <QTextStream>
	#include <QTreeView>
	#include <QVariant>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <functional>
	#include <map>
	#include <span>
	#include <string_view>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::review_mutation_provider_marker(
		const github_application& aApplication) noexcept -> std::string_view
	{
		const auto* implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->Fixture != nullptr
			? std::string_view{ "fixture" }
			: std::string_view{ "none" };
	}

	auto detail::github_application_fixture_access::exercise_defect_composition(
		github_application&                    aApplication,
		const detail::defect_composition_probe aCourse)
		-> detail::defect_composition_probe_result
	{
		using result               = detail::defect_composition_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->DefectCompositionProbe.has_value()) {
			implementation->DefectCompositionProbe.emplace();
			implementation->DefectCompositionProbe->Course = aCourse;
		}
		auto& probe = *implementation->DefectCompositionProbe;
		if (probe.Phase
			== defect_composition_probe_state::phase::failed) {
			return result::failed;
		}
		if (probe.Phase
			== defect_composition_probe_state::phase::passed) {
			return result::passed;
		}
		using state_type = defect_composition_probe_state;
		const auto fail  = [&probe] {
			probe.Phase = defect_composition_probe_state::phase::failed;
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
		const auto phaseName = [](const state_type::phase aPhase) {
			switch (aPhase) {
			case state_type::phase::awaiting_ready:
				return "awaiting_ready";
			case state_type::phase::awaiting_diff:
				return "awaiting_diff";
			case state_type::phase::awaiting_classified:
				return "awaiting_classified";
			case state_type::phase::awaiting_verified:
				return "awaiting_verified";
			case state_type::phase::awaiting_reclassified:
				return "awaiting_reclassified";
			case state_type::phase::awaiting_reverse_right:
				return "awaiting_reverse_right";
			case state_type::phase::awaiting_epoch_change:
				return "awaiting_epoch_change";
			case state_type::phase::awaiting_shared_ready:
				return "awaiting_shared_ready";
			case state_type::phase::passed:
				return "passed";
			case state_type::phase::failed:
				return "failed";
			}
			return "unknown";
		};
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| implementation->ReviewMutationHost == nullptr
			|| implementation->Window->defect_card() == nullptr) {
			// The course drives the composed window, ledger and mutation host.
			return wait("DEFECT_COMPOSITION_STALLED", [&](QTextStream& aStream) {
				aStream << "phase=" << phaseName(probe.Phase)
						<< " fixture=" << present(implementation->Fixture != nullptr)
						<< " window=" << present(implementation->Window != nullptr)
						<< " host=" << present(implementation->Host != nullptr)
						<< " ledger_host=" << present(implementation->ReviewLedgerHost != nullptr)
						<< " mutation_host=" << present(implementation->ReviewMutationHost != nullptr)
						<< " defect_card=" << present(implementation->Window != nullptr && implementation->Window->defect_card() != nullptr);
			});
		}
		const auto modality = enter_course_modality(aApplication, probe.Modality);
		if (modality == detail::course_modality_result::pending) {
			return result::pending;
		}
		if (modality == detail::course_modality_result::failed) {
			return fail();
		}
		// [[BPR-1211]]. A defect command is kept once its write is durable.
		if (probe.Phase == state_type::phase::awaiting_verified || probe.Phase == state_type::phase::awaiting_reclassified) {
			return defect_composition_after_verify(aApplication);
		}
		auto* const window = implementation->Window.get();
		if (!probe.CommentsRequested) {
			probe.CommentsRequested = true;
			if (window->conversations_drawer()->state() != core::workbench_drawer_state::expanded) {
				window->conversations_drawer_toggle_action()->trigger();
				return result::pending;
			}
		}
		if (probe.Phase == state_type::phase::awaiting_ready && !window->conversation_panel()->isVisible()) {
			return wait("DEFECT_COMMENTS_STALLED", [&](QTextStream& aStream) { aStream << course_modality_facts(aApplication); });
		}
		const auto firstIdentity        = std::string{ "review-thread:PRRT_fixture_1" };
		const auto secondIdentity       = std::string{ "review-thread:PRRT_fixture_3" };
		const auto fileIdentity         = std::string{ "review-thread:PRRT_fixture_file" };
		const auto firstDefectIdentity  = defect_identity(firstIdentity);
		const auto secondDefectIdentity = defect_identity(secondIdentity);
		const auto findDefect           = [implementation](const local_defect_identity aIdentity)
			-> const application::durable_defect_record* {
			const auto found = std::ranges::find_if(
				implementation->DurableDefects,
				[aIdentity](const application::durable_defect_record& aDefect) {
					return aDefect.Conversation.value() == aIdentity.Conversation
						&& aDefect.History.Defect.Id.value() == aIdentity.Defect;
				});
			return found == implementation->DurableDefects.end() ? nullptr : &*found;
		};
		const auto anchorMatchesThread = [implementation](
											 const application::durable_defect_record* const aDefect,
											 const std::string_view                          aConversation) {
			const auto* const thread = implementation->accepted_thread(aConversation);
			if (aDefect == nullptr || thread == nullptr || thread->Outdated
				|| !thread->Line.has_value() || *thread->Line == 0U) {
				return false;
			}
			const auto side = thread->DiffSide == application::github_diff_side::left
				? std::optional{ core::diff_side::left }
				: thread->DiffSide == application::github_diff_side::right
				? std::optional{ core::diff_side::right }
				: std::nullopt;
			return side.has_value()
				&& aDefect->History.OriginalAnchor.Side == *side
				&& aDefect->History.OriginalAnchor.Line == *thread->Line - 1U;
		};

		if (aCourse == detail::defect_composition_probe::reopen) {
			const auto* const first  = findDefect(firstDefectIdentity);
			const auto* const second = findDefect(secondDefectIdentity);
			if (first == nullptr || second == nullptr) {
				// The seed course's two defects come back from the ledger.
				return wait("DEFECT_REOPEN_STALLED", [&](QTextStream& aStream) {
					const auto statistics = implementation->ReviewLedgerHost->statistics();
					aStream << "durable_defects=" << static_cast<qulonglong>(implementation->DurableDefects.size())
							<< " first=" << present(first != nullptr)
							<< " second=" << present(second != nullptr)
							<< " ledger_private_loads=" << static_cast<qulonglong>(statistics.PrivateLoads)
							<< " ledger_private_writes=" << static_cast<qulonglong>(statistics.PrivateWrites)
							<< " shown_defects=" << static_cast<qulonglong>(window->defect_count());
				});
			}
			probe.Evidence.ExactThreads = anchorMatchesThread(first, firstIdentity)
				&& anchorMatchesThread(second, secondIdentity);
			probe.Evidence.LocalClassification
				= first->History.Defect.Severity == core::defect_severity::critical
				&& first->History.Defect.Category == core::defect_category::security;
			probe.Evidence.CorrelatedEvidence
				= second->History.Defect.State == core::defect_state::verified
				&& second->History.Availability
					== core::defect_anchor_availability::outdated;
			probe.Evidence.ExplicitVerification
				= first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State == core::defect_state::verified;
			probe.Evidence.DurableHistory
				= second->History.Availability
					== core::defect_anchor_availability::outdated
				&& second->History.Defect.Anchor == second->History.OriginalAnchor;
			const auto before = window->defect_card()->shown_defect();
			if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			}
			probe.Evidence.MultiDefectNavigation = window->defect_count() == 2U
				&& before.has_value()
				&& window->defect_card()->shown_defect().has_value()
				&& window->defect_card()->shown_defect()->Id != before->Id;
			probe.Evidence.SharedActionSeparate
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			const auto statistics            = implementation->ReviewLedgerHost->statistics();
			probe.Evidence.RestartRestored   = statistics.PrivateLoads >= 1U;
			probe.Evidence.ZeroReopenWrites  = statistics.PrivateWrites == 0U;
			probe.Evidence.VisibleProjection = window->defect_count_label()->text().contains(
				QStringLiteral("/2"));
			if (probe.Evidence != detail::defect_composition_probe_evidence{ .ExactThreads = true, .LocalClassification = true, .CorrelatedEvidence = true, .ExplicitVerification = true, .DurableHistory = true, .MultiDefectNavigation = true, .SharedActionSeparate = true, .RestartRestored = true, .ZeroReopenWrites = true, .VisibleProjection = true }) {
				return fail();
			}
			probe.Phase = defect_composition_probe_state::phase::passed;
			return result::passed;
		}

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_shared_ready) {
			auto* const editor = window->review_editor();
			if (editor == nullptr || !editor->shown_draft().has_value()) {
				// The shared follow-up needs a draft shown in the editor.
				return wait("DEFECT_SHARED_READY_STALLED", [&](QTextStream& aStream) {
					aStream << "review_editor=" << present(editor != nullptr)
							<< " shown_draft=" << present(editor != nullptr && editor->shown_draft().has_value())
							<< " review_draft=" << present(implementation->ReviewDraft.has_value())
							<< " submissions=" << static_cast<qulonglong>(implementation->Fixture->review_mutation_evidence().Submissions);
				});
			}
			probe.Evidence.SharedActionSeparate
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			if (!probe.Evidence.SharedActionSeparate) {
				return fail();
			}
			probe.Evidence.RestartRestored  = false;
			probe.Evidence.ZeroReopenWrites = false;
			probe.Phase                     = defect_composition_probe_state::phase::passed;
			editor->body_editor()->setPlainText(
				QStringLiteral("Explicit shared follow-up after local classification."));
			editor->publish_action()->trigger();
			return result::passed;
		}

		auto* const model = window->conversation_model();
		auto* const view  = window->conversation_view();
		if ((probe.Phase == state_type::phase::awaiting_ready || probe.Phase == state_type::phase::awaiting_diff)
			&& (!model->index_for_id(firstIdentity).isValid()
				|| !model->index_for_id(secondIdentity).isValid()
				|| !model->index_for_id(fileIdentity).isValid())) {
			// The three fixture conversations reach the conversation model.
			return wait("DEFECT_CONVERSATIONS_STALLED", [&](QTextStream& aStream) {
				aStream << "phase=" << phaseName(probe.Phase)
						<< " first_conversation=" << present(model->index_for_id(firstIdentity).isValid())
						<< " second_conversation=" << present(model->index_for_id(secondIdentity).isValid())
						<< " file_conversation=" << present(model->index_for_id(fileIdentity).isValid())
						<< " conversation_rows=" << model->rowCount();
			});
		}
		const auto selectConversation = [window, model, view](
											const std::string_view aIdentity) {
			window->conversation_panel()->set_anchor_line(std::nullopt);
			const auto index = model->index_for_id(aIdentity);
			if (!index.isValid()) {
				return false;
			}
			view->setCurrentIndex(index);
			return true;
		};

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_ready) {
			if (!implementation->DurableDefects.empty()) {
				return fail();
			}
			auto* const files = window->files_drawer()->contents_view();
			if (implementation->WorkbenchHost == nullptr
				|| implementation->WorkbenchHost->outstanding_requests() != 0U
				|| implementation->WorkbenchHost->actionable().empty()
				|| files == nullptr || files->model() == nullptr
				|| files->model()->rowCount() == 0) {
				// The workbench host observes the pull request's files and
				// the drawer lists them.
				return wait("DEFECT_READY_STALLED", [&](QTextStream& aStream) {
					aStream << "workbench_host=" << present(implementation->WorkbenchHost != nullptr)
							<< " actionable=" << static_cast<qulonglong>(implementation->WorkbenchHost == nullptr ? 0U : implementation->WorkbenchHost->actionable().size())
							<< " files_view=" << present(files != nullptr)
							<< " file_rows=" << (files == nullptr || files->model() == nullptr ? 0 : files->model()->rowCount());
				});
			}
			files->setCurrentIndex(files->model()->index(0, 0));
			return advance(state_type::phase::awaiting_diff);
		}

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_diff) {
			if (!implementation->SelectedDiffPublication.has_value()
				|| window->comparison_surface()->current_result() == nullptr) {
				// The file gesture's diff is computed through the Git and cache
				// actors and published to the comparison.
				return wait("DEFECT_SELECTED_DIFF_STALLED", [&](QTextStream& aStream) {
					const auto work = diff_work_evidence(aApplication);
					aStream << "selected_diff_publication=" << present(implementation->SelectedDiffPublication.has_value())
							<< " comparison_result=" << present(window->comparison_surface()->current_result() != nullptr)
							<< " selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
							<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
							<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
							<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
							<< " diff_pending=" << truth(work.Pending)
							<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs);
				});
			}
			if (!selectConversation(firstIdentity)
				|| !window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			if (!selectConversation(secondIdentity)
				|| !window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			if (!selectConversation(fileIdentity)
				|| window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			// [[BPR-1211]]. Each classification is kept once its write is durable.
			return advance(state_type::phase::awaiting_classified);
		}

		if (probe.Phase == state_type::phase::awaiting_classified) {
			const auto* const first  = findDefect(firstDefectIdentity);
			const auto* const second = findDefect(secondDefectIdentity);
			if (first == nullptr || second == nullptr || !implementation->ReviewLedgerHost->is_quiescent()) {
				return wait("DEFECT_CLASSIFICATION_STALLED", [&](QTextStream& aStream) {
					aStream << "durable_defects=" << static_cast<qulonglong>(implementation->DurableDefects.size())
							<< " ledger_outstanding=" << static_cast<qulonglong>(implementation->ReviewLedgerHost->outstanding());
				});
			}
			probe.Evidence.ExactThreads = anchorMatchesThread(first, firstIdentity)
				&& anchorMatchesThread(second, secondIdentity);
			probe.Evidence.LocalClassification
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			probe.Evidence.CorrelatedEvidence = first != nullptr && second != nullptr
				&& first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State
					== core::defect_state::needs_verification;
			probe.Evidence.DurableHistory = second != nullptr
				&& second->History.Availability
					== core::defect_anchor_availability::available
				&& second->History.Defect.Anchor == second->History.OriginalAnchor;
			probe.Evidence.VisibleProjection = window->defect_count() == 2U
				&& window->defect_count_label()->text().contains(QStringLiteral("/2"));
			if (!probe.Evidence.ExactThreads || !probe.Evidence.LocalClassification
				|| !probe.Evidence.CorrelatedEvidence
				|| !probe.Evidence.DurableHistory
				|| !probe.Evidence.VisibleProjection) {
				return fail();
			}
			// [[BPR-1208]]. The same accepted defect is reachable on its own
			// thread, while the private tab contains no thread controls.
			auto        menu   = std::unique_ptr<QMenu>{ window->conversation_panel()->cards()->thread_menu(secondIdentity) };
			const auto& cards  = window->conversation_panel()->cards()->cards();
			const auto  card   = std::ranges::find(cards, secondIdentity, &qt::thread_card::Id);
			auto*       verify = menu ? menu->findChild<QAction*>(QStringLiteral("defectVerify")) : nullptr;
			if (!verify || !verify->isEnabled() || card == cards.end() || card->DefectBadge.isEmpty() || !window->conversation_panel()->cards()->isVisible()
				|| !window->comments_tab_scroll()->isAncestorOf(window->defect_count_label())
				|| window->findings_tab_scroll()->isAncestorOf(window->defect_card())) {
				return fail();
			}
			QTextStream{ stdout } << "DEFECT_THREAD_CONTROLS_OK\n";
			probe.BeforeComparison = core::comparison_digest(
				implementation->SelectedDiffPublication->Key);
			auto* const control  = window->epoch_selectors();
			const auto  leftRow  = control->left_selector()->currentIndex();
			const auto  rightRow = control->right_selector()->currentIndex();
			if (leftRow < 0 || rightRow < 0 || leftRow == rightRow) {
				return fail();
			}
			probe.ReverseLeftRow              = rightRow;
			probe.ReverseRightRow             = leftRow;
			probe.RefuseCorrelatedPersistence = true;
			control->left_selector()->setCurrentIndex(rightRow);
			return advance(state_type::phase::awaiting_reverse_right);
		}

		// What reversing the comparison depends on: the epoch core's status
		// and both selectors against the rows the course asked for.
		const auto epochFacts = [&](QTextStream& aStream) {
			auto* const control = window->epoch_selectors();
			aStream << "epoch_host=" << present(implementation->EpochHost != nullptr)
					<< " epoch_status=" << (implementation->EpochHost == nullptr ? -1 : static_cast<int>(implementation->EpochHost->state().Status))
					<< " resolving=" << truth(implementation->EpochHost != nullptr && implementation->EpochHost->state().Status == core::epoch_status::resolving)
					<< " left_row=" << control->left_selector()->currentIndex()
					<< " expected_left_row=" << probe.ReverseLeftRow
					<< " right_row=" << control->right_selector()->currentIndex()
					<< " expected_right_row=" << probe.ReverseRightRow;
		};

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_reverse_right) {
			// [[BPR-0655]]. The control blanks the left selector the moment
			// the core starts resolving the anchor it dispatched, so reading a
			// blank selector as "not applied" and dispatching again on the
			// next 10 ms tick superseded every resolution slower than a tick:
			// each completion arrived one generation stale and the course
			// never left `resolving`. Wait while the core resolves; only a
			// settled core showing another left anchor is asked again.
			auto* const control = window->epoch_selectors();
			if (implementation->EpochHost == nullptr
				|| implementation->EpochHost->state().Status
					== core::epoch_status::resolving) {
				return wait("DEFECT_REVERSE_RIGHT_STALLED", epochFacts);
			}
			if (control->left_selector()->currentIndex() != probe.ReverseLeftRow) {
				control->left_selector()->setCurrentIndex(probe.ReverseLeftRow);
				return wait("DEFECT_REVERSE_RIGHT_STALLED", epochFacts);
			}
			control->right_selector()->setCurrentIndex(probe.ReverseRightRow);
			return advance(state_type::phase::awaiting_epoch_change);
		}

		// The reversed comparison is published and reaches the surface.
		const auto epochChangeFacts = [&](QTextStream& aStream) {
			const auto& shown = window->comparison_surface()->current_result();
			aStream << "selected_diff_publication=" << present(implementation->SelectedDiffPublication.has_value())
					<< " comparison_changed="
					<< truth(implementation->SelectedDiffPublication.has_value()
							 && core::comparison_digest(implementation->SelectedDiffPublication->Key) != probe.BeforeComparison)
					<< " comparison_result=" << present(shown != nullptr)
					<< " result_matches_publication="
					<< truth(shown != nullptr && implementation->SelectedDiffPublication.has_value()
							 && shown->Alignment.Digest == core::comparison_digest(implementation->SelectedDiffPublication->Key))
					<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
					<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
					<< " refused_persistence_attempts=" << static_cast<qulonglong>(probe.RefusedPersistenceAttempts)
					<< ' ';
			epochFacts(aStream);
		};
		if (!implementation->SelectedDiffPublication.has_value()
			|| core::comparison_digest(implementation->SelectedDiffPublication->Key)
				== probe.BeforeComparison) {
			return wait("DEFECT_EPOCH_CHANGE_STALLED", epochChangeFacts);
		}
		const auto& reversedResult
			= window->comparison_surface()->current_result();
		if (reversedResult == nullptr
			|| reversedResult->Alignment.Digest
				!= core::comparison_digest(
					implementation->SelectedDiffPublication->Key)) {
			return wait("DEFECT_EPOCH_CHANGE_STALLED", epochChangeFacts);
		}
		// BPR-1205 deliberately hides later threads on this older pair. The
		// same refusal is stronger here: there is no selectable wrong-revision
		// thread, no code anchor, and no classification command to dispatch.
		if (model->index_for_id(firstIdentity).isValid() || model->index_for_id(secondIdentity).isValid()) {
			return fail();
		}
		const auto defectCountBeforeRefusal = implementation->DurableDefects.size();
		if (window->comparison_surface()->conversation_anchor().has_value()
			|| window->classify_defect_action()->isEnabled()
			|| defectCountBeforeRefusal != 2U) {
			return fail();
		}
		window->classify_defect_action()->trigger();
		if (implementation->DurableDefects.size() != defectCountBeforeRefusal) {
			return fail();
		}
		const auto* first  = findDefect(firstDefectIdentity);
		const auto* second = findDefect(secondDefectIdentity);
		if (first == nullptr || second == nullptr
			|| first->History.Defect.State
				!= core::defect_state::open
			|| second->History.Defect.State
				!= core::defect_state::needs_verification
			|| first->History.Availability
				!= core::defect_anchor_availability::outdated
			|| second->History.Availability
				!= core::defect_anchor_availability::outdated) {
			return fail();
		}
		probe.Evidence.PersistFailureFailsClosed
			= probe.RefusedPersistenceAttempts >= 2U;
		probe.RefuseCorrelatedPersistence = false;
		if (!probe.Evidence.PersistFailureFailsClosed) {
			return fail();
		}
		while (window->defect_card()->shown_defect().has_value()
			   && window->defect_card()->shown_defect()->Id != second->History.Defect.Id) {
			if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			} else if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else {
				return fail();
			}
		}
		window->defect_card()->verify_action()->trigger();
		return advance(state_type::phase::awaiting_verified);
	}

	auto detail::github_application_fixture_access::defect_composition_after_verify(
		github_application& aApplication) -> detail::defect_composition_probe_result
	{
		using result               = detail::defect_composition_probe_result;
		using state_type           = defect_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		auto&       probe          = *implementation->DefectCompositionProbe;
		auto* const window         = implementation->Window.get();
		const auto  fail           = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto findDefect = [implementation](const std::string_view aThread) -> const application::durable_defect_record* {
			const auto identity = defect_identity(aThread);
			const auto found    = std::ranges::find_if(implementation->DurableDefects, [identity](const application::durable_defect_record& aDefect) {
				return aDefect.Conversation.value() == identity.Conversation && aDefect.History.Defect.Id.value() == identity.Defect;
			});
			return found == implementation->DurableDefects.end() ? nullptr : &*found;
		};
		const auto* first  = findDefect("review-thread:PRRT_fixture_1");
		const auto* second = findDefect("review-thread:PRRT_fixture_3");
		const auto  facts  = [&](QTextStream& aStream) {
			aStream << "phase=" << static_cast<int>(probe.Phase)
					<< " durable_defects=" << static_cast<qulonglong>(implementation->DurableDefects.size())
					<< " ledger_outstanding=" << static_cast<qulonglong>(implementation->ReviewLedgerHost->outstanding());
		};
		if (!implementation->ReviewLedgerHost->is_quiescent()) {
			return keep_waiting(aApplication, probe.Wait, "DEFECT_COMMAND_STALLED", facts) ? result::pending : fail();
		}
		auto* const card = window->defect_card();
		if (probe.Phase == state_type::phase::awaiting_verified) {
			probe.Evidence.ExplicitVerification = first != nullptr && second != nullptr
				&& first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State == core::defect_state::verified;
			if (!probe.Evidence.ExplicitVerification) {
				return fail();
			}
			while (card->shown_defect().has_value()
				   && card->shown_defect()->Id != first->History.Defect.Id) {
				if (window->previous_defect_action()->isEnabled()) {
					window->previous_defect_action()->trigger();
				} else if (window->next_defect_action()->isEnabled()) {
					window->next_defect_action()->trigger();
				} else {
					return fail();
				}
			}
			card->severity_selector()->setCurrentIndex(
				card->severity_selector()->findData(
					static_cast<int>(core::defect_severity::critical)));
			card->category_selector()->setCurrentIndex(
				card->category_selector()->findData(
					static_cast<int>(core::defect_category::security)));
			card->reclassify_action()->trigger();
			return advance(state_type::phase::awaiting_reclassified);
		}
		probe.Evidence.DurableHistory = probe.Evidence.DurableHistory
			&& first != nullptr
			&& first->History.Defect.Severity == core::defect_severity::critical
			&& first->History.Defect.Category == core::defect_category::security;
		const auto before = card->shown_defect()->Id;
		if (window->next_defect_action()->isEnabled()) {
			window->next_defect_action()->trigger();
		} else {
			window->previous_defect_action()->trigger();
		}
		probe.Evidence.MultiDefectNavigation
			= card->shown_defect().has_value() && card->shown_defect()->Id != before;
		// [[BPR-1238]]. The shared follow-up is a new root comment, which only a
		// right-side line takes ([[BPR-0587]]). The comparison keeps the first
		// thread's left-side line selected, so the reviewer picks a right-side
		// line, as they would before writing one.
		auto rightLineSelected = false;
		for (auto line = std::uint32_t{ 0U }; line < 64U && !rightLineSelected; ++line) {
			rightLineSelected = window->comparison_surface()->select_conversation_line(core::diff_side::right, line);
		}
		if (!rightLineSelected) {
			return fail();
		}
		return advance(state_type::phase::awaiting_shared_ready);
	}

	auto detail::github_application_fixture_access::defect_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::defect_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->DefectCompositionProbe.has_value()
			? implementation->DefectCompositionProbe->Evidence
			: detail::defect_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::select_appearance_mode(
		github_application&         aApplication,
		const core::appearance_mode aMode) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		implementation->AppearanceHost->select(aMode);
		return implementation->AppearanceHost->settings().Mode == aMode;
	}

	auto detail::github_application_fixture_access::review_ledger_receipt_present(
		github_application& aApplication) -> bool
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
			&& implementation->ReviewLedgerWasPresentOnEntry;
	}

	auto detail::github_application_fixture_access::review_ledger_settled(
		github_application& aApplication, const std::optional<core::bpr_window_context> aContext) -> bool
	{
		const auto* implementation = aApplication.Implementation.get();
		if (aContext) {
			const auto found = aApplication.Sessions.find(*aContext);
			implementation   = found == aApplication.Sessions.end() ? nullptr : found->second.get();
		}
		return implementation != nullptr && !implementation->Closing && implementation->ReviewLedgerHost != nullptr
			&& implementation->ReviewLedgerHost->available().has_value()
			&& implementation->ReviewLedgerHost->is_quiescent();
	}

	auto detail::github_application_fixture_access::hold_restoration_terminals(github_application& aApplication) -> std::function<void()>
	{
		auto profile = aApplication.Profile;
		profile->RestorationDecisionGate.hold();
		return [profile] { profile->RestorationDecisionGate.release(); };
	}

	auto detail::github_application_fixture_access::hold_window_close_callbacks(github_application& aApplication) -> std::function<void()>
	{
		auto profile = aApplication.Profile;
		profile->WindowCloseDecisionGate.hold();
		return [profile] { profile->WindowCloseDecisionGate.release(); };
	}

	auto detail::github_application_fixture_access::held_restoration_and_close_callbacks(const github_application& aApplication) -> std::array<std::size_t, 2>
	{
		return { aApplication.Profile->RestorationDecisionGate.pending(), aApplication.Profile->WindowCloseDecisionGate.pending() };
	}

	auto detail::github_application_fixture_access::restoration_durable(const github_application& aApplication) -> bool
	{
		return aApplication.Profile->ReviewRestoration && aApplication.Profile->ReviewRestoration->durable();
	}

	auto detail::github_application_fixture_access::hold_review_ledger_terminals(github_application& aApplication, const core::bpr_window_context aContext, const bool aHold) -> bool
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found == aApplication.Sessions.end() || found->second->Closing || !found->second->ReviewLedgerHost || !found->second->ReviewLedgerDecisionGate) {
			return false;
		}
		auto& gate = *found->second->ReviewLedgerDecisionGate;
		if (aHold) {
			gate.hold();
		} else {
			gate.release();
		}
		return true;
	}

	auto detail::github_application_fixture_access::release_review_ledger_terminal(github_application& aApplication, const core::bpr_window_context aContext) -> bool
	{
		const auto found = aApplication.Sessions.find(aContext);
		return found != aApplication.Sessions.end() && !found->second->Closing && found->second->ReviewLedgerDecisionGate
			&& found->second->ReviewLedgerDecisionGate->release_next();
	}

	auto detail::github_application_fixture_access::held_review_ledger_terminals(const github_application& aApplication, const core::bpr_window_context aContext) -> std::size_t
	{
		const auto found = aApplication.Sessions.find(aContext);
		return found != aApplication.Sessions.end() && !found->second->Closing && found->second->ReviewLedgerDecisionGate
			? found->second->ReviewLedgerDecisionGate->pending()
			: 0U;
	}
#endif
}
