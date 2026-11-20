#include "github_application/window_composition.hpp"

#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/home_view_state.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/remembered_connection_host.hpp"
#include "bpr/application/review_queue_notification.hpp"
#include "bpr/application/staged_review_publication.hpp"
#include "bpr/application/view_filter_memory_host.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/github_transition.hpp"
#include "bpr/core/home_ignore.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/review_queue_attention.hpp"
#include "bpr/core/view_filter_memory.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/author_description_editor.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/motion_observer.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/source_find_bar.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_application/queue_notifications.hpp"
#include "github_application/session_settings.hpp"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QObject>
#include <QStyleHints>
#include <QTextStream>
#include <QWindow>

#include <algorithm>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <variant>

#if !defined(BPR_BUILD_TESTING)
	#include "review_submission_execution_authority.hpp"
#endif
#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/sqlite/agent_setup_store.hpp"
	#include "bpr/adapters/workspace/clone_folder_scan.hpp"
	#include "bpr/application/agent_finding_delivery.hpp"
	#include "bpr/application/managed_clone.hpp"
	#include "bpr/core/agent_entry.hpp"
	#include "bpr/core/agent_wrapper.hpp"
	#include "bpr/core/review_composition.hpp"
	#include "bpr/qt/agent_setup_settings.hpp"
	#include "bpr/qt/claude_wrapper_panel.hpp"
	#include "bpr/qt/private_review_panel.hpp"
	#include <QDir>
	#if !defined(BPR_BUILD_TESTING)
		#include <QCoreApplication>
	#endif
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/review_submission_application_host.hpp"
	#include "bpr/application/workbench_preferences_host.hpp"
#endif

namespace bpr::app {
	github_application::implementation::implementation(std::shared_ptr<github_profile_runtime> aProfile, core::bpr_window_context aContext)
		: Profile{ std::move(aProfile) }
	{
		const auto& aConfig    = Profile->Config;
		const auto& aAdmission = Profile->Admission;
		DraftWindow            = aContext;
		if (aContext.Window.value() == std::numeric_limits<std::uint64_t>::max()) {
			throw std::runtime_error{ "window identity exhausted" };
		}
		WindowSession = core::application_session_id{ aContext.Window.value() + 1U };
		SetupOwner    = next_setup_owner();
		Profile->Observers.emplace(DraftWindow, github_profile_runtime::window_observer{ .Lifetime = WindowLifetime });
		// BPR-0191: the application shows RFC-BPR-0005's shell. Until this
		// line changed, the shell existed only in its own tests and every
		// reviewer saw the RFC-BPR-0002 window instead.
		Window = std::make_unique<qt::workbench_window>(
			[this](core::workbench_interaction_intent aIntent) {
#if defined(BPR_BUILD_TESTING)
				if (DisableWorkbenchSelectionWire
					&& std::holds_alternative<
						core::select_workbench_file_intent>(aIntent)) {
					return;
				}
#endif
				dispatch_workbench_intent(std::move(aIntent));
			},
			[this](core::navigate_diff_change_intent aIntent) {
				dispatch_workbench_change(aIntent);
			},
			[this](const core::workbench_drawer aDrawer,
				   const std::uint32_t          aWidth) {
				dispatch_workbench_width(aDrawer, aWidth);
			},
			ProfileOptions,
			[this](application::github_intent aIntent) { dispatch_intent(std::move(aIntent)); },
			nullptr,
			aConfig.DifCatalog);
		Window->set_comment_state_dispatch([this](const qt::comment_state_gesture& aGesture) { dispatch_comment_state(aGesture); });
		Window->set_header_source_dispatch([this] { dispatch_header_source(); });
		Window->set_review_navigation_dispatch([this](const auto aCommand) { request_review_navigation(aCommand); });
		Window->set_close_review_dispatch([this] { close_review(); });
		Window->tour_editor()->set_dispatch([this](core::tour_command aCommand) {
			if (!Closing && local_tour_shown()) {
				DraftDesktop->tour_command(aCommand);
				return;
			}
			if (!Closing && TourHost && current(DraftWindow)) {
				aCommand.Attention = current_tour_attention();
				// [[BPR-2014]]. Accepting records the code at every reference
				// the stop's text makes: the changed paths resolve them.
				if (aCommand.Kind == core::tour_command_kind::accept || aCommand.Kind == core::tour_command_kind::complete) {
					aCommand.Files = tour_changed_files();
				}
				static_cast<void>(TourHost->execute({ .Command = std::move(aCommand) }));
			} }, [this](qt::tour_editor_action aAction) { dispatch_tour_action(aAction); });
		Window->tour_editor()->set_repair_dispatch([this](core::tour_stop_id aStop, qt::tour_repair_action aAction) { dispatch_tour_repair(aStop, aAction); });
		// [[BPR-1781]]. Overview's Start tour: the published tour, imported
		// as following it in Review needs, at its first stop.
		Window->set_tour_start_dispatch([this] { start_published_tour(); });
		// [[BPR-1806]]. A published tour stop's Open in tour: the same,
		// at that stop.
		Window->set_tour_stop_dispatch([this](const std::string& aPost, const core::published_tour_stop& aStop) { start_published_tour(std::pair{ aPost, aStop }); });
#if defined(_WIN32)
		// [[BPR-0653]]. The strip is the title row: the host removes the
		// system title bar and answers the platform's hit test for it.
		TitleRow = std::make_unique<windows_title_row_host>(*Window);
#endif

		SetupDestination = std::make_unique<application::setup_destination_host>(*this);
		Window->set_setup_destination_dispatch([this](application::setup_destination_intent aIntent) {
			if (Closing || !SetupDestination) {
				return;
			}
			if (!std::holds_alternative<application::cancel_setup_destination>(aIntent)
				&& Host && !core::github_connection_setup_available(Host->state())) {
				return;
			}
			std::visit([this](const auto& aValue) {
				using intent_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<intent_type, application::continue_setup_destination>) {
					SetupDestination->continue_setup(aValue.Request);
				} else {
					retire_setup_effects();
					if constexpr (std::is_same_v<intent_type, application::submit_setup_destination>) {
						cancel_restoration(true);
						retire_cli_discovery();
						SetupDestination->submit(aValue.Text);
					} else if constexpr (std::is_same_v<intent_type, application::submit_repository_setup>) {
						retire_cli_discovery();
						SetupDestination->submit_repository(aValue.Text);
					} else {
						SetupDestination->cancel();
					}
				}
			},
					   aIntent);
		});
		Window->set_recent_review_dispatch([this](application::recent_review_intent aIntent) { open_recent(aIntent); });
		Window->set_recent_removal_dispatch([this](application::recent_review_row aRow) { forget_recent(aRow); });
		Window->set_review_window_dispatch([this](const application::review_window_intent& aIntent) { return request_review_window(aIntent); });
		// RFC-BPR-0108. Local PR drafts read the chosen worktree through
		// their own isolated Git reader; signed out as well as in.
		DraftDesktop = std::make_unique<local_draft_desktop>(*Window, ApplicationDataRoot, Profile->InstalledGit, windows_system_root(), WorkbenchEffects, DecisionLane,
															 local_draft_syntax{ .Definitions = SyntaxDefinitions.get(), .Effects = &AppearanceEffects, .Insights = Profile->InsightsRecorder });
		// RFC-BPR-0111. Create draft PR acts as this profile's accepted
		// account through its pull request catalog (a gh connection carries
		// it). A tests-enabled build creates nothing unless the agent-run live
		// course composed it, and then only on MikeMarcin/BPR-Test.
		// RFC-BPR-0111 as drawn: a draft pull request created from the window
		// opens there, read fresh as any opened pull request is.
		// It opens by the path Recently viewed's Open takes, under the
		// signed-in account's exact session only.
		DraftDesktop->set_pull_request_opener([this](const std::string& aHost, const std::string& aOwner, const std::string& aName, const std::uint64_t aNumber) {
			const auto* const session = Host ? &Host->accepted_session() : nullptr;
			if (Closing || session == nullptr || !*session || aNumber == 0U || aNumber > std::numeric_limits<std::uint32_t>::max()) {
				return;
			}
			Window->leave_local_draft();
			open_review_destination({ .Mode       = core::github_access_mode::authenticated,
									  .Hostname   = aHost,
									  .Repository = { .Owner = aOwner, .Name = aName },
									  .Number     = core::pull_request_number{ static_cast<std::uint32_t>(aNumber) },
									  .Account    = (*session)->Viewer.Account });
		});
		DraftDesktop->set_publishing([this]() -> std::optional<application::github_publication_account> {
			if (Closing || !Host) {
				return std::nullopt;
			}
#if defined(BPR_BUILD_TESTING)
			if (Fixture || !DesignatedLivePublication) {
				return std::nullopt;
			}
			auto account = Host->accepted_pull_request_account();
			return account ? std::optional{ designated_pull_request_account(std::move(*account)) } : std::nullopt;
#else
			return Host->accepted_pull_request_account();
#endif
		});
		Window->set_profile_label(QString::fromStdString(aAdmission.Label));
		Window->set_storage_location(QString::fromStdString(ApplicationDataRoot.string()));
		Window->set_receipt_dispatch(
			[this](core::file_receipt_command aCommand) {
#if defined(BPR_BUILD_TESTING)
				if (DisableReviewLedgerGestureWire) {
					return;
				}
#endif
				dispatch_receipt(std::move(aCommand));
			});
#if defined(BPR_BUILD_TESTING)
		ReviewSubmissionDecisionGate = std::make_unique<gated_review_mutation_decision_executor>(DecisionLane);
		auto& reviewDecisions        = *ReviewSubmissionDecisionGate;
#else
		auto& reviewDecisions = DecisionLane;
#endif
#if defined(BPR_BUILD_TESTING)
		// RFC-BPR-0099 AC08, RFC-BPR-0105 AC08. The agent-run live course
		// issues the same one-use grant the gesture issues in a shipped build
		// (five minutes, this request), and only for BPR-Test.
		review_submission_workflow::grant_factory issueReviewGrant = [this](auto aSession, auto aGeneration, const auto& aRequest) {
			if (Fixture) {
				return application::make_review_submission_testing_grant(aSession, aGeneration, aRequest);
			}
			if (DesignatedLivePublication && aRequest.LiveBinding && designated_live_publication_target(aRequest.LiveBinding->Target)) {
				return application::make_review_submission_testing_grant(aSession, aGeneration, aRequest, std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
			}
			return application::review_submission_execution_grant{ };
		};
#else
		review_submission_workflow::grant_factory issueReviewGrant = [](auto session, auto generation, const application::review_submission_request& request) { return application::detail::review_submission_user_authority_issuer::issue(session, generation, request); };
#endif
		ReviewSubmission = std::make_unique<review_submission_workflow>(ReviewSubmissionEffects, reviewDecisions, *Window->review_submission_editor_view(), std::move(issueReviewGrant), [this](const application::review_submission_live_binding& aBinding) { return admit_cli_publication(aBinding.Target, aBinding.ActingIdentity); },
																		// [[RFC-BPR-0105]]. Each review host sends on its own lane:
																		// tour publication, replies and another pull request's
																		// review work never wait in front of a submission.
																		[] { return std::make_unique<worker_executor>(); }, DiagnosticLog);
#if defined(BPR_BUILD_TESTING)
		review_tour_publication_workflow::grant_factory issueTourGrant = [this](auto aSession, auto aGeneration, const application::staged_review_plan& aPlan) {
			if (Fixture) {
				return application::make_review_submission_testing_grant(aSession, aGeneration, aPlan);
			}
			if (DesignatedLivePublication && aPlan.Submission.LiveBinding && designated_live_publication_target(aPlan.Submission.LiveBinding->Target)) {
				return application::make_review_submission_testing_grant(aSession, aGeneration, aPlan, std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
			}
			return application::review_submission_execution_grant{ };
		};
#else
		review_tour_publication_workflow::grant_factory issueTourGrant = [](auto aSession, auto aGeneration, const application::staged_review_plan& aPlan) {
			return application::detail::review_submission_user_authority_issuer::issue(aSession, aGeneration, aPlan);
		};
#endif
		ConversationReply = std::make_unique<conversation_reply_workflow>(ReviewSubmissionEffects, reviewDecisions, *Window, issueTourGrant, [this](const auto& aBinding) { return admit_cli_publication(aBinding.Target, aBinding.ActingIdentity); }, [this] { reread_after_publication(); });
		Window->set_conversation_reply_dispatch([this](const core::conversation_reply_action aAction, core::conversation_reply_draft aDraft) {if(!Closing && ConversationReply){ConversationReply->dispatch(aAction,std::move(aDraft));} });
		TourPublication = std::make_unique<review_tour_publication_workflow>(ReviewSubmissionEffects, reviewDecisions, *Window->tour_editor(), issueTourGrant, [this](const auto& aBinding) { return admit_cli_publication(aBinding.Target, aBinding.ActingIdentity); }, [this](const application::staged_publication_record& aRecord) {
                // [[BPR-1475]]. A published tour is a review on GitHub: read it.
                reread_after_publication();
                if(!TourHost){return;}const auto draft=TourHost->presentation().Draft;
                if(!draft){return;}
                if(auto published=application::published_tour(aRecord,*draft)){TourHost->observe_publication(std::move(*published));} }, DiagnosticLog);
		Window->set_review_submission_dispatch([this](qt::review_submission_editor_intent aIntent) {
			if (!Closing && ReviewSubmission) {
				ReviewSubmission->dispatch(std::move(aIntent));
			}
		});
		// [[BPR-1345]]. Start a review: pending drafts ride the review.
		ReviewSubmission->set_review_comment_hooks(
			[this](const application::review_submission_request& aRequest, std::function<void(bool)> aDone) { hold_review_comments(aRequest, std::move(aDone)); },
			[this](const std::string& aKey, application::review_submission_publication aPublication, const std::optional<application::review_submission_request>& aRequest) {
				settle_review_comments(aKey, aPublication, aRequest);
			},
			[this](const std::vector<core::review_submission_comment>& aCarried, const core::git_commit_oid& aHead) { discard_review_comments(aCarried, aHead); });
		ReviewSubmission->set_publication_observer([this] { reread_after_publication(); });
		ReviewSubmission->set_unplaceable_refusal_observer([this](const application::review_submission_request& aRefused) { demote_off_hunk_fact(aRefused); });
		// [[BPR-2263]]. A refusal the application owns is repaired over the
		// window's connection as it is now; a moved commit is read again.
		ReviewSubmission->set_connection_renewal([this](const application::review_submission_live_binding& aBinding) { return Closing ? std::nullopt : review_submission_factory(aBinding); });
		ReviewSubmission->set_reread_request([this] { reread_after_publication(); });
		// [[BPR-1816]]. Where a finished review goes is the profile's
		// accepted choice when GitHub answers, not the window's.
		ReviewSubmission->set_finish_settings([this] {
			const auto* const preferences = Profile->WorkbenchPreferences.get();
			const auto&       accepted    = preferences != nullptr ? preferences->accepted() : std::optional<application::workbench_profile_preferences>{ };
			return accepted ? accepted->Behaviour.FinishReview : core::review_finish_settings{ };
		});
		Window->set_start_review_dispatch([this] { return start_review_on_shown_draft(); });
		Window->set_unsent_reply_dispatch([this](const core::review_draft_id aDraft, const qt::workbench_window::unsent_reply_action aAction) { act_on_unsent_reply(aDraft, aAction); });
		Window->set_pending_comment_delete_dispatch([this](const core::review_draft_id aDraft, const std::uint64_t aRevision) { delete_pending_comment(aDraft, aRevision); });
		// [[BPR-2094]]. Finish your review's Add, Add all and Leave out.
		Window->set_draft_review_dispatch([this](const std::vector<core::review_draft_id>& aDrafts, const bool aAdd) {
			if (aAdd) {
				add_drafts_to_review(aDrafts);
			} else {
				leave_drafts_out_of_review(aDrafts);
			}
		});
		Window->set_carried_suggestion_dispatch([this](const core::review_draft_id aDraft, const std::uint64_t aRevision) { discard_carried_suggestion(aDraft, aRevision); });
		Window->set_review_draft_dispatch(
			[this](core::review_draft_command aCommand) {
				dispatch_review_draft(core::revisioned_review_draft_command{ .Command = std::move(aCommand) });
			});
		Window->review_editor()->set_revisioned_dispatch(
			[this](core::revisioned_review_draft_command aCommand) { dispatch_review_draft(std::move(aCommand)); });
		Window->set_review_reply_dispatch(
			[this](const std::string& aIdentity) { begin_review_reply(aIdentity); });
		Window->set_thread_reply_dispatch(
			[this](const std::string& aIdentity) { begin_thread_bound_reply(aIdentity); });
		Window->set_thread_resolution_dispatch(
			[this](const qt::thread_resolution_gesture& aGesture) { resolve_review_thread(aGesture); });
		Window->set_reaction_dispatch(
			[this](const qt::reaction_gesture& aGesture) { react_to_post(aGesture); });
		// [[BPR-1774]]. Save in Overview's description editor.
		Window->overview_page()->set_description_save_dispatch(
			[this](const qt::overview_description_save& aSave) {
				// A local draft's description is the draft's, saved with its title.
				if (local_tour_shown()) {
					DraftDesktop->save_description(aSave.Body);
					Window->overview_page()->close_description_editor();
					return;
				}
				save_description(aSave);
			});
		// [[BPR-2280]]. Save in Author's description editor: the same write.
		Window->author_description()->set_save_dispatch(
			[this](const qt::overview_description_save& aSave) { save_description(aSave, description_editor::author); });
		// [[BPR-2281]]. Draft Description beside it, and Stop drafting.
		Window->author_description()->set_draft_dispatch([this] { request_description_draft(); },
														 [this] {
															 if (DescriptionDraft) {
																 DescriptionDraft->stop();
															 }
														 });
		// [[BPR-1852]]. Ready for review and Convert to draft, from Overview
		// and the Pull Request menu.
		Window->set_draft_state_dispatch(
			[this](const core::draft_state_change aChange) { change_draft_state(aChange); });
		// [[BPR-2329]]. Enable auto-merge (with its method) and Disable, in Merge.
		Window->overview_page()->set_auto_merge_dispatch(
			[this](const core::auto_merge_change aChange, const std::optional<core::merge_method> aMethod) { change_auto_merge(aChange, aMethod); });
		// [[BPR-1804]]. Request review in People and its picker.
		Window->overview_page()->set_reviewer_picker_dispatch(
			[this](const qt::reviewer_picker_gesture& aGesture) { reviewer_gesture(aGesture); });
		// [[BPR-1775]]. Save while editing one of my comments.
		if (Window->conversation_panel() != nullptr && Window->conversation_panel()->cards() != nullptr) {
			Window->conversation_panel()->cards()->set_comment_edit_dispatch(
				[this](const qt::comment_edit_save& aSave) { save_comment_edit(aSave); });
		}

#if defined(BPR_AGENT_INTEGRATIONS)
		// BPR-0533. The six comment-workshop controls reach the composed
		// workshop host and nothing else: the editor asks for an operation
		// on one exact draft, and this composition is what decides whether
		// there is anything to answer with. The widget holds no host, no
		// agent, and no revision of its own.
		// [[BPR-2051]]. Enhance on a draft comment, a reply or a private
		// note reaches the writing operation a tour stop's Enhance uses.
		Window->set_enhance_dispatch([this](qt::enhance_request aRequest) { request_enhancement(std::move(aRequest)); },
									 [this](const std::uint64_t aRequest) { cancel_enhancement(aRequest); });
		if (Window->review_editor() != nullptr) {
			Window->review_editor()->set_workshop_dispatch(
				[this](qt::comment_workshop_request aRequest) {
					ask_comment_workshop(aRequest);
				});
		}
#endif
		Window->comparison_surface()->set_selection_sink(
			[this](const std::optional<core::semantic_selection>& aSelection) {
				observe_review_selection(aSelection);
				retain_reading_position();
				// [[BPR-1236]]. The active tab takes the reading now, with
				// the position: a Home or Quit before the scheduled
				// presentation would otherwise keep a tab without it.
				observe_tab_reading();
				schedule_tab_presentation();
			});
		Window->comparison_surface()->set_line_reselection_sink([this](const core::semantic_selection& aSelection) {
			// An unused draft can be retired by a reread that keeps its
			// selected line. Re-choosing that exact line retries the normal
			// presenter, after tab restoration and pending draft work end.
			const auto selection = std::optional{ aSelection };
			if (!Closing && !PendingReviewMutation && !PrivateDraftCommandPending && !PrivateDraftSelectionPending
				&& !TabLoad && !ReviewDraft && selection == SelectedSemanticSelection
				&& Window->comparison_surface()->selection() == selection) {
				present_review_draft(aSelection);
			}
		});
		Window->comparison_surface()->set_reading_gesture_sink([this] {
			cancel_restoration(false, true);
			// A line can be chosen on the exact displayed diff before
			// its tab driver finishes. That newer gesture owns the reading;
			// completing the load must not restore an older selection.
			if (!Closing && TabLoad && SelectedDiffPublication && SelectedDiffRequest == std::optional{ SelectedDiffPublication->Request }
				&& SelectedDiffPublication->Request.Context.Generation == WorkbenchHost->generation()) {
				auto shown      = TabLoad->View;
				shown.Selection = Window->comparison_surface()->selection();
				if (tab_view_presented(shown)) {
					finish_tab_load(core::document_tab_view_outcome::ready, true);
				}
			}
			retain_reading_position();
			observe_tab_reading();
			schedule_tab_presentation();
		});
		Window->set_defect_dispatch(
			[this](core::defect_command aCommand) {
				dispatch_defect(std::move(aCommand));
			});
		Window->set_defect_classification_dispatch(
			[this](qt::workbench_defect_classification aClassification) {
				classify_defect(std::move(aClassification));
			});
		Window->set_conversation_read_dispatch(
			[this](const std::string& aThread) { mark_conversation_read(aThread); });
		Window->set_conversation_unread_dispatch(
			[this](const std::string& aThread) { mark_conversation_unread(aThread); });
		Window->set_conversations_read_dispatch(
			[this](const std::optional<std::string>& aAuthor) { mark_conversations_read(aAuthor); });

		Window->set_queue_rule_dispatch(
			[this](application::queue_rule_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					QueueHost->apply(std::move(aIntent));
				}
			});
		Window->set_queue_preferences_dispatch(
			[this](application::queue_preferences_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					QueueHost->apply(std::move(aIntent));
				}
			});
#if defined(BPR_BUILD_TESTING)
		// [[BPR-1083]], [[BPR-1088]]. `--label-helper`, the attended
		// workshop look and the label-authoring probe turn the helper on
		// through the profile's own setting: the write the Comment labels ›
		// Label helper switch makes. Since [[BPR-1565]] the helper only
		// sends the Review feedback guidance with a workshop request; the
		// probe keeps it on to show a typed `:qu:` stays text even so.
		if (Profile->LabelHelper && !Profile->LabelHelperRequested && Profile->WorkbenchPreferences) {
			Profile->LabelHelperRequested = true;
			auto* const preferences       = Profile->WorkbenchPreferences.get();
			const auto  subscription      = std::make_shared<std::uint64_t>(0U);
			*subscription                 = preferences->subscribe(DraftWindow.Profile, [preferences, subscription](const application::workbench_profile_preferences& aAccepted) {
				if (aAccepted.Behaviour.LabelHelper == application::review_label_helper::on) {
					if (*subscription != 0U) {
						preferences->unsubscribe(*subscription);
					}
					return;
				}
				auto next        = aAccepted.Behaviour;
				next.LabelHelper = application::review_label_helper::on;
				static_cast<void>(preferences->apply_behaviour(next));
			});
		}
#endif
		Window->set_behaviour_dispatch(
			[this](const application::review_behaviour_settings& aSettings) {
				if (!Closing && WorkbenchHost != nullptr) {
					WorkbenchHost->apply_behaviour(aSettings);
				}
			});
		// [[BPR-1844]]. One memory for the profile's windows: read when a
		// pull request opens, told each reviewer's own choice.
		Window->set_view_filter_memory(
			[this] { return Profile->ViewFilterMemory ? Profile->ViewFilterMemory->defaults() : core::view_filter_defaults{ }; },
			[this](const core::view_filter_remember& aChoice) {
				if (!Closing && Profile->ViewFilterMemory) {
					Profile->ViewFilterMemory->remember(aChoice);
				}
			});
		// [[BPR-2171]]. The profile's Discuss preferences: each repository's
		// bot marks, for every window.
		Window->set_discuss_preferences(
			[this] { return Profile->DiscussPreferences ? Profile->DiscussPreferences->preferences() : core::discuss_preferences{ }; },
			[this](const std::string& aRepository, const std::string& aLogin, const bool aBot) {
				if (!Closing && Profile->DiscussPreferences) {
					static_cast<void>(Profile->DiscussPreferences->mark(aRepository, aLogin, aBot));
				}
			});
		// [[BPR-1089]]. The Overview's chips: its request is counted from the
		// kept label buckets, and a count's activation lists exactly its
		// conversations in Discuss. Neither asks a provider.
		Window->set_overview_feedback_dispatch(
			[this](const application::overview_feedback_request& aRequest) {
				if (!Closing) {
					FeedbackRequest = aRequest;
					publish_overview_feedback();
				}
			});
		// RFC-BPR-0089. The Commits tab and the filter chips.
		Window->set_commit_filter_dispatch([this](const application::commit_filter_gesture& aGesture) { apply_commit_filter_gesture(aGesture); });
		Window->set_commit_filter_shown_dispatch([this] { want_commit_filter_trees(); });
		// RFC-BPR-0089 M2. File filters and generated-file gestures.
		Window->set_file_filter_dispatch([this](const std::vector<std::string>& aCommits) { apply_file_filter_gesture(aCommits); });
		Window->set_generated_dispatch([this](const application::generated_file_gesture& aGesture) { apply_generated_gesture(aGesture); });
		// [[BPR-1693]]. A set-aside file's diff, read on request.
		Window->set_generated_open_dispatch([this](const std::string& aPath) { open_set_aside_file(aPath); });
		// RFC-BPR-0089 M3. A split file's per-commit diff, and a rematch offer.
		Window->set_split_segment_dispatch([this](const std::size_t aSegment) {
			if (!Closing && WorkbenchHost != nullptr && SelectedDiffPublication) {
				WorkbenchHost->select_split_segment(SelectedDiffPublication->Request.File.File, aSegment);
			}
		});
		Window->set_commit_rematch_dispatch([this](const application::commit_filter_rematch_gesture& aGesture) { apply_commit_rematch(aGesture); });
		Window->set_commit_open_dispatch([this](const std::string& aCommit) {
			if (!Closing && WindowRead) {
				static_cast<void>(WindowRead->open_pull_request_commit_page(aCommit, Profile->WindowBrowser));
			}
		});
		// [[BPR-2327]]. Resolve on GitHub: the accepted pull request's conflicts page.
		Window->set_conflicts_page_dispatch([this] {
			if (!Closing && WindowRead) {
				static_cast<void>(WindowRead->open_pull_request_conflicts_page(Profile->WindowBrowser));
			}
		});
		Window->set_overview_drill_down_dispatch(
			[this](const application::overview_drill_down& aDrillDown) {
				if (!Closing && FeedbackRequest) {
					open_feedback_drill_down(aDrillDown, *FeedbackRequest, true);
				}
			});
		// [[BPR-0783]]. Home's arrangement is this window's, so it travels
		// the same durable path as the window's layout: Qt reports the view
		// it now shows, the host validates and persists it, and the record
		// it loads comes back as the projection's HomeView.
		Window->set_home_view_dispatch(
			[this](const application::home_view_state& aView) {
				if (!Closing && WorkbenchHost != nullptr) {
					WorkbenchHost->apply_home_view(aView);
				}
			});
		Window->overview_page()->set_summary_resize_dispatch([this](std::optional<std::uint32_t> aFraction) {
			if (!Closing && WorkbenchHost != nullptr) {
				WorkbenchHost->resize_summary(aFraction);
			}
		});
		// [[BPR-1588]]. The Overview's Status drawer, kept like the summary:
		// this window's own width and collapsed state, stored locally.
		Window->overview_page()->status_drawer()->set_change_dispatch([this](const application::workbench_status_drawer& aDrawer) {
			if (!Closing && WorkbenchHost != nullptr) {
				WorkbenchHost->apply_status_drawer(aDrawer);
			}
		});
		// [[BPR-1334]]. The window's own Line details height, kept like
		// the summary's: an explicit gesture or Reset, stored locally only.
		Window->comparison_surface()->set_line_details_resize_sink([this](std::optional<std::uint32_t> aHeight) {
			if (!Closing && WorkbenchHost != nullptr) {
				WorkbenchHost->resize_line_details(aHeight);
			}
		});
		// [[BPR-2333]]. The window's own item side chat width, kept like
		// Line details': an explicit drag or Reset, stored locally only.
		Window->item_side_chat()->set_width_sink([this](std::optional<std::uint32_t> aWidth) {
			if (!Closing && WorkbenchHost != nullptr) {
				WorkbenchHost->resize_side_chat(aWidth);
			}
		});
		Window->set_geometry_dispatch(
			[this](const application::workbench_window_geometry& aGeometry) {
				if (!Closing && WorkbenchHost != nullptr) {
					WorkbenchHost->observe_geometry(aGeometry);
				}
			});
		Window->set_saved_repository_dispatch(
			[this](application::saved_repository_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					QueueHost->apply(std::move(aIntent));
				}
			});
		Window->set_repository_addition_dispatch(
			[this](application::repository_addition_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					aIntent.Source = DraftWindow;
					QueueHost->apply(std::move(aIntent));
				}
			});
		// [[BPR-0990]]. Follow is the queue's; Home's glyph and Home's
		// activation are the profile's discovery. Neither reaches GitHub
		// from here: discovery reads, and only under the accepted session.
		Window->set_queue_follow_dispatch(
			[this](application::queue_follow_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					QueueHost->apply(std::move(aIntent));
				}
			});
		// [[BPR-1683]]. Ignore is the queue's too, and local.
		Window->set_home_ignore_dispatch(
			[this](core::home_ignore_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					QueueHost->apply(std::move(aIntent));
				}
			});
		// [[BPR-2049]]. Seen is the queue's, and local.
		// [[BPR-2249]]. The review's Seen leaves the review as a confirmed
		// Approve does: the core decides from Approve's accepted finish
		// choice once the queue has kept it; anything else keeps the review.
		Window->set_home_seen_dispatch(
			[this](application::queue_seen_intent aIntent, qt::workbench_seen_answer aAnswer) {
				if (Closing || QueueHost == nullptr) {
					if (aAnswer) {
						aAnswer(core::review_finish_transition::keep_review);
					}
					return;
				}
				if (!aAnswer) {
					QueueHost->apply(std::move(aIntent));
					return;
				}
				QueueHost->apply(std::move(aIntent), [this, weak = std::weak_ptr<const int>{ WindowLifetime }, answer = std::move(aAnswer)](const application::queue_seen_outcome aOutcome) {
					if (weak.expired() || Closing) {
						return;
					}
					const auto* const preferences = Profile->WorkbenchPreferences.get();
					const auto&       accepted    = preferences != nullptr ? preferences->accepted() : std::optional<application::workbench_profile_preferences>{ };
					answer(core::decide_seen_finish(accepted ? accepted->Behaviour.FinishReview : core::review_finish_settings{ },
													aOutcome == application::queue_seen_outcome::recorded));
				});
			});
		// [[BPR-1407]]. Home's row ages are measured to discovery's clock.
		Window->set_home_clock([this]() -> std::optional<std::int64_t> {
			return DiscoveryClock != nullptr ? std::optional{ DiscoveryClock->now_seconds() } : std::nullopt;
		});
		Window->set_queue_discovery_dispatch(
			[this](const application::queue_discovery_intent aIntent) {
				if (Closing || Discovery == nullptr) {
					return;
				}
				if (aIntent == application::queue_discovery_intent::refresh) {
					Discovery->refresh();
				} else {
					Discovery->activate();
				}
			});
		// [[BPR-1848]]. Coming back to Home from a pull request: discovery
		// decides, through the core, whether that reads the row, the list or
		// nothing.
		Window->set_home_return_dispatch(
			[this](const core::pull_request_ref& aRef) {
				if (!Closing && Discovery != nullptr) {
					Discovery->returned(aRef);
				}
			});
		// [[BPR-0660]]. Remembering a server is the same durable path.
		Window->set_saved_server_dispatch(
			[this](application::saved_server_intent aIntent) {
				if (!Closing && QueueHost != nullptr) {
					if (aIntent.Action == application::saved_server_action::remove && Profile->Connections) {
						Profile->for_each_window([&](const auto& aObserver) { if (aObserver.RetireServerSetup) { aObserver.RetireServerSetup(aIntent.Server.Hostname); } });
						static_cast<void>(Profile->Connections->forget(aIntent.Server.Hostname));
						if (DeviceHost->state().Profile) {
							const auto* pending = find_profile(*DeviceHost->state().Profile);
							if (pending && pending->hostname() == aIntent.Server.Hostname) {
								DeviceHost->dispatch(core::cancel_device_authorization_intent{ });
							}
						}
						if (Host->state().ActiveProfile) {
							const auto* active = find_profile(*Host->state().ActiveProfile);
							if (active && active->hostname() == aIntent.Server.Hostname) {
								DeviceHost->dispatch(core::cancel_device_authorization_intent{ });
								Host->dispatch(core::deactivate_github_identity_intent{ });
							}
						}
					}
					QueueHost->apply(std::move(aIntent));
				}
			});
		if (Profile->cli_read_offered_here()) {
			Window->set_github_cli_discovery_dispatch(
				[this] { discover_github_cli_accounts(); });
			Window->set_github_cli_check_dispatch(
				[this] { discover_github_cli_accounts(false); });
			present_github_cli_accounts();
		}
		compose_account_setup();

		EpochHost = std::make_unique<application::epoch_application_host>(
			core::epoch_state{
				.ApplicationSession = WindowSession,
			},
			*Profile->EpochActors, DraftWindow,
			*EpochCatalogs,
			*EpochEntropy,
			*EpochClock,
			EpochEffects,
			ReviewerDecisions,
			*this, application::epoch_host_limits{ }, DiagnosticLog);
		EpochHost->set_history_effects(EpochHistoryEffects);

		PrivateReview = std::make_shared<private_review_workflow>(ReviewMutationEffects, DecisionLane, ReviewLedgerFile, Profile->PrivateRecovery, *Window->private_review(), Window.get(), [this](const application::durable_draft_commit& aCommit, bool aOpen) { adopt_private_drafts(aCommit, aOpen); }, [this](core::agent_profile_id aAgentProfile, application::private_run_intent aIntent, core::agent_setting_facts aRequested, std::string aPrompt) {
#if defined(BPR_AGENT_INTEGRATIONS)
			if (Closing || !AgentHost || !agent_turn_quiescent()) {
				return false;
			}
			const auto composing = aIntent.Purpose == core::private_run_purpose::composition;
			ExplicitPrivateRun   = std::move(aIntent);
			PrivateRequested     = std::move(aRequested);
			const auto sent      = dispatch_agent_invocation(aAgentProfile, { }, std::nullopt, false, aPrompt, std::nullopt,
															 composing ? core::agent_entry_point::workshop_comment : core::agent_entry_point::ask_about_selection,
															 composing ? std::optional{ core::workshop_operation::clarify } : std::nullopt);
			ExplicitPrivateRun.reset();
			PrivateRequested = { };
			return sent;
#else
			// [[BPR-1179]]. An agent-free desktop has no agent to run: private
			// history stays readable, and a request for a new run is refused.
			static_cast<void>(this);
			static_cast<void>(aAgentProfile);
			static_cast<void>(aIntent);
			static_cast<void>(aRequested);
			static_cast<void>(aPrompt);
			return false;
#endif
		});
#if defined(BPR_AGENT_INTEGRATIONS)
		// The one new edge out of the agent host. The launch panel still
		// receives every projection it already did; this adds the typed
		// finding delivery, and nothing else. There is deliberately no
		// path from here to the review-mutation coordinator: RFC-BPR-0012's
		// "no private backchannel" is that absence rather than a check
		// somewhere downstream.
		AgentProjections
			= std::make_unique<composition_agent_invocation_projection>(
				*Window->agent_panel(),
				[this](
					const application::
						agent_invocation_projection_identity&  aIdentity,
					const application::agent_finding_delivery& aDelivery) {
					static_cast<void>(aIdentity);
					if (Closing) {
						return;
					}
					if (FindingHost != nullptr) {
						FindingHost->deliver(aDelivery);
					}
					// The chat's reply is this same validated value, at
					// this same seam. There is no second delivery edge for
					// a chat turn, so nothing a chat receives can bypass
					// the finding bounds this one enforces.
					if (ChatHost != nullptr) {
						ChatHost->deliver(aDelivery);
					}
					Window->present_dif_agent_delivery(
						aIdentity,
						aDelivery,
						ChatHost != nullptr && SideChat != nullptr
							&& !aDelivery.Findings.empty(),
						FindingHost != nullptr && FindingQueue != nullptr
							&& !aDelivery.Findings.empty());
					// BPR-0538. Whether the turn a press is waiting on has
					// produced anything the reviewer could still convert.
					// Recorded from the delivery rather than inferred at
					// the terminal, because by then the payload is gone.
					if (PendingWorkshopOperation.has_value()
						&& PendingWorkshopOperation->Session.has_value()
						&& *PendingWorkshopOperation->Session
							== aDelivery.Session
						&& !aDelivery.Findings.empty()) {
						PendingWorkshopOperation->Delivered = true;
					}
				},
				[this](const core::agent_session& aSession) {
					if (Closing) {
						return;
					}
					LatestAgentSession = aSession;
					if (ChatHost != nullptr) {
						ChatHost->admitted(aSession);
					}
					// The same seam, for the same reason. A workshop press
					// learns which turn is answering it here rather than
					// keeping an identity of its own, so the outcome the
					// operation is spent on is the outcome of the turn the
					// press actually started. Only a press still waiting
					// for a turn takes one: a session admitted for the
					// launch panel or the chat while a workshop turn is
					// already running belongs to neither.
					if (PendingWorkshopOperation.has_value()
						&& !PendingWorkshopOperation->Recording
						&& !PendingWorkshopOperation->Session.has_value()) {
						PendingWorkshopOperation->Session = aSession.Id;
					}
				},
				[this](
					const application::agent_invocation_projection_identity&
						aIdentity) {
					if (!Closing && Window != nullptr) {
						if (PendingWriting && !PendingWriting->Invocation) {
							PendingWriting->Invocation = aIdentity;
						}
						Window->admit_dif_invocation(aIdentity);
					}
				},
				// BPR-0538. The third sink, and the one this composition
				// was missing: the turn a press started has ended. Until
				// it existed the six controls stayed shut behind "a
				// workshop turn is already running" long after the turn
				// stopped running, and only an unrelated draft edit
				// reopened them.
				[this](
					const application::agent_invocation_projection_identity&
														 aIdentity,
					const application::agent_run_result& aResult) {
					if (Closing) {
						return;
					}
					finish_writing(aIdentity, aResult);
					note_agent_scratch(aIdentity.Session.Profile, aResult);
					settle_comment_workshop(
						aIdentity.Session.Id, aResult.Outcome);
					Window->finish_dif_invocation(aIdentity, aResult.Outcome);
					// [[BPR-2181]]. The session's runtime ran this turn on its
					// own host, so the native history this window's host last
					// read no longer holds it: the next look reads it again.
					if (AgentHost != nullptr) {
						AgentHost->native_history_changed();
					}
				},
				[this](const application::prepared_workspace& aWorkspace) {
					LastAgentWorkspacePath = aWorkspace.Path;
				},
				[this](
					const application::agent_invocation_projection_identity&
						aIdentity) {
					if (!Closing && Window != nullptr) {
						Window->start_dif_invocation(aIdentity);
					}
				},
				[this](std::string_view) {
					if (!PendingWriting) {
						return;
					}
					if (const auto enhance = PendingWriting->Enhance) {
						PendingWriting.reset();
						WritingState.cancel();
						answer_enhancement(*enhance, std::nullopt, QStringLiteral("The agent refused this request. Your text is unchanged."));
						return;
					}
					const bool test = PendingWriting->Test;
					PendingWriting.reset();
					WritingState.cancel();
					if (test) {
						Window->writing_hooks()->show_notice("Writing request was refused by the selected agent host.");
					} else {
						Window->tour_editor()->show_writing_notice(QStringLiteral("Writing request was refused by the selected agent host. Manual editing is available."));
					}
				});
		auto agentProfiles = core::built_in_agent_profiles();
		// [[BPR-1753]], owner amendment 2026-10-23. Every slot an
		// added agent may launch as, told to every host once.
		std::ranges::move(core::agent_entry_profiles(), std::back_inserter(agentProfiles));
		// Stated once and given to both hosts. The chat host asks
		// `core::decide_agent_action` the same host-policy question the
		// launch host already asks, and two lists written apart are two
		// answers waiting to differ.
		const auto agentPolicies = std::vector<core::agent_host_policy>{
			core::built_in_agent_policy(
				core::git_host_id{ 1U },
				core::built_in_agent_host_kind::github_com),
			core::built_in_agent_policy(
				core::git_host_id{ 2U },
				core::built_in_agent_host_kind::explicit_ghes),
		};
		// [[BPR-1749]]. Setup's rows carry the same policy.
		AgentPolicies = agentPolicies;
	#if defined(BPR_BUILD_TESTING)
		if (ExternalAgentExecutable.has_value()) {
			const auto external = std::ranges::find(
				agentProfiles,
				core::external_terminal_profile_id,
				&core::agent_profile::Id);
			if (external != agentProfiles.end()) {
				external->Executable = ExternalAgentExecutable->string();
			}
			BuiltInProfilesOffered = true;
		}
		if (ClaudeAgentExecutable) {
			const auto claude = std::ranges::find(agentProfiles, core::claude_stream_json_profile_id, &core::agent_profile::Id);
			if (claude != agentProfiles.end()) {
				claude->Executable = ClaudeAgentExecutable->string();
				// [[BPR-2177]]. A person's session says whose answers they read.
				if (FixtureAgentSession) {
					claude->DisplayName = "Fixture agent";
				}
			}
			BuiltInProfilesOffered = true;
		}
		if (CodexAgentExecutable.has_value()) {
			const auto codex = std::ranges::find(
				agentProfiles,
				core::codex_jsonl_profile_id,
				&core::agent_profile::Id);
			if (codex != agentProfiles.end()) {
				codex->Executable = CodexAgentExecutable->string();
				if (FixtureAgentSession) {
					codex->DisplayName = "Fixture agent (Codex)";
				}
			}
			BuiltInProfilesOffered = true;
		}
	#endif
	#if defined(BPR_BUILD_TESTING)
		if (!DisableAgentProcessLauncherComposition) {
	#endif
			AgentProcess
				= std::make_unique<adapters::agent::agent_process_launcher>();
	#if defined(BPR_BUILD_TESTING)
		}
		if (AgentProcess != nullptr) {
			AgentProcessProbe
				= std::make_unique<composition_agent_process_launcher_probe>(
					*AgentProcess);
			// [[BPR-2177]] (owner, 2026-11-14). With a fixture, a program named
			// `claude` or `codex` starts only from a folder this run named (the
			// fixture agent's, a course's stub, a search path it set); in a
			// person's session every program must be one of those.
			if (Fixture) {
				AgentProcessProbe->set_admission(fixture_agent_admission());
			}
		}
		if (AgentProcessProbe != nullptr) {
			ClaudeProcess = std::make_unique<application::wrapped_claude_process>(*AgentProcessProbe, ClaudeWrapperSource, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
			// [[BPR-1748]], RFC-BPR-0092 Q5. Codex and the plain-text
			// harness get an absolute program here, in front of the Claude
			// wrapper, so a shipped build starts them.
			LocatedProcess = std::make_unique<application::located_agent_process>(*ClaudeProcess, AgentProgramSource, &adapters::agent::inspect_claude_process_wrapper, locate_agent_program());
			EntryProcess   = std::make_unique<application::agent_entry_process>(*AgentProcessProbe, *LocatedProcess, AgentEntries, &adapters::agent::inspect_claude_process_wrapper, locate_claude(), locate_agent_program());
		}
		if (!DisableAgentHostComposition && AgentProcessProbe != nullptr)
	#else
		ClaudeProcess  = std::make_unique<application::wrapped_claude_process>(*AgentProcess, ClaudeWrapperSource, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
		LocatedProcess = std::make_unique<application::located_agent_process>(*ClaudeProcess, AgentProgramSource, &adapters::agent::inspect_claude_process_wrapper, locate_agent_program());
		EntryProcess   = std::make_unique<application::agent_entry_process>(*AgentProcess, *LocatedProcess, AgentEntries, &adapters::agent::inspect_claude_process_wrapper, locate_claude(), locate_agent_program());
	#endif
		{
			// BPR-0532. The re-alignment a continued turn asks for, served
			// by the same two ports a fresh alignment resolves and
			// materializes through, so one revision cannot be aligned two
			// ways. Composed with the chat host, because it is only a
			// continued turn of that host's that ever asks for one.
			Realigner
				= std::make_unique<application::composed_agent_realignment>(
					*EpochHost, *EpochHost, *LineMatcher);
			// [[BPR-1827]]. Every run the host starts leaves an
			// `agent.run` started and ended line: enums and counts only.
			DiagnosedProcess = std::make_unique<application::diagnosed_agent_process>(*EntryProcess, DiagnosticLog);
			// [[BPR-2181]]. The profiles this window's agent host offers, for the
			// session runtime it seeds.
			ComposedAgentProfiles = agentProfiles;
			AgentHost             = std::make_unique<application::agent_invocation_host>(
				agentProfiles,
				agentPolicies,
				AgentWorkspaceNeighbourhood,
				*this,
				*EpochHost,
				*DiagnosedProcess,
				[](const std::string& aName) -> std::optional<std::string> {
					if (!qEnvironmentVariableIsSet(aName.c_str())) {
						return std::nullopt;
					}
					return qEnvironmentVariable(aName.c_str()).toStdString();
				},
				AgentEffects,
				DecisionLane,
				*AgentProjections, PrivateReview.get(), NativeAgentSessions.get(), DiagnosticLog.get());
			// [[BPR-2007]], RFC-BPR-0104. Scratch folders sit beside the
			// profile's data. Claude cannot match a rule to a quoted path, so a
			// root whose path has a space (macOS's "Application Support") moves
			// to the user's cache folder, one per profile; with neither usable
			// no folder is granted.
			{
				auto leaf = ApplicationDataRoot.filename();
				leaf += "-agent-scratch-v1";
				auto root = ApplicationDataRoot.parent_path() / leaf;
				if (!application::valid_agent_grant_path((root / "x").string())) {
					auto hash = std::uint64_t{ 14695981039346656037ULL };
					for (const auto byte : ApplicationDataRoot.generic_string()) {
						hash = (hash ^ static_cast<unsigned char>(byte)) * 1099511628211ULL;
					}
					root = std::filesystem::path{ QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation).toStdString() } / "betterpullreview-agent-scratch-v1"
						/ QString::number(hash, 16).toStdString();
				}
				if (application::valid_agent_grant_path((root / "x").string())) {
					AgentHost->set_scratch_root(root);
					AgentScratchRoot = root;
				}
			}
			AgentProjections->NativeSession = [this](const core::agent_session& aSession) { if (!Closing && ChatHost) { LatestAgentSession=aSession; ChatHost->admitted(aSession,true); } };
			AgentProjections->NativeHistory = [this](const application::native_agent_history_projection& aView) { present_agent_native_history(aView); };
			compose_tour_session(agentProfiles, agentPolicies);
			compose_local_draft_agent(agentProfiles);
		}
		PrivateReview->profiles(agentProfiles);
#else
		// [[BPR-1179]]. No agent profile is offered, discovered or resolved,
		// and the window offers no control that would need one.
		PrivateReview->profiles({ });
		Window->withdraw_agent_surfaces();
#endif
#if defined(BPR_AGENT_INTEGRATIONS)
		AgentProjections->set_offer_filter([this](const std::span<const core::agent_profile> aProfiles) {
			HostOfferedProfiles.assign(aProfiles.begin(), aProfiles.end());
			return offered_agent_profiles();
		});
		AgentProjections->set_private_sink([this](const auto& aDelta) {if(!Closing && PrivateReview){PrivateReview->changed(aDelta);} }, [this](const auto&) {if(!Closing && Window){Window->private_review()->show_status(QStringLiteral("Private run admission or retention refused; no missing provenance is inferred."));} });
		Window->set_agent_dispatch(
			[this](qt::agent_intent aIntent) {
	#if defined(BPR_BUILD_TESTING)
				if (DisableAgentGestureWire) {
					return;
				}
	#endif
				dispatch_agent_intent(std::move(aIntent));
			});
		// RFC-BPR-0012 composition, here rather than anywhere else and
		// under the same fixture and production split the agent host and
		// the review-mutation host already use. A surface built in one of
		// the two graphs is a claim about the other one nobody checks.
		auto mcpHelper = std::filesystem::path{ };
	#if defined(BPR_BUILD_TESTING)
		mcpHelper = std::filesystem::path{ BPR_MCP_HELPER_EXECUTABLE };
	#else
		mcpHelper = native_filesystem_path(QCoreApplication::applicationDirPath()) / BPR_MCP_HELPER_FILE_NAME;
	#endif
		// Its own launcher rather than the agent's. `cancel` is a launcher
		// operation, and one shared between an agent run and a helper
		// lifetime would stop whichever the reviewer did not mean.
		McpProcess      = std::make_unique<adapters::agent::agent_process_launcher>();
		McpSnapshotRoot = ApplicationDataRoot / "mcp-snapshot" / ("window-" + std::to_string(DraftWindow.Window.value()) + "-generation-" + std::to_string(DraftWindow.Generation.value()));
	#if defined(BPR_BUILD_TESTING)
		if (!DisableMcpHelperComposition)
	#endif
		{
			McpHelper = std::make_unique<
				application::mcp_context_helper_supervisor>(
				std::move(mcpHelper),
				*McpProcess,
				McpEffects,
				DecisionLane,
				DiagnosticLog.get()); // [[BPR-1830]]
		}
	#if defined(BPR_BUILD_TESTING)
		if (!DisableSideChatPanelComposition)
	#endif
		{
			// BPR-0513: the window places both panels in its conversations
			// drawer, so the composition here is the one edge that matters
			// — the dispatch. A panel this branch skips stays on screen and
			// stays inert, because a widget nobody handed a dispatch to has
			// nowhere to send anything.
			SideChat = Window->side_chat();
			SideChat->set_dispatch([this](qt::side_chat_intent aIntent) {
				dispatch_side_chat_intent(std::move(aIntent));
			});
		}
	#if defined(BPR_BUILD_TESTING)
		if (!DisableFindingQueueComposition)
	#endif
		{
			FindingQueue = Window->finding_queue();
			FindingQueue->set_dispatch(
				[this](qt::finding_action_intent aIntent) {
					dispatch_finding_intent(std::move(aIntent));
				});
		}
#endif
#if defined(BPR_BUILD_TESTING)
		if (!DisableDeviceAuthorizationPanelComposition)
#endif
		{
			// [[BPR-0526]], on exactly the terms above. The window mounted
			// the panel in its review header; the composition adopts it and
			// wires its two edges — gestures out through the device host,
			// presentations in through the fan-out below, which reads
			// `DevicePanel` on every publication. Skipping this branch
			// leaves a panel that is drawn, is inert, and is never told
			// anything.
			DevicePanel = Window->device_authorization();
			Window->set_device_authorization_dispatch(
				[this](application::device_authorization_gesture aGesture) {
					dispatch_device_authorization_gesture(aGesture);
				});
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		FindingProjections
			= std::make_unique<composition_agent_finding_projection>(
				[this](const application::agent_finding_delivery& aDelivery) {
					if (FindingQueue != nullptr) {
						FindingQueue->present(aDelivery);
					}
				},
				[this](const application::agent_finding_outcome& aOutcome) {
					present_finding_outcome(aOutcome);
				},
				[this]() {
					if (FindingQueue != nullptr) {
						FindingQueue->present(
							application::agent_finding_delivery{ });
					}
				});
		WorkshopProjections
			= std::make_unique<composition_comment_workshop_projection>(
				[this](
					const application::agent_comment_workshop_proposal&
						aProposal) {
					if (Window == nullptr
						|| Window->review_editor() == nullptr) {
						return;
					}
					++CommentWorkshopProposals;
					Window->review_editor()->present(
						qt::comment_workshop_proposal{
							.Draft     = aProposal.Draft,
							.Original  = aProposal.Original,
							.Proposed  = aProposal.Proposed,
							.Operation = aProposal.Operation,
						});
				},
				[this](
					const application::agent_comment_workshop_refusal
						aRefusal) {
					present_agent_refusal(
						"comment workshop", application::name(aRefusal));
				});
	#if defined(BPR_BUILD_TESTING)
		if (!DisableCommentWorkshopComposition)
	#endif
		{
			WorkshopHost = std::make_unique<
				application::agent_comment_workshop_host>(
				*WorkshopProjections);
		}
	#if defined(BPR_BUILD_TESTING)
		if (!DisableAgentFindingHostComposition)
	#endif
		{
			// [[BPR-1211]]. A promoted note is written through the review's
			// ledger, off the decision lane; its answer reaches the finding
			// host only while this window and that ledger remain.
			FindingHost = std::make_unique<application::agent_finding_host>(
				[this](const application::local_comment_record&                                                                  aNote,
					   std::function<void(std::expected<application::local_comment_record, application::local_comment_failure>)> aDone) {
					if (Closing || ReviewLedgerHost == nullptr) {
						return false;
					}
					ReviewLedgerHost->persist(aNote, std::nullopt, ledger_answer([this, done = std::move(aDone)](std::expected<application::local_comment_record, application::local_comment_failure> aStored) {
												  if (FindingHost != nullptr) {
													  done(std::move(aStored));
												  }
											  }));
					return true;
				},
				*FindingProjections);
		}
		// RFC-BPR-0012's chat *session*, under the same fixture and
		// production split every other agent host here uses. The turn it
		// produces is handed to `dispatch_agent_invocation` — the one
		// launch path in this file — so a chat send and a launch-panel
		// start are the same launch, the same protocol, and the same
		// frozen snapshot findings are delivered from.
		ChatProjections
			= std::make_unique<composition_agent_chat_projection>(
				[this](const application::agent_chat_projection& aChat) {
					present_side_chat(aChat);
				},
				[this](const application::agent_chat_outcome& aOutcome) {
					present_chat_outcome(aOutcome);
				},
				[this]() {
					if (SideChat != nullptr) {
						++SideChatPresentations;
						SideChat->present(qt::side_chat_projection{ });
					}
					if (Window != nullptr) {
						Window->clear_dif_context();
					}
				});
	#if defined(BPR_BUILD_TESTING)
		if (!DisableChatSessionHostComposition)
	#endif
		{
			ChatHost
				= std::make_unique<application::agent_chat_session_host>(
					agentProfiles,
					agentPolicies,
					core::codex_jsonl_profile_id,
					[this](const application::agent_chat_turn_request&
							   aTurn) -> bool {
	#if defined(BPR_BUILD_TESTING)
						LastChatTurn = aTurn;
	#endif
						// The reviewer's own words, from the send intent
						// the chat host validated, handed to the same
						// launch path a start gesture uses. This is the
						// only call site that passes a message.
						// [[BPR-2002]]. A question carries its subject and
						// the entry point the core decided for it; a turn
						// without one asks about the selection, as before.
						ChatQuestion    = aTurn.Question;
						ChatSkillText   = aTurn.SkillText;
						const auto sent = dispatch_agent_invocation(
							aTurn.Session.Profile,
							aTurn.IncludeNotes
								? all_private_note_ids()
								: std::vector<application::local_comment_id>{ },
							aTurn.IncludeDraft ? current_agent_draft_choice()
											   : std::nullopt,
							false,
							aTurn.Message,
							aTurn.Session,
							aTurn.Question                                                            ? aTurn.Question->EntryPoint
								: aTurn.EntryPoint == core::agent_entry_point::ask_about_conversation ? core::agent_entry_point::ask_about_conversation
																									  : core::agent_entry_point::ask_about_selection,
							// RFC-BPR-0118. Suggest the change is the
							// workshop's seventh operation.
							aTurn.Question ? aTurn.Question->Operation : std::nullopt);
						ChatQuestion.reset();
						ChatSkillText.reset();
						return sent;
					},
					[this]() -> application::agent_chat_private_prose {
						return current_chat_private_prose();
					},
					*ChatProjections);
		}
#endif
		Window->epoch_selectors()->set_dispatch(
			[this](application::epoch_intent aIntent) { dispatch_epoch_intent(std::move(aIntent)); });
		DiffProjections = std::make_unique<composition_diff_projection>(
			*Window->comparison_surface(),
			[this](const application::diff_publication& aPublication) {
				ReviewSourceMaterial.reset();
				AcceptedDiff = aPublication.Result && aPublication.Result->Alignment.Key == aPublication.Context.Key ? aPublication.Result : nullptr;
				if (aPublication.Result && aPublication.Result->Alignment.Key == aPublication.Context.Key) {
					ReviewSourceMaterial = application::review_mutation_source_material{
						.Key        = aPublication.Context.Key,
						.LeftLines  = aPublication.Result->LeftDocument ? std::optional{ aPublication.Result->Alignment.LeftLineCount } : std::nullopt,
						.RightLines = aPublication.Result->RightDocument ? std::optional{ aPublication.Result->Alignment.RightLineCount } : std::nullopt,
					};
				}

				if (SelectedDiffPublication.has_value()) {
					Window->set_conversation_endpoint_placement(
						base_numbered_as_github(),
						conversation_endpoint_is_current(
							SelectedDiffPublication->Request.Diff.Comparison,
							core::diff_side::right));
				}
				observe_base_numbering();
				refresh_review_conversations();
				// Before the ledger observation, because it is about the
				// panes rather than about the review, and because
				// `observe_selected_material` returns early for reasons
				// that have nothing to do with whether the comparison on
				// screen is coloured.
				adopt_published_classification(aPublication);
				observe_selected_material(aPublication);
				finish_tour_navigation();
				observe_tour_comparison();
				// [[BPR-2014]]. A comparison on screen is the epoch context the
				// accepted code's comparisons with a new head need.
				check_tour_material();
				correlate_defects();
				retain_reading_position();
				schedule_reading_restoration();
				schedule_tab_presentation();
				schedule_link_location();
			},
			[this](const application::diff_failure_publication& aFailure) {
				const auto current = SelectedDiffPublication && SelectedDiffPublication->Key == aFailure.Context.Key;
				if (current) {
					SelectedDiffRequest.reset();
				}
				if (ReadingRestoration) {
					ReadingFailed = true;
					schedule_reading_restoration();
				}
				// Leaving Discuss may cancel its window-pair diff while the
				// tab is already restoring another exact pair. That failure
				// belongs to the view left behind, not the requested tab.
				if (current && TabDriver
					&& TabDriver->position().File == SelectedDiffPublication->Request.File.File
					&& TabDriver->position().Comparison == SelectedDiffPublication->Request.Diff.Comparison) {
					TabFailed = true;
					schedule_tab_presentation();
				}
			});
#if defined(BPR_BUILD_TESTING)
		DiffBlobReads = std::make_unique<counting_diff_blob_port>(*EpochHost);
#endif
		DiffHost = std::make_unique<application::diff_application_host>(
			core::diff_state{
				.ApplicationSession = WindowSession,
			},
#if defined(BPR_BUILD_TESTING)
			*DiffBlobReads,
#else
			*EpochHost,
#endif
			DiffEffects,
			ReviewerDecisions,
			*DiffProjections,
			LineMatcher
#if defined(BPR_BUILD_TESTING)
			,
			application::diff_host_limits{ }, Profile->DiffComputeLimits
#endif
		);
		DiffHost->observe_work(Profile->InsightsRecorder);
		PaneFind = std::make_unique<pane_find_controller>(*Window->comparison_surface(), SourceRegex, ReviewerDecisions, DiffEffects, [this]() -> std::optional<core::source_search_scope> {
                    const auto* surface = Window->comparison_surface();
                    if(Closing || !SelectedDiffPublication || !surface->current_context() || !surface->current_result()
                        || SelectedDiffPublication->Key != surface->current_context()->Key){ return std::nullopt;
}
                    auto scope = core::source_search_scope{};
                    scope.Session=WindowSession;
                    scope.Authority=SelectedDiffPublication->Key.Authority;
                    scope.Comparison=SelectedDiffPublication->Request.Diff.Comparison;
                    scope.CatalogueGeneration=1U;
                    scope.Tab=TabHost ? TabHost->state().Active : std::nullopt;
                    return scope; }, [this] { EpochHost->yield_to_source_navigation(); });

		// The workbench observes the review's files through the read model
		// the provider already produces. The supplier is read at call time
		// rather than captured, because the accepted snapshot changes as
		// the reviewer opens pull requests.
		WorkbenchObservations = std::make_unique<
			application::snapshot_workbench_observation>(
			[this](const application::workbench_pull_request_authority& aAuthority)
				-> std::optional<
					std::vector<application::github_file_record>> {
				return WorkbenchReview.files(aAuthority);
			},
			[this](
				const application::workbench_pull_request_authority& aAuthority,
				const core::file_identity                            aFile)
				-> std::optional<core::actionability_input> {
				// RFC-BPR-0089. A filtered diff's receipt is its own.
				const auto filtered = WorkbenchReview.file_filter(aAuthority, aFile);
				// M3. A split file's diffs each have their own receipt; the
				// file stays in the walk until the reviewer leaves it.
				if (filtered && filtered->Split) {
					return std::nullopt;
				}
				const auto receiptFile = filtered && filtered->Endpoints ? core::filtered_receipt_file(aFile, *filtered->Endpoints) : aFile;
				const auto lock        = std::scoped_lock{ ReviewLedgerStateMutex };
				if (!ReviewLedgerAuthority.has_value()
					|| *ReviewLedgerAuthority != aAuthority
					|| !ReviewLedgerReceipt.has_value()
					|| ReviewLedgerReceipt->File != receiptFile) {
					return std::nullopt;
				}
				auto receipt = *ReviewLedgerReceipt;
				receipt.File = aFile;
				return core::actionability_input{
					.Receipt = receipt,
				};
			},
			[this](const application::workbench_pull_request_authority& aAuthority, const core::file_identity aFile) {
				return WorkbenchReview.file_filter(aAuthority, aFile);
			});

		// One port, two readers, exactly as the device projection below:
		// the recording sink is told first and unconditionally, so every
		// self-test marker reads what it read before a window was in the
		// picture, and the window is told the same thing afterwards.
		//
		// Composed on every platform, registered or not. The signed-out
		// mode being unavailable is a typed answer a reviewer must see,
		// not a silence, so the projection is bound even where the
		// authorities below are withheld.
		// Composed last, because its observation supplier receives accepted
		// snapshots from `Host`. The settings port is always present: either
		// the durable adapter opened or composition supplied session defaults.
		// The real window is the projection port, so success and failure both
		// terminate at the shipped surface rather than at an intermediate sink.
		if (Profile->WindowLayouts) {
			auto layout              = std::make_unique<profile_window_layout_port>(Profile->WindowLayouts, DraftWindow.Window);
			DurableWindowLayout      = layout.get();
			SessionWorkbenchSettings = std::move(layout);
		} else if (Profile->FirstWindow) {
			auto initial = application::workbench_layout_settings{ };
			if (const auto& accepted = Profile->WorkbenchPreferences->accepted(); accepted) {
				initial = application::combined_layout(accepted->NewWindowDefaults, accepted->Behaviour);
			}
			SessionWorkbenchSettings = std::make_unique<session_workbench_settings>(std::move(initial));
		}
		WritingHookSubscription = Profile->WorkbenchPreferences->subscribe(DraftWindow.Profile, [this](const auto& aPreferences) {
			if (!Closing) {
				// [[BPR-2051]]. An Enhance run keeps the prompt saved when it
				// was pressed; a later save does not stop it.
				if (PendingWriting && !PendingWriting->Enhance && PendingWriting->Request.Target.HookRevision != aPreferences.WritingHooks.Revision) {
					cancel_writing();
				}
				Window->writing_hooks()->present(aPreferences.WritingHooks);
				// [[BPR-1090]] The feedback hook's row says whether the label helper sends it.
				Window->writing_hooks()->present_label_helper(aPreferences.Behaviour.LabelHelper);
#if defined(BPR_AGENT_INTEGRATIONS)
				// [[BPR-1301]]. The next Claude launch reads what this
				// profile has just published, and so does the row.
				ClaudeWrapperSource->set(AgentSetup && AgentSetup->projection().Preferences.AgentsMoved ? std::string{ } : aPreferences.ClaudeWrapper);
				Window->claude_wrapper()->present(aPreferences.ClaudeWrapper);
				// [[BPR-1375]]. The next Claude request names this model.
				ClaudeModel = aPreferences.ClaudeModel;
				Window->claude_wrapper()->present_model(aPreferences.ClaudeModel);
				present_run_settings();
#endif
			}
		});
#if defined(BPR_AGENT_INTEGRATIONS)
		Window->claude_wrapper()->set_model_dispatch([this](std::string aModel) { return !Closing && Profile->WorkbenchPreferences->apply_claude_model(std::move(aModel)); });
		ClaudeWrapperEditor = std::make_unique<application::claude_wrapper_editor>(*Profile->WorkbenchPreferences, WorkbenchEffects, DecisionLane, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
		Window->claude_wrapper()->set_dispatch(
			[this](std::string aPath) {
				if (Closing || !ClaudeWrapperEditor) {
					return;
				}
				ClaudeWrapperEditor->choose(std::move(aPath), [this](const application::claude_wrapper_answer& aAnswer) {
					if (!Closing && Window != nullptr) {
						Window->claude_wrapper()->show_answer(aAnswer);
					}
				});
			},
			[this] {
				if (Closing || !ClaudeWrapperEditor) {
					return;
				}
				ClaudeWrapperEditor->locate([this](const std::optional<core::claude_search_candidate>& aFound) {
					if (!Closing && Window != nullptr) {
						Window->claude_wrapper()->show_found(aFound);
					}
				});
			});
		// [[BPR-1749]], RFC-BPR-0092 M2. The profile's agent setup:
		// its saved choice and programs, detection when setup shows (Q2) and
		// the bounded status check (Q3 A) on a launcher and lane of its own.
		AgentSetupStore    = adapters::sqlite::make_agent_setup_store(ApplicationDataRoot);
		AgentSetupLauncher = std::make_unique<adapters::agent::agent_process_launcher>();
		AgentSetupLane     = std::make_unique<worker_executor>();
		{
			auto dependencies  = application::agent_setup_dependencies{ };
			dependencies.Store = AgentSetupStore.get();
			{
				const auto file        = adapters::sqlite::agent_setup_store_file(ApplicationDataRoot).u8string();
				dependencies.StoreFile = std::string{ file.begin(), file.end() };
			}
			dependencies.Process  = AgentSetupLauncher.get();
			dependencies.Locate   = locate_agent_program();
			dependencies.Inspect  = &adapters::agent::inspect_claude_process_wrapper;
			dependencies.Programs = AgentProgramSource;
			dependencies.Parent   = [](const std::string& aName) -> std::optional<std::string> {
				if (!qEnvironmentVariableIsSet(aName.c_str())) {
					return std::nullopt;
				}
				return qEnvironmentVariable(aName.c_str()).toStdString();
			};
			dependencies.Profiles = core::built_in_agent_profiles();
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Checks   = AgentSetupLane.get();
			dependencies.Decision = &DecisionLane;
			dependencies.Publish  = [this](const application::agent_setup_projection& aProjection) { present_agent_setup(aProjection); };
			dependencies.Log      = DiagnosticLog; // [[BPR-1828]]
	#if defined(BPR_BUILD_TESTING)
			// [[BPR-2177]], look ask-v003 (owner, 2026-11-14: "The footer does
			// not say fixture agent"): the setup, Check and Settings name it.
			if (FixtureAgentSession) {
				dependencies.Names = { { core::setup_agent::claude_code, "Fixture agent" }, { core::setup_agent::codex, "Fixture agent (Codex)" } };
			}
	#endif
			AgentSetup = std::make_unique<application::agent_setup_host>(std::move(dependencies));
		}
		AgentSetup->load();
		// [[BPR-1750]], RFC-BPR-0092 D2. The profile's clone folders
		// and each repository's remembered clone, read by metadata alone.
		CloneLinkStore = adapters::sqlite::make_clone_link_store(ApplicationDataRoot);
		{
			auto dependencies           = application::clone_link_dependencies{ };
			dependencies.Store          = CloneLinkStore.get();
			dependencies.Reads.Scan     = [](const std::span<const std::string> aFolders, std::stop_token aStop) { return adapters::workspace::scan_clone_folders(aFolders, std::move(aStop)); };
			dependencies.Reads.Read     = [](const std::string& aPath) { return adapters::workspace::read_clone(aPath); };
			dependencies.Reads.Existing = [](const std::span<const std::string> aFolders) { return adapters::workspace::existing_folders(aFolders); };
			dependencies.Home           = QDir::toNativeSeparators(QDir::homePath()).toStdString();
			dependencies.Log            = DiagnosticLog; // [[BPR-1831]]
	#if defined(Q_OS_WIN)
			dependencies.Windows = true;
	#endif
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			dependencies.Publish  = [this](const application::clone_link_projection& aProjection) { present_clone_links(aProjection); };
			// [[BPR-1879]]. The clones BetterPullReview keeps live in the
			// profile beside its repository caches, shown in Storage and
			// removed by profile purge.
			{
				std::error_code canonicalError;
				const auto      root = std::filesystem::weakly_canonical(ApplicationDataRoot, canonicalError);
				if (!canonicalError) {
					dependencies.ManagedRoot = (root / std::string{ application::managed_clone_folder }).string();
				}
			}
			dependencies.Keep = [this](const application::clone_repository& aRepository, const std::string& aDestination) -> application::clone_link_dependencies::keep_job {
				return prepare_kept_clone(aRepository, aDestination);
			};
			CloneLinks = std::make_unique<application::clone_link_host>(std::move(dependencies));
		}
		CloneLinks->load();
		observe_clone_repository();
		// [[BPR-1752]], RFC-BPR-0092 D1. The review skill: the
		// built-in default until the profile edits it.
		ReviewSkillStore = adapters::sqlite::make_review_skill_store(ApplicationDataRoot);
		{
			auto dependencies     = application::review_skill_dependencies{ };
			dependencies.Store    = ReviewSkillStore.get();
			dependencies.BuiltIn  = std::string{ application::built_in_review_skill() };
			dependencies.SaveCopy = [](const std::string& aText) { return save_review_skill_copy(aText); };
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			dependencies.Publish  = [this](const application::review_skill_projection& aProjection) {
				if (!Closing && Window != nullptr) {
					Window->review_skill()->present(aProjection, std::string{ application::built_in_review_skill() });
					Window->writing_hooks()->present_review_skill_changed(Window->review_skill()->changed());
				}
			};
			ReviewSkill = std::make_unique<application::review_skill_host>(std::move(dependencies));
		}
		ReviewSkill->load();
		// [[BPR-2016]], RFC-BPR-0107. Import and export of agents and
		// prompts: the file and the stores on the effect lane, no process.
		AgentConfigurationStore = adapters::sqlite::make_agent_configuration_store(ApplicationDataRoot);
		{
			auto dependencies  = application::agent_configuration_dependencies{ };
			dependencies.Store = AgentConfigurationStore.get();
			// [[BPR-2283]]. A regular file only: a FIFO or a folder dropped or
			// chosen is refused, never waited on.
			dependencies.ReadFile  = [](const std::string& aPath, const std::size_t aLimit) { return read_bounded_regular_file(aPath, aLimit + 1U); };
			dependencies.WriteFile = [](const std::string& aPath, const std::string& aBytes) {
				auto file = QSaveFile{ QString::fromStdString(aPath) };
				return file.open(QIODevice::WriteOnly) && file.write(aBytes.data(), static_cast<qint64>(aBytes.size())) == static_cast<qint64>(aBytes.size()) && file.commit();
			};
			dependencies.MintIdentity = [] {
				auto           random   = std::random_device{ };
				auto           identity = std::string{ };
				constexpr auto digits   = std::string_view{ "0123456789abcdef" };
				for (auto index = 0U; index < 32U; ++index) {
					identity += digits[random() & 15U];
				}
				return identity;
			};
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			AgentConfiguration    = std::make_unique<application::agent_configuration_host>(std::move(dependencies));
		}
		// [[BPR-1753]], owner decision (b) 2026-10-23. One
		// conversation per pull request, kept with the setup.
		AgentConversationStore = adapters::sqlite::make_agent_conversation_store(ApplicationDataRoot);
		{
			auto dependencies     = application::agent_conversation_dependencies{ };
			dependencies.Store    = AgentConversationStore.get();
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			dependencies.Publish  = [this](const application::agent_conversation_projection& aView) { present_conversation(aView); };
			AgentConversation     = std::make_unique<application::agent_conversation_host>(std::move(dependencies));
		}
		// Owner report 2026-11-10, after BPR-2052. A stored answer's finding
		// cards come back after a restart as they were left.
		AgentFindingCardStore = adapters::sqlite::make_agent_finding_card_store(ApplicationDataRoot);
		{
			auto dependencies     = application::agent_finding_card_dependencies{ };
			dependencies.Store    = AgentFindingCardStore.get();
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			dependencies.Loaded   = [this] { present_remembered_finding_cards(); };
			AgentFindingCards     = std::make_unique<application::agent_finding_card_host>(std::move(dependencies));
		}
		AgentFindingCards->load();
		// RFC-BPR-0125 (AC07). A suggested prompt's turn keeps its name past
		// a restart.
		PromptTurnNameStore = adapters::sqlite::make_prompt_turn_name_store(ApplicationDataRoot);
		{
			auto dependencies     = application::prompt_turn_name_dependencies{ };
			dependencies.Store    = PromptTurnNameStore.get();
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			dependencies.Loaded   = [this] { present_remembered_turn_names(); };
			PromptTurnNames       = std::make_unique<application::prompt_turn_name_host>(std::move(dependencies));
		}
		PromptTurnNames->load();
		// RFC-BPR-0125 milestone 3 ([[BPR-2146]]), RFC-BPR-0128. The
		// profile's suggested prompts: read at start and whenever the window
		// comes back to the front, so another window's edit reaches it.
		PromptMacroStore = adapters::sqlite::make_prompt_macro_store(ApplicationDataRoot);
		{
			auto dependencies     = application::prompt_macro_dependencies{ };
			dependencies.Store    = PromptMacroStore.get();
			dependencies.Effects  = &WorkbenchEffects;
			dependencies.Decision = &DecisionLane;
			dependencies.Changed  = [this](const application::prompt_macro_projection& aProjection) {
				if (!Closing) {
					present_prompt_macros(aProjection);
				}
	#if defined(BPR_BUILD_TESTING)
				// [[BPR-2179]] look prompts-v002: a fresh profile starts with
				// two prompts of the reviewer's own whose conditions nest.
				if (!Closing && PromptExamplesLook && !PromptExamplesSeeded && aProjection.Loaded && aProjection.Readable && aProjection.Revision == 0U) {
					PromptExamplesSeeded = true;
					using core::prompt_fact;
					using core::prompt_match;
					auto again            = core::prompt_macro{ .Id = "own.example-look-again", .Place = core::prompt_place::agent_tab };
					again.Body.Name       = "Ready to look again?";
					again.Body.Template   = "Has {pr.title} changed enough since my review that I should look again?";
					again.Body.Conditions = { { .Fact = prompt_fact::pushes_since_review } };
					again.Body.Groups     = { { .Match = prompt_match::none, .Conditions = { { .Fact = prompt_fact::draft } } },
											  { .Match = prompt_match::any, .Conditions = { { .Fact = prompt_fact::checks_failing }, { .Fact = prompt_fact::author_is_me, .Negated = true } } } };
					auto open             = core::prompt_macro{ .Id = "own.example-still-open", .Place = core::prompt_place::conversation };
					open.Body.Name        = "Still open after the push?";
					open.Body.Template    = "Is the concern in this thread still open after the latest push?";
					open.Body.Subject     = core::prompt_subject::conversation;
					open.Body.Conditions  = { { .Fact = prompt_fact::thread_unresolved } };
					open.Body.Groups      = { { .Match = prompt_match::any, .Conditions = { { .Fact = prompt_fact::replies_since_mine }, { .Fact = prompt_fact::thread_activity, .Activity = core::prompt_activity_value::pushes } } } };
					auto macros           = aProjection.Set.macros();
					auto surfaces         = aProjection.Set.Surfaces;
					macros.push_back(again);
					macros.push_back(open);
					for (const auto& [id, surface] : { std::pair{ again.Id, core::prompt_surface::pull_request_menu }, std::pair{ again.Id, core::prompt_surface::pull_request_chips },
													   std::pair{ open.Id, core::prompt_surface::thread_menu }, std::pair{ open.Id, core::prompt_surface::thread_chips } }) {
						static_cast<void>(core::place(surfaces, macros, id, surface));
					}
					auto document     = core::prompt_edit_document_of(aProjection.Set);
					document.Library  = std::move(macros);
					document.Surfaces = std::move(surfaces);
					PromptMacros->save(core::prompt_macro_differences_of(core::shipped_prompt_macros(), document), aProjection.Revision);
				}
	#endif
			};
			PromptMacros = std::make_unique<application::prompt_macro_host>(std::move(dependencies));
		}
		PromptMacros->load();
		// [[BPR-2179]], [[BPR-2185]]. Settings › Prompts sends the save its
		// core editor made: the differences and the revision they came from.
		// The host writes them if the store is still there and says how it
		// ended before it reads the store back; the host mints identities.
		Window->prompts()->set_save_dispatch([this](core::prompt_save_request aRequest) {
			if (!Closing && PromptMacros) {
				PromptMacros->save(std::move(aRequest.Differences), aRequest.From, [this](const application::prompt_macro_write aWrite) {
					if (!Closing && Window != nullptr) {
						Window->prompts()->settle_save(aWrite);
					}
				});
			}
		});
		Window->prompts()->set_identity_source([this] { return PromptMacros ? PromptMacros->mint_identity() : std::string{ }; });
		// [[BPR-2219]]. What a review prompt is always sent with, shown
		// read-only under its editor: the report format the answer is read by.
		Window->prompts()->set_review_result(QString::fromStdString(application::report_templates_guidance().Text));
		QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, Window.get(), [this](QWindow* const aFocus) {
			if (!Closing && PromptMacros && Window && aFocus != nullptr && aFocus == Window->windowHandle()) {
				PromptMacros->load();
			}
		});
		compose_agent_setup_view();
		observe_conversation();
#endif
		Window->writing_hooks()->set_dispatch(
			[this](core::writing_hook aHook, std::string aText) {
				if (Closing || !Profile->WorkbenchPreferences->apply_writing_hook(aHook, aText)) {
					return false;
				}
#if defined(BPR_AGENT_INTEGRATIONS)
				// [[BPR-2016]]. A saved override was written against today's built-in.
				if (AgentConfiguration) {
					AgentConfiguration->record_written_against(aHook, std::move(aText));
				}
#endif
				return true;
			},
			[this](core::writing_hook aHook) { test_writing_hook(aHook); });
		compose_skill_catalog();
		Window->tour_editor()->set_writing_dispatch(
			[this](core::writing_hook aHook, qt::prose_capture aCaptured) {
				if (local_tour_shown()) {
					DraftDesktop->tour_writing(aHook);
					return;
				}
				request_writing(aHook, std::move(aCaptured));
			},
			[this] {
				if (local_tour_shown()) {
					DraftDesktop->writing_stop();
					return;
				}
				cancel_writing();
			});
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1900]]. The tour session's gestures: Draft the tour, Stop,
		// and each proposed field's or stop's Accept and Discard.
		Window->tour_editor()->side_chat()->set_continuation_dispatch([this](const core::tour_thread& aThread) {
			if (!local_tour_shown()) {
				choose_tour_continuation(aThread);
			}
		});
		Window->tour_editor()->side_chat()->set_history_dispatch([this](const core::tour_thread& aThread, std::optional<std::string> aChoice) {
			if (!local_tour_shown()) {
				observe_tour_native_history(aThread, std::move(aChoice));
			}
		});
		Window->tour_editor()->set_tour_ask_dispatch([this](const qt::tour_ask& aAsk) {
			// [[BPR-2235]]. A prompt sent as offered keeps its name, as an
			// Agent tab turn does.
			if (!aAsk.Name.isEmpty() && PromptTurnNames) {
				PromptTurnNames->remember({ .Asked = aAsk.Question.toStdString(), .Name = aAsk.Name.toStdString() });
			}
			// RFC-BPR-0126 ([[BPR-2154]]). A follow-up typed while the agent
			// runs, or while the part's queue holds messages, is queued on
			// the part's own session (a pull request's tour or a local draft's).
			if (aAsk.Steer) {
				queue_chat_follow_up(aAsk);
				return;
			}
			if (local_tour_shown()) {
				DraftDesktop->tour_ask(aAsk);
				return;
			}
			TourAskFresh     = aAsk.Fresh;
			TourAskSkillText = aAsk.SkillText;
			request_tour_ask(aAsk.Thread, aAsk.Passage, aAsk.Question, aAsk.Update);
			TourAskFresh = false;
			TourAskSkillText.reset();
		});
		// [[BPR-1930]], [[BPR-1931]]. The focus chip's ×, Reset tour and its Undo.
		Window->tour_editor()->set_tour_shape_dispatch(
			[this] {
				if (!local_tour_shown()) {
					clear_tour_focus();
				}
			},
			[this] {
				if (!local_tour_shown()) {
					reset_tour();
				}
			},
			[this] {
				if (!local_tour_shown()) {
					undo_tour_reset();
				}
			});
		Window->tour_editor()->side_chat()->set_focus_undo_dispatch([this](const core::tour_agent_run_id aRun) {
			if (!local_tour_shown()) {
				undo_tour_focus(aRun);
			}
		});
		// [[BPR-1941]]. Delete on a chat turn that ended without an answer.
		Window->tour_editor()->side_chat()->set_delete_dispatch([this](const core::tour_agent_run_id aRun) {
			if (local_tour_shown()) {
				return;
			}
			if (core::remove_tour_run(TourAgent, aRun)) {
				present_tour_session();
			}
		});
		Window->tour_editor()->set_tour_session_dispatch(
			[this] {
				if (local_tour_shown()) {
					// Draft the tour, for a local draft: a whole-change question
					// whose proposals wait in the tour.
					DraftDesktop->tour_ask({ .Question = QStringLiteral("Draft the tour: propose stops on this candidate's lines.") });
					return;
				}
				request_tour_draft();
			},
			[this] {
				if (local_tour_shown()) {
					DraftDesktop->tour_stop();
					return;
				}
				cancel_tour_run();
			},
			[this](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) {
				if (local_tour_shown()) {
					DraftDesktop->tour_decide(aIndex, aDecision, aText);
					return;
				}
				decide_tour_proposal(aIndex, aDecision, aText);
			},
			[this](const std::size_t aIndex) {
				if (!local_tour_shown()) {
					relocate_tour_proposal(aIndex);
				}
			});
		Window->tour_editor()->set_tour_ask_more_dispatch([this](const core::tour_agent_run_id aRun) {
			// A local draft's turns replay or resume their conversation; none is fresh.
			if (!local_tour_shown()) {
				ask_more_from(aRun);
			}
		});
#endif
		WorkbenchHost = std::make_unique<
			application::workbench_application_host>(
			core::workbench_interaction_state{ },
			*WorkbenchObservations,
			SessionWorkbenchSettings ? *SessionWorkbenchSettings : *WorkbenchSettings,
			WorkbenchEffects,
			ReviewerDecisions,
			*this,
			WindowSession,
			application::workbench_host_limits{ },
			EpochHost.get(),
			this,
			Profile->WorkbenchPreferences.get(),
			aConfig.Profile);
		// [[BPR-1132]]. The window's document tabs. Their gestures reach the
		// tab host; its loads are presented through the hosts above; its
		// collection is kept with the window's accepted destination.
		TabPorts = std::make_unique<document_tab_ports>(*this);
		TabHost  = std::make_unique<application::document_tab_host>(
			DecisionLane, *TabPorts, *TabPorts,
			[this](const core::window_review_tabs& aTabs) { retain_tabs(aTabs); }, DiagnosticLog);
		PrFind = std::make_unique<pr_search_controller>(*Window->pr_search(), *Window->comparison_surface(), *EpochHost, SourceRegex, ReviewerDecisions, Profile->SearchEffects, [this] { return pr_search_scope(); }, [this](const core::source_search_scope& aScope, const application::source_corpus_entry& aEntry, const pr_search_controller::result_tab aTab) -> std::optional<core::document_tab_id> {
                if(pr_search_scope()!=std::optional{aScope}){return {};}
                cancel_review_navigation();cancel_restoration(false);abandon_tab_load();
                SearchOpenFile=std::pair{aScope,aEntry};static_cast<void>(register_search_open());
                observe_tab_reading();
                const auto before=TabHost->state().Active;
                SearchOpening=true;SearchOpeningBinding=aScope.Tab;
                if(aTab==pr_search_controller::result_tab::new_tab){open_search_result_in_new_tab(aEntry.File);}else{TabHost->apply(core::open_in_active_tab_intent{.File=aEntry.File});}
                if(TabHost->state().Active && TabHost->state().Active!=before){SearchCreatedTab=TabHost->state().Active;SearchCreatedComparison=aScope.Comparison;SearchCreatedBinding=aScope.Tab;}
                SearchOpening=false;return TabHost->state().Active; }, [this](const core::source_search_scope& aScope, core::file_identity aFile, const std::string& aPath, const core::source_match_location& aLocation) {
                if(!SelectedDiffPublication || SelectedDiffPublication->Request.Diff.Authority!=aScope.Authority || SelectedDiffPublication->Request.Diff.Comparison!=aScope.Comparison || SelectedDiffPublication->Key.File!=aFile){return false;}
                const auto& endpoint=aLocation.Side==core::diff_side::left?SelectedDiffPublication->LeftEndpoint:SelectedDiffPublication->RightEndpoint;
                if(!endpoint || endpoint->blob()!=aLocation.Blob || endpoint->side()!=aLocation.Side){return false;}
                const auto path=endpoint->path().generic_u8string();return std::string_view{reinterpret_cast<const char*>(path.data()),path.size()}==aPath; }, [this] { return TabHost ? TabHost->state().Active : std::nullopt; });
		Window->set_pr_search_dispatch([this](auto aGesture) {
			// A local draft searches its own candidate through the same panel.
			if (DraftDesktop && Window->showing_local_draft() && !Closing) {
				DraftDesktop->search(static_cast<int>(aGesture));
				return;
			}
			if (!PrFind || Closing) {
				return;
			}
			if (aGesture == qt::source_find_gesture::document_changed) {
				PrFind->open();
			} else if (aGesture == qt::source_find_gesture::next || aGesture == qt::source_find_gesture::previous) {
				PrFind->navigate(aGesture == qt::source_find_gesture::next ? core::change_navigation_direction::next : core::change_navigation_direction::previous);
			} else {
				PrFind->dispatch(aGesture);
			}
		});
		Window->set_local_draft_observer([this](const bool aShown) {
			if (Closing) {
				return;
			}
			if (aShown && DraftDesktop) {
				DraftDesktop->attach_search();
				DraftDesktop->present_tour();
			} else if (!aShown) {
				if (PrFind) {
					PrFind->attach();
					PrFind->observe_scope();
				}
				restore_tour_panel();
			}
		});
		Window->set_document_tab_dispatch([this](const core::document_tab_intent& aIntent) {
			if (Closing || TabHost == nullptr) {
				return;
			}
			cancel_review_navigation();
			if (PrFind) {
				PrFind->cancel_reveal();
			}
			SearchCreatedTab.reset();
			SearchCreatedComparison.reset();
			observe_tab_reading();
			DecisionLane.post([this, weak = std::weak_ptr<const int>{ WindowLifetime }, intent = aIntent] {
				if (weak.expired() || Closing || TabHost == nullptr) {
					return;
				}
				cancel_restoration(false, true);
				TabHost->apply(intent);
				// A background open or closing an inactive tab must leave
				// the active load running. The core names any replacement.
				if (TabLoad && TabHost->state().Pending != TabLoad) {
					abandon_tab_load();
				}
				if (!TabHost->state().Active) {
					ObservedTabFile = WorkbenchHost->selected();
					clear_diff_selection();
				}
			});
		});
		// Reads the current durable or session layout. Restoring never writes
		// it back, so merely launching still leaves a fresh store fresh.
		// [[BPR-0742]]. Session defaults over a store that could not open load
		// successfully, so the host never reports the loss. The window is told
		// once and retains it; a one-shot failure would be cleared by its first
		// projection.
		Window->set_saved_layout_unavailable(!Profile->WorkbenchPersistenceAvailable);

		Window->set_notification_preview_dispatch([this](const core::pull_request_ref& aRef) {
			if (Closing || QueueHost == nullptr || Notifications == nullptr) {
				return false;
			}
			const auto queue = QueueHost->queue();
			const auto item  = std::ranges::find(queue, aRef, &core::queue_item::Ref);
			if (item == queue.end() || !core::permits_notification(QueueHost->preferences().Notifications, item->Bucket)) {
				return false;
			}
			if (!QueueActivity || QueueActivity->Ref != aRef) {
				return false;
			}
			// [[BPR-2330]]. A draft waiting quietly in Following previews nothing.
			const auto drafts  = Profile->WorkbenchPreferences && Profile->WorkbenchPreferences->accepted() ? Profile->WorkbenchPreferences->accepted()->Behaviour.OthersDrafts : core::home_draft_placement::following;
			const auto request = application::notification_for(*item, QueueActivity->Activity.UnseenComments, drafts);
			return request && Notifications->deliver(*request);
		});

		// Composed before the appearance host, which needs a live
		// classification port. The blob source reads through the epoch
		// host rather than opening a second Git cache session.
		SyntaxBlobs          = std::make_unique<composition_blob_source>(*EpochHost);
		SyntaxClassification = std::make_unique<
			adapters::syntax::packaged_syntax_classifier>(
			*SyntaxDefinitions,
			*SyntaxBlobs);
		// [[BPR-1562]]. The readers' fenced code, classified by the same
		// definitions on the same effect lane through a host of its own.
		MarkdownCodeBlobs      = std::make_unique<markdown_code_blob_source>(MarkdownCodeTexts);
		MarkdownCodeClassifier = std::make_unique<adapters::syntax::packaged_syntax_classifier>(*SyntaxDefinitions, *MarkdownCodeBlobs);
		MarkdownCode           = std::make_unique<application::markdown_code_syntax_host>(
			*MarkdownCodeClassifier, MarkdownCodeTexts, AppearanceEffects, DecisionLane,
			[this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
				auto definition = SyntaxDefinitions->definition_for_path(aSelector);
				if (definition.Name == adapters::syntax::plain_text_definition_name) {
					return std::nullopt;
				}
				return definition;
			},
			[this](const application::markdown_code_syntax_publication& aSpans) {
				if (!Closing && Window != nullptr) {
					Window->apply_markdown_code_syntax(aSpans);
				}
			});
		MarkdownCode->observe_work(Profile->InsightsRecorder);
		Window->set_markdown_code_dispatch([this](std::vector<application::markdown_code_syntax_request> aRequests) {
			if (!Closing && MarkdownCode) {
				MarkdownCode->request(aRequests);
			}
		});
		// [[BPR-1802]]. Permalinks in the accepted repository show their
		// lines, read from the review's objects at their commit.
		MarkdownSnippetSource = std::make_unique<permalink_snippet_source>(
			[this](const application::markdown_blob_source_read& aRead, std::function<void(application::markdown_blob_source_result)> aCompletion) {
				const auto commit = core::parse_git_commit_oid(aRead.Permalink.Commit);
				if (Closing || EpochHost == nullptr || !LastAcceptedHandoff || !commit) {
					return false;
				}
				auto request               = application::permalink_source_request{ };
				request.WorkbenchAuthority = workbench_authority(*LastAcceptedHandoff);
				request.Head               = LastAcceptedHandoff->Head;
				request.Commit             = *commit;
				request.Path               = aRead.Permalink.Path;
				return EpochHost->resolve_permalink_source(std::move(request), [completion = std::move(aCompletion)](const application::permalink_source_observation& aObservation) {
					using status  = application::permalink_source_status;
					auto result   = application::markdown_blob_source_result{ };
					result.Status = aObservation.Status == status::resolved ? (aObservation.Bytes ? application::markdown_blob_source_status::read : application::markdown_blob_source_status::absent)
						: aObservation.Status == status::too_large          ? application::markdown_blob_source_status::refused
						: aObservation.Status == status::cancelled          ? application::markdown_blob_source_status::cancelled
																			: application::markdown_blob_source_status::unavailable;
					result.Bytes  = aObservation.Bytes.value_or(std::string{ });
					completion(std::move(result));
				});
			});
		MarkdownSnippets = std::make_unique<application::markdown_blob_snippet_host>(
			*MarkdownSnippetSource, DecisionLane, [this] { return permalink_snippet_scope(); },
			[this](const application::markdown_blob_snippet_publication& aSnippet) {
				if (!Closing && Window != nullptr) {
					Window->apply_markdown_permalink_snippet(aSnippet);
				}
			});
		Window->set_markdown_permalink_dispatch([this](std::vector<application::markdown_blob_snippet_request> aRequests) {
			if (!Closing && MarkdownSnippets) {
				MarkdownSnippets->request(aRequests);
			}
		});
		Window->set_tour_permalink_source([this](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const bool aBefore) { return Closing ? std::nullopt : tour_permalink(aPath, aFirst, aLast, aBefore); });

		AppearanceProjections.set_span_sink(
			[this](const application::token_span_publication& aSpans) {
				adopt_classification(aSpans);
			});
		AppearanceProjections.set_presentation_sink(
			[this](const application::appearance_presentation_invalidation& aInvalidation) {
				adopt_appearance_settings();
				// [[BPR-1714]]. The known namespaces are a classification
				// input: the C++ on screen asks again under the new list.
				if (aInvalidation.Namespaces.has_value()) {
					reclassify_shown_comparison();
				}
			});
		AppearanceProjections.set_palette_adoption_sink(
			[this](const application::palette_adoption_publication&) {
				adopt_appearance_settings(true);
			});
		AppearanceProjections.set_theme_export_sink([this] {
			if (!Closing && Window != nullptr && Window->appearance_settings() != nullptr) {
				Window->appearance_settings()->report_theme_export(true);
			}
		});
		AppearanceProjections.set_refusal_sink(
			[this](const application::appearance_failure_publication&
					   aFailure) { report_appearance_refusal(aFailure); });
		AppearanceHost = std::make_unique<
			application::appearance_application_host>(
			Profile->AppearancePreferences,
			*SyntaxClassification,
			AppearanceEffects,
			DecisionLane,
			AppearanceProjections,
			WindowSession);
		AppearanceHost->observe_work(Profile->InsightsRecorder);
		PaneFind->observe_work(Profile->InsightsRecorder);
		PrFind->observe_work(Profile->InsightsRecorder);
		PrFind->observe_diagnostics(Profile->OperationAggregate); // [[BPR-1831]]
		// Loads the durable record. An absent record keeps the composed
		// settings and is never written back, so a fresh installation
		// stays a fresh installation until the reviewer chooses
		// something.

		// The Qt adapter is the presentation authority for appearance;
		// the host owns the durable record and the span cache. The
		// profile is therefore built *from* the host's settings after
		// restore, so a stored mode is the one presentation starts on
		// rather than one it is corrected to a frame later.
		AppearanceProfile = std::make_unique<qt::appearance_profile>(
			AppearanceHost->settings(),
			native_appearance_observation());
		AppearanceProfile->set_sink([this] { apply_appearance(); });

		// The native scheme is an *observation*, not a setting. This is
		// the inversion the whole slice exists for: before it, the
		// operating system decided presentation directly and an
		// explicit mode had nothing to ignore. Now the system reports
		// and `core::resolve_appearance` decides.
		if (auto* const hints = QGuiApplication::styleHints();
			hints != nullptr) {
			QObject::connect(
				hints,
				&QStyleHints::colorSchemeChanged,
				Window.get(),
				[this] { observe_native_appearance(); });
		}
		// [[BPR-0645]]. The scheme read above is taken before the event
		// loop has run, and on macOS the platform theme can still answer
		// `Unknown` at that point; `colorSchemeChanged` reports later
		// changes only, never the first real answer. One deferred
		// re-observation after startup asks again once the theme is
		// settled, so a dark desktop is not rendered as light for the
		// whole session.
		QTimer::singleShot(0, Window.get(), [this] {
			observe_native_appearance();
		});
		// The settings surface dispatches into the host, never into
		// presentation. That is what makes the host the single
		// authority on what is in effect: a gesture that reached the
		// profile directly would change the screen without changing
		// the record, and the next launch would silently disagree.
		if (auto* const panel = Window->appearance_settings();
			panel != nullptr) {
			if (!AppearancePersistenceAvailable) {
				panel->report_persistence_unavailable();
			}
			panel->set_dispatch(
				[this](application::appearance_intent aIntent) {
					dispatch_appearance_intent(std::move(aIntent));
				});
		}
		// [[BPR-1336]]. View's zoom commands, keys and gestures step the
		// profile's zoom through the same host as Settings.
		Window->set_code_zoom_dispatch([this](const core::code_zoom_step aStep) {
			dispatch_appearance_intent(application::step_code_zoom_intent{ .Step = aStep });
		});
		if (aConfig.MotionObserver) {
			const auto weak    = std::weak_ptr<const int>{ WindowLifetime };
			MotionSubscription = aConfig.MotionObserver->subscribe(
				*Window, [this, weak](const core::motion_observation aObservation) {
					if (weak.expired() || Closing || !AppearanceHost) {
						return;
					}
					AppearanceHost->observe_motion(aObservation);
					adopt_appearance_settings();
				});
		}
		// Shows what restore() loaded, so the surface opens on the
		// settings actually in effect rather than on its own defaults.
		adopt_appearance_settings();
		apply_appearance();
	}

	github_application::implementation::~implementation()
	{
		WindowLifetime.reset();
		if (MotionSubscription != 0U && Profile->Config.MotionObserver) {
			Profile->Config.MotionObserver->unsubscribe(MotionSubscription);
		}
	}
}
