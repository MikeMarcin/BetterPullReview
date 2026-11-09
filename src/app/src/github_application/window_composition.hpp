#ifndef BPR_APP_GITHUB_APPLICATION_WINDOW_COMPOSITION_HPP
#define BPR_APP_GITHUB_APPLICATION_WINDOW_COMPOSITION_HPP

#if defined(_WIN32)
	#include "windows_title_row_host.hpp"
#endif

#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/app/github_application.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/cached_review_source_port.hpp"
#include "bpr/application/commit_filter_model.hpp"
#include "bpr/application/commit_filter_projection.hpp"
#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/device_binding_store.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/document_tab_host.hpp"
#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/home_open_host.hpp"
#include "bpr/application/insights_observer.hpp"
#include "bpr/application/markdown_blob_snippet_host.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/application/off_hunk_fact_port.hpp"
#include "bpr/application/overview_feedback.hpp"
#include "bpr/application/private_review_application_host.hpp"
#include "bpr/application/pull_request_overview.hpp"
#include "bpr/application/review_comment_placement.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_submission_contract.hpp"
#include "bpr/application/review_submission_port.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/review_tour_store.hpp"
#include "bpr/application/setup_destination.hpp"
#include "bpr/application/setup_destination_projection.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/application/skill_catalog.hpp"
#include "bpr/application/source_corpus_port.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/commit_filter.hpp"
#include "bpr/core/comparison_preparation.hpp"
#include "bpr/core/conversation_mutation_lifecycle.hpp"
#include "bpr/core/conversation_reactions.hpp"
#include "bpr/core/conversation_reply.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/device_authorization_model.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/diff_transition.hpp"
#include "bpr/core/document_tabs.hpp"
#include "bpr/core/draft_state.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/markdown_navigation.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/pull_request_location.hpp"
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/core/review_ledger.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/core/setup_destination.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/source_matcher.hpp"
#include "bpr/core/source_search.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/core/window_review_restoration.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/reviewer_picker.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "gh_cli_transport.hpp"
#include "github_application/agent_composition.hpp"
#include "github_application/blob_sources.hpp"
#include "github_application/defect_identity.hpp"
#include "github_application/profile_runtime.hpp"
#include "github_application/review_mutation_selection.hpp"
#include "github_application/window_probe_states.hpp"
#include "github_application/window_review_lanes.hpp"
#include "github_application/workbench_composition.hpp"
#include "github_host_profile.hpp"
#include "pane_find_controller.hpp"
#include "pr_search_controller.hpp"
#include "private_review_workflow.hpp"
#include "profile_window_layout_port.hpp"
#include "review_comment_permalink.hpp"
#include "review_cycle_desktop.hpp"
#include "review_submission_workflow.hpp"

#include <QString>
#include <QTimer>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/agent/agent_process_launcher.hpp"
	#include "bpr/application/agent_chat_session_host.hpp"
	#include "bpr/application/agent_comment_workshop_host.hpp"
	#include "bpr/application/agent_conversation.hpp"
	#include "bpr/application/agent_conversation_host.hpp"
	#include "bpr/application/agent_finding_host.hpp"
	#include "bpr/application/agent_invocation_host.hpp"
	#include "bpr/application/agent_program_resolution.hpp"
	#include "bpr/application/agent_realignment.hpp"
	#include "bpr/application/agent_run_diagnosis.hpp"
	#include "bpr/application/agent_setup_host.hpp"
	#include "bpr/application/agent_setup_preferences.hpp"
	#include "bpr/application/agent_workspace.hpp"
	#include "bpr/application/claude_process_wrapper.hpp"
	#include "bpr/application/clone_link_host.hpp"
	#include "bpr/application/mcp_context_helper_supervisor.hpp"
	#include "bpr/application/review_skill_host.hpp"
	#include "bpr/application/tour_agent_context.hpp"
	#include "bpr/core/agent_chat_scope.hpp"
	#include "bpr/core/agent_finding.hpp"
	#include "bpr/core/agent_setup.hpp"
	#include "bpr/core/tour_agent_session.hpp"
	#include "bpr/qt/agent_launch_panel.hpp"
	#include "bpr/qt/agent_tab_view.hpp"
	#include "bpr/qt/finding_queue_view.hpp"
	#include "bpr/qt/side_chat_panel.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/github_window_read_host.hpp"
#endif

// One window's composition (github_application::implementation): its
// hosts, providers, executors, stores and effects, and what it presents.
namespace bpr::app {
	class github_application::implementation final
		: public application::github_epoch_handoff_port,
		  public application::cached_review_source_port,
		  public application::epoch_projection_port,
		  public application::workbench_selected_diff_projection_port,
		  public application::workbench_projection_port,
		  public application::review_mutation_projection_port,
		  public application::review_mutation_source_port,
		  public application::receipt_projection_port,
		  public application::queue_landing_projection_port,
		  public application::agent_session_catalog_port,
		  public application::setup_destination_port,
		  public application::shared_draft_context_port
	{
		friend class github_application;
		std::shared_ptr<github_profile_runtime> Profile;
		// Declared first so fixture cleanup runs after every private service releases its root.
		decltype(github_profile_runtime::StartupScratch)& StartupScratch = Profile->StartupScratch;
		decltype(github_profile_runtime::RegistryRoot)&   RegistryRoot   = Profile->RegistryRoot;
		decltype(github_profile_runtime::DesktopQuit)&    DesktopQuit    = Profile->DesktopQuit;

	public:
		std::function<void()>      OnClosed;
		std::shared_ptr<const int> WindowLifetime = std::make_shared<const int>(0);

		/// \brief Delivers one ledger answer to this window, and only while
		/// the ledger that answered is still the one composed here.
		template <typename Handler>
		[[nodiscard]] auto ledger_answer(Handler aHandler)
		{
			return [this, weak = std::weak_ptr<const int>{ WindowLifetime }, generation = ReviewLedgerGeneration, handler = std::move(aHandler)](auto aResult) mutable {
				if (weak.expired() || Closing || generation != ReviewLedgerGeneration || ReviewLedgerHost == nullptr) {
					return;
				}
				handler(std::move(aResult));
			};
		}

		std::unique_ptr<window_read_decision_lane>            ReadDecision;
		std::shared_ptr<application::github_window_read_host> WindowRead;
		std::optional<core::pull_request_request_context>     QueueFactRequest;
		std::uint64_t                                         QueueFactRevision = 0;
		/// [[BPR-1357]]. What this window's Home shows of the pull request it
		/// asked to open, and the clock behind its wait.
		std::unique_ptr<application::home_open_host>    HomeOpen;
		std::optional<application::queue_consumer_id>   QueueConsumer;
		std::optional<core::window_review_open_request> PendingRestoration;
		/// [[BPR-0733]]. Policy indices behind the rows this window lists.
		std::vector<std::size_t>                              RecentIndexMap;
		std::optional<application::recent_reviews_projection> RecentPublished;
		void                                                  publish_recent();
		auto                                                  open_pull_request_page(const application::pull_request_page_intent& aIntent) -> core::markdown_link_result;
		auto                                                  request_review_window(const application::review_window_intent& aIntent) -> bool;
		void                                                  open_review_destination(const core::window_review_destination& aDestination);
		void                                                  forget_recent(application::recent_review_row aRow);
		void                                                  open_recent(application::recent_review_intent aIntent);
		std::optional<core::request_id>                       RestorationReadRequest;
		std::optional<core::window_reading_restoration>       ReadingRestoration;
		std::optional<core::pending_comparison_resolution>    ReadingComparisonRequest;
		std::optional<core::window_review_destination>        ReadingDestination;
		std::optional<core::window_review_position>           AcceptedReadingPosition;
		bool                                                  ReadingAdvanceQueued      = false;
		bool                                                  ReadingFailed             = false;
		bool                                                  ReadingRestoreUnavailable = false;
		bool                                                  RestorationReadAccepted   = false;
		void                                                  schedule_reading_restoration();
		void                                                  finish_reading_restoration(bool aSuccess);
		void                                                  advance_reading_restoration();
		// [[BPR-1132]]. The window's document tabs: [[BPR-1131]]'s host, whose
		// loads this composition presents through the epoch and workbench
		// hosts. The reviewer's own choices of file and comparison are
		// reflected into the active tab (the observer); switching, closing,
		// duplicating, returning and restoring tabs are presented exactly, by
		// the stages Resume uses (the driver), and completed or refused for the
		// load in force only.
		struct document_tab_ports final
			: application::document_tab_view_port,
			  application::document_tabs_projection_port
		{
			explicit document_tab_ports(implementation& aOwner);
			void            load(const core::document_tab_view_request& aRequest) override;
			void            publish(const application::document_tabs_projection& aProjection) override;
			implementation& Owner;
		};
		std::unique_ptr<document_tab_ports>             TabPorts;
		std::unique_ptr<application::document_tab_host> TabHost;
		std::optional<core::document_tab_view_request>  TabLoad;
		std::optional<core::window_reading_restoration> TabDriver;
		std::optional<core::document_tab_view_request>  TabPresented;
		std::optional<core::workbench_modality>         TabModality;
		std::optional<core::file_identity>              ObservedTabFile;
		bool                                            TabAdvanceQueued = false;
		bool                                            TabFailed        = false;
		/// \brief [[BPR-1303]]. The last accepted handoff kept the selected
		/// diff, so Review owes the active tab's view once its files return.
		bool RefreshKeptTabView      = false;
		bool RevalidatingKeptTabView = false;
		/// \brief [[BPR-1539]]. The pull request the tabs were opened in.
		/// `ReadingDestination` cannot say: opening another pull request
		/// invalidates the shown one, which clears it, before the new read is
		/// accepted.
		std::optional<core::window_review_destination> TabDestination;
		[[nodiscard]] static auto                      tab_modality(const core::workbench_modality aModality) noexcept -> bool;
		[[nodiscard]] auto                             tab_view_presented(const core::window_review_position& aView) const -> bool;
		auto                                           pr_search_scope() -> std::optional<core::source_search_scope>;
		[[nodiscard]] auto                             filtered_search_files() const -> std::optional<std::vector<core::source_search_filtered_file>>;
		void                                           open_search_result_in_new_tab(const core::file_identity& aFile);
		auto                                           register_search_open() -> bool;
		auto                                           register_set_aside_open() -> bool;
		void                                           open_set_aside_file(const std::string& aPath);
		void                                           present_tab(const core::document_tab_view_request& aRequest);
		void                                           adopt_tab_destination(const core::window_review_destination& aDestination);
		void                                           close_review();
		/// \brief One file/change press bound to the PR and exact active tab that entered Review.
		struct pending_review_navigation
		{
			core::workbench_command_id                    Command;
			application::workbench_pull_request_authority Authority;
			core::request_generation                      Generation;
			std::optional<core::document_tab_id>          Tab;
			std::optional<core::window_review_position>   View;
			bool                                          RestoreReading = true;
		};
		std::optional<pending_review_navigation> PendingReviewNavigation;
		std::optional<pending_review_navigation> ReviewNavigationFocus;
		void                                     cancel_review_navigation();
		void                                     finish_review_navigation_focus();

		void               continue_review_navigation();
		void               request_review_navigation(const std::optional<core::workbench_command_id> aCommand);
		void               finish_tab_load(const core::document_tab_view_outcome aOutcome, const bool aAcceptReading = false);
		void               abandon_tab_load();
		void               schedule_tab_presentation();
		void               advance_tab_presentation();
		void               drive_tab();
		void               observe_tab_changes();
		auto               represent_refreshed_tab() -> bool;
		void               observe_tab_reading();
		void               retain_tabs(const core::window_review_tabs& aTabs);
		void               retain_reading_position();
		void               publish(const application::workbench_projection& aProjection) override;
		void               fail(const application::workbench_failure_publication& aFailure) override;
		[[nodiscard]] auto opened_file_facts() const -> core::opened_file_wait_facts;
		[[nodiscard]] auto opened_file_wait() const -> core::opened_file_wait;
		void               present_opened_file();
		void               restoration_notice(const QString& aText);
		void               present_agent_workspace_repair();
		void               cancel_restoration(bool aStartup, bool aReadingGesture = false);
		void               restore_review(const core::window_review_open_request& aRequest);
		void               observe_restoration_read(const application::github_window_read_update& aUpdate);
		void               start();
		[[nodiscard]] auto status() const noexcept -> github_application_status;
		explicit implementation(std::shared_ptr<github_profile_runtime> aProfile, core::bpr_window_context aContext);

		~implementation() override;

		[[nodiscard]] auto credential_vault() noexcept -> application::github_credential_vault_port&;

		[[nodiscard]] auto device_vault_stores() const noexcept -> std::size_t;

		[[nodiscard]] auto device_vault_stored_digest() const -> std::string;

		[[nodiscard]] auto device_vault_stored_is_grant() const -> bool;

		[[nodiscard]] auto device_browser_opens() const noexcept -> std::size_t;

		[[nodiscard]] auto device_enrollments() const noexcept -> std::size_t;

		[[nodiscard]] auto public_preview_unavailable() const noexcept -> bool;

		[[nodiscard]] auto public_preview_policy_refused() const noexcept -> bool;

		[[nodiscard]] auto public_preview_signed_out() const noexcept -> bool;

		[[nodiscard]] auto public_preview_requests() const noexcept -> std::size_t;

		[[nodiscard]] auto public_preview_header_effects() const noexcept
			-> std::size_t;

		[[nodiscard]] auto public_preview_vault_effects() const noexcept
			-> std::size_t;

		[[nodiscard]] auto public_preview_production_factories() const noexcept
			-> bool;

		[[nodiscard]] auto public_preview_wire_requests() const noexcept
			-> std::size_t;

		[[nodiscard]] auto public_preview_wire_capability_probes() const noexcept
			-> std::size_t;

		[[nodiscard]] auto public_preview_wire_credentialed_refusals()
			const noexcept -> std::size_t;

		[[nodiscard]] auto public_preview_wire_operations() const
			-> std::vector<public_preview_wire_operation_record>;

		[[nodiscard]] auto public_preview_budget() const noexcept
			-> std::optional<application::github_rate_limit_record>;

		[[nodiscard]] auto public_preview_snapshot_signed_out() const noexcept
			-> bool;

		[[nodiscard]] auto public_preview_snapshot_head_oid() const
			-> std::string;

		[[nodiscard]] auto device_transport_submissions() const noexcept
			-> std::size_t;

		[[nodiscard]] auto device_durable_copies() const -> std::size_t;

		void observe_device_stage(
			const application::device_authorization_presentation& aPresentation);

		void bind_device_refresh_authority(const application::github_presentation_update& aUpdate);

		void observe_device_refresh(
			const application::device_authorization_refresh_observation&
				aObservation);

		void dispatch_device_authorization_gesture(
			const application::device_authorization_gesture& aGesture);

		void select_device_authorization();

		void activate_public_preview(const bool aEnterprise);

		void attempt_public_preview_writes();

		[[nodiscard]] static auto public_preview_now() noexcept -> std::uint64_t;

		void begin_public_preview_sign_in(const core::host_profile_id aProfile);

		void cancel_device_authorization();

		void open_device_verification_address();

		void dispatch_header_source();

		void dispatch_workbench_intent(
			core::workbench_interaction_intent aIntent);

		void dispatch_workbench_change(
			const core::navigate_diff_change_intent aIntent);

		void dispatch_workbench_width(
			const core::workbench_drawer aDrawer,
			const std::uint32_t          aWidth);

		void dispatch_receipt(core::file_receipt_command aCommand);

		[[nodiscard]] static auto thread_identity(
			const application::github_review_thread_record& aThread) -> std::string;

		[[nodiscard]] auto accepted_thread(const std::string_view aIdentity) const
			-> const application::github_review_thread_record*;

		void present_defects();

		void persist_correlated_defect(
			const application::durable_defect_record& aDefect);

		/// \brief Defects whose write has not answered yet. A second gesture
		/// on one of them would be decided against the state before the first.
		std::set<core::defect_id> PendingDefectWrites;

		void persist_defect(application::durable_defect_record aDefect, std::function<void()> aAfter = { });

		void correlate_defects(
			const std::span<const core::source_line_ref> aChangedAnchors = { });

		void load_defects();

		void review_ledger_opened(const bool aAvailable);

		void reload_private_records(const bool aCorrelate);

		[[nodiscard]] auto conversation_endpoint_is_current(
			const core::comparison_pair& aComparison,
			const core::diff_side        aSide) const -> bool;

		[[nodiscard]] auto base_numbering_request() const -> std::optional<application::github_base_numbering_request>;

		[[nodiscard]] auto base_numbering_answers(const application::github_base_numbering_request& aRequest) const -> bool;

		[[nodiscard]] auto base_numbered_as_github() const -> bool;

		void observe_base_numbering();

		void rearm_base_numbering();

		void apply_base_numbering();

		[[nodiscard]] auto current_thread_anchor(
			const application::github_review_thread_record& aThread) const
			-> std::optional<core::source_line_ref>;

		[[nodiscard]] auto defect_anchor_is_current(
			const application::durable_defect_record&       aDefect,
			const application::github_review_thread_record& aThread) const -> bool;

		void classify_defect(qt::workbench_defect_classification aClassification);

		void dispatch_defect(core::defect_command aCommand);

		[[nodiscard]] static auto review_mutation_target(
			const application::accepted_pull_request_handoff& aHandoff)
			-> core::review_mutation_target;

		struct retryable_review_draft_record
		{
			core::review_comment_draft                    Draft;
			application::workbench_pull_request_authority Authority;
			/// [[BPR-1477]]. Why the outcome is still unknown, as last said,
			/// so it is said again when a reread holds the draft again.
			application::review_mutation_failure Failure = application::review_mutation_failure::unavailable;
		};

		[[nodiscard]] static auto exact_retryable_review_draft(
			const retryable_review_draft_record&                 aRecord,
			const core::semantic_selection&                      aSelection,
			const application::workbench_pull_request_authority& aAuthority)
			-> bool;

		[[nodiscard]] auto admit_cli_publication(
			const application::review_mutation_target_binding&  aTarget,
			const application::review_mutation_acting_identity& aActor) const
			-> std::expected<void, application::review_mutation_failure>;

		std::unique_ptr<application::review_tour_host> TourHost;
		/// [[BPR-1157]]. Local Review Cycles in this window: no account needed.
		std::unique_ptr<review_cycle_desktop>             CycleDesktop;
		std::uint64_t                                     WritingHookSubscription = 0;
		std::unique_ptr<review_tour_publication_workflow> TourPublication;
		std::unique_ptr<conversation_reply_workflow>      ConversationReply;
		struct retained_tour_host
		{
			core::tour_owner                               Owner;
			std::unique_ptr<application::review_tour_host> Host;
		};
		std::vector<retained_tour_host> RetiredTourHosts;
		std::optional<core::tour_owner> TourOwner;
		/// [[BPR-1781]]. Start tour waits for the published tour's import.
		bool PendingTourStart = false;
		/// [[BPR-1806]]. The published stop Open in tour asked for, while
		/// the start waits; none starts at the first stop.
		std::optional<std::pair<std::string, core::published_tour_stop>> PendingTourStop;
		std::optional<application::tour_saved_target>                    RememberedTourTarget;
		core::git_commit_oid                                             TourHead;
		core::git_commit_oid                                             TourAttentionHead;
		std::optional<core::tour_code_destination>                       PendingTourSource;
		/// [[BPR-1227]]. Source reads a forwarded `bpr tour` command has on the diff lane. Counted and
		/// atomic because the worker ends its read; shutdown stops them and waits for none.
		std::atomic_size_t TourSourceReadsInFlight{ 0U };
		std::stop_source   TourSourceStop;
		auto               selected_tour_source() const -> std::optional<core::tour_source_range>;
		auto               current_tour_attention() const -> core::tour_attention;
		void               observe_tour_visit();
		void               observe_tour_comparison();

		void finish_tour_navigation();
		void navigate_tour_source(const std::optional<core::tour_code_destination>& aSource);
		void observe_tour_head();
		void suspend_review_tour();
		struct live_tour_view
		{
			application::review_tour_host* Host    = nullptr;
			bool                           Current = false, Composing = false;
		};
		auto live_tour(std::string_view aSelector) -> live_tour_view;
		auto read_tour_source(std::string_view aSelector, const core::tour_code_destination& aSource, std::function<void(application::tour_channel_bytes)>& aRespond) -> bool;
		void start_published_tour(std::optional<std::pair<std::string, core::published_tour_stop>> aStop = std::nullopt);
		void start_tour_at(const std::optional<std::pair<std::string, core::published_tour_stop>>& aStop);
		void finish_pending_tour_start(const application::tour_presentation& aProjection);
		void compose_review_tour(const application::accepted_pull_request_handoff& aHandoff);
		void remember_tour_target(const core::review_tour& aSeed, const application::github_pull_request_snapshot& aSnapshot, std::string_view aHostname);
		void refresh_tour_imports();
		struct writing_operation
		{
			core::writing_request                                            Request;
			std::vector<application::writing_source>                         Sources;
			std::optional<application::agent_invocation_projection_identity> Invocation;
			bool                                                             Test = false;
			/// [[BPR-1900]]. Run in the workspace with read-only tools; its
			/// answer is the agent's last message.
			bool ReadOnly = false;
		};
		struct tour_outline_proposal
		{
			core::writing_target                  Target;
			std::vector<core::tour_outline_entry> Entries;
		};
		std::optional<tour_outline_proposal> OutlineProposal;
		core::scoped_writing_state           WritingState;
		std::optional<writing_operation>     PendingWriting;
		/// [[BPR-1916]]. The skills a prompt can name: the person's own and
		/// the reviewed repository's at the pull request's base.
		std::unique_ptr<application::skill_catalog_loader> SkillCatalog;
		application::skill_library                         SkillLibrary;
		/// [[BPR-1916]]. Loads the skill catalog again: the person's skills,
		/// and with a linked clone the repository's at the base commit.
		void compose_skill_catalog();
		void refresh_skills();
		void present_skills(const application::skill_library& aLibrary);
		/// [[BPR-1916]]. The skills a writing hook names, carried inline to
		/// its run, whatever the agent.
		[[nodiscard]] auto skill_run_for(core::writing_hook aHook) const -> skill_run;
		[[nodiscard]] auto agent_idle() const -> bool;
		[[nodiscard]] auto tour_session_idle() const -> bool;
#if defined(BPR_AGENT_INTEGRATIONS)
		[[nodiscard]] auto locate_claude() const -> application::claude_locator;

		[[nodiscard]] auto locate_agent_program() const -> application::agent_program_locator;

		/// [[BPR-1753]], owner decision 2026-10-23. A card's action
		/// waiting for its file to open in Review: tried again until the file
		/// is on screen, then given up with words after about ten seconds.
		/// Where a card's state goes: its card, or ([[BPR-1773]]) the
		/// answer whose Draft comment borrowed the card's way to a line.
		using card_report = std::function<void(qt::agent_finding_card_state, const QString&, std::uint64_t)>;
		struct pending_card_open
		{
			qt::agent_finding_card_request Request;
			int                            Tries = 0;
			card_report                    Report;
		};
		std::optional<pending_card_open> PendingCardOpen;

		void resume_card_open();

		void save_agent_note(std::string aText, std::optional<std::pair<core::diff_side, std::uint32_t>> aLine, std::string aPath, qt::agent_answer_note_done aDone);

		void act_on_agent_card(const qt::agent_finding_card_request& aRequest, card_report aReport = { });

		[[nodiscard]] auto offered_agent_profiles() const -> std::vector<core::agent_profile>;

		[[nodiscard]] auto run_limit_milliseconds() const -> std::optional<int>;

		[[nodiscard]] auto current_conversation_agent() const -> std::uint32_t;

		void observe_conversation_agent();

		void observe_conversation();

		void present_agent_pull_request();

		void ask_agent_about_thread(const std::string& aIdentity);

		void ask_agent_about_note(const application::local_note_record& aNote);

		[[nodiscard]] auto asked_note() const -> const application::local_note_record*;

		[[nodiscard]] auto decide_answer_draft() -> qt::agent_answer_draft_target;

		void draft_agent_answer(const std::string& aKey, const QString& aText, qt::agent_answer_draft_done aDone);

		void go_to_agent_answer_draft(const std::string& aKey);

		void dismiss_asked_conversation();

		[[nodiscard]] auto read_asked_conversation() const -> std::optional<core::supplement_conversation>;

		void present_conversation(const application::agent_conversation_projection& aView);

		void observe_clone_repository();

		[[nodiscard]] auto prepare_kept_clone(const application::clone_repository& aRepository, const std::string& aDestination) -> application::clone_link_dependencies::keep_job;

		[[nodiscard]] auto linked_history_clone(const bool aWritingOnly) const -> std::optional<std::filesystem::path>;

		[[nodiscard]] auto linked_history_clone_managed(const bool aWritingOnly) const -> bool;

		void check_agent_setup(const bool aWorkspace = true);

		void stop_agent_setup_check();

		[[nodiscard]] auto settings_check_decision() const -> core::agent_check_decision;

		void check_agent_from_settings();

		void present_settings_check();

		void check_agent_workspace();

		void present_clone_links(const application::clone_link_projection& aProjection);

		void compose_agent_setup_view();

		void present_setup_view();

		[[nodiscard]] static auto claude_models() -> std::vector<std::pair<std::string, QString>>;

		void present_run_settings();

		[[nodiscard]] auto apply_run_settings(const std::uint64_t aProfile, std::optional<std::string> aModel, std::optional<std::string> aEffort) -> bool;

		[[nodiscard]] static auto setup_saved_profile(const application::agent_setup_projection& aProjection) -> std::optional<core::agent_profile_id>;

		void take_setup_choice();

		void present_agent_setup(const application::agent_setup_projection& aProjection);
#endif
		void               cancel_writing();
		auto               writing_context_key() const -> std::array<std::uint8_t, 32>;
		[[nodiscard]] auto writing_agent() const -> std::optional<core::agent_profile_id>;

		[[nodiscard]] auto agent_display_name(const core::agent_profile_id aProfile) const -> QString;
		[[nodiscard]] auto writes_tool_free(const core::agent_profile_id aProfile) const -> bool;

		auto writing_target(core::writing_hook aHook, const qt::prose_capture& aCapture) const -> std::optional<core::writing_target>;
		auto selected_writing_source(bool aOutline) const -> std::optional<application::writing_source>;
		auto writing_sources(core::writing_hook aHook, const core::writing_request& aRequest) const -> std::vector<application::writing_source>;
		void request_writing(core::writing_hook aHook, qt::prose_capture aCapture);
#if defined(BPR_AGENT_INTEGRATIONS)
		void               compose_tour_session(const std::vector<core::agent_profile>& aProfiles, const std::vector<core::agent_host_policy>& aPolicies);
		[[nodiscard]] auto tour_reading_notice() const -> QString;
		[[nodiscard]] auto reads_read_only(const core::agent_profile_id aProfile) const -> bool;
		[[nodiscard]] auto dispatch_tour_run(const core::agent_profile_id aAgent, const core::tour_agent_request aKind, const std::string& aAsked, const std::string& aPrompt, const std::optional<core::writing_hook> aRunFor,
											 const std::optional<core::tour_passage>& aPassage = std::nullopt, const core::tour_thread& aThread = { }, bool aUpdate = false, skill_run aSkills = { }) -> bool;
		void               tour_run_admitted(const application::agent_invocation_projection_identity& aIdentity);
		void               tour_run_refused(const std::string_view aReason);
		void               tour_run_finished(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult);
		/// \brief [[BPR-1941]]. The running tour request's events, said as the
		/// Agent tab's live line says them: its workspace made, then the first
		/// output, which is when it writes.
		void               tour_run_started(const application::agent_invocation_projection_identity& aIdentity, const application::prepared_workspace& aWorkspace);
		void               tour_run_streamed(const application::agent_invocation_projection_identity& aIdentity);
		void               present_tour_live(std::optional<qt::agent_run_status> aStatus);
		[[nodiscard]] auto tour_answer_words(const application::agent_run_result& aResult) const -> std::string;
		void               request_tour_draft();
		void               adopt_tour_conversation();
		void               request_tour_ask(const core::tour_thread& aThread, const std::optional<core::tour_passage>& aPassage, const QString& aQuestion, bool aUpdate = false);
		void               finish_tour_ask(const core::tour_agent_run_id aRun, const application::agent_run_result& aResult);
		[[nodiscard]] auto tour_anchor_files(const application::agent_run_result& aResult) const -> std::vector<core::tour_anchor_file>;
		void               finish_tour_draft(const std::optional<core::tour_agent_run_id> aRun, const application::agent_run_result& aResult, const QString& aWords);
		void               decide_tour_proposal(const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText);
		void               relocate_tour_proposal(const std::size_t aIndex);
		void               present_tour_session();
		void               cancel_tour_run();
		/// [[BPR-1930]]. Sends the feedback kept while Draft the tour ran, as
		/// the tour chat's next turn, once the session is idle.
		void send_waiting_tour_feedback(int aTries = 0);
		/// [[BPR-1930]]. Clears the remembered focus (its chip's ×).
		void clear_tour_focus();
		/// [[BPR-1930]] follow-up. Undo on a turn's "Focus set to" line.
		void undo_tour_focus(core::tour_agent_run_id aRun);
		/// [[BPR-1931]]. Resets the tour being authored, keeping a snapshot to
		/// undo it, and undoes that reset while the tour is as it left it.
		void reset_tour();
		void undo_tour_reset();
		void present_tour_reset();
#endif
		auto outline_entries(const writing_operation& aOperation, const application::writing_result& aResult) const -> std::optional<std::vector<core::tour_outline_entry>>;
		auto apply_writing_result(const writing_operation& aOperation, const application::writing_result& aResult) -> std::string;
		void finish_writing(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult);
		auto current_outline_target() const -> std::optional<core::writing_target>;
		void insert_tour_outline();
		void test_writing_hook(core::writing_hook aHook);

		void dispatch_tour_action(qt::tour_editor_action aAction);

		void compose_review_submission(const application::accepted_pull_request_handoff& aHandoff);

		void observe_review_preflight();

		[[nodiscard]] auto current(const core::bpr_window_context& aContext) const noexcept -> bool override;

		void present_saved_drafts();

		[[nodiscard]] auto reply_threads() const -> core::reply_thread_observation;

		[[nodiscard]] auto saved_draft_values() const -> std::vector<core::review_comment_draft>;

		void describe_held_reply();

		[[nodiscard]] auto thread_reply_facts_of(const application::github_review_thread_record& aThread) const -> core::thread_reply_facts;

		[[nodiscard]] auto thread_reply_binding_of(const std::string_view aThread) const -> std::optional<application::review_mutation_thread_reply_binding>;

		void begin_thread_bound_reply(const std::string& aIdentity);

		/// \brief [[BPR-1745]]. An outdated thread's Reply pressed while
		/// a line's draft was being opened, made once that has answered.
		std::optional<std::string> PendingThreadReply;
		void                       resume_pending_thread_reply();

		auto resume_reply_at(const core::source_line_ref& aLine) -> bool;

		void resume_reply();

		void resume_comment();

		auto resume_comment_at(const core::source_line_ref& aLine) -> bool;

		struct pending_unsent_reply_commands
		{
			std::shared_ptr<application::shared_draft_application_host> Owner;
			application::shared_draft_consumer                          Consumer;
			// One exact admitted command per draft in the bounded owner. A
			// later explicit action on that draft replaces its pending intent.
			std::map<core::review_draft_id, core::revisioned_review_draft_command> Commands;
		};
		std::optional<pending_unsent_reply_commands> PendingUnsentReplyCommands;

		void change_unsent_reply(const core::revisioned_review_draft_command& aCommand);

		void flush_unsent_reply_commands();

		void act_on_unsent_reply(const core::review_draft_id aDraft, const qt::workbench_window::unsent_reply_action aAction);

		void publish_review_comments(const std::vector<core::revisioned_review_draft>& aSaved);

		[[nodiscard]] auto start_review_on_shown_draft() -> bool;

		void change_review_drafts(std::vector<core::revisioned_review_draft_command> aCommands, std::function<void(bool)> aDone);

		void hold_review_comments(const application::review_submission_request& aRequest, std::function<void(bool)> aDone);

		void settle_review_comments(const std::string& aKey, const application::review_submission_publication aPublication,
									const std::optional<application::review_submission_request>& aRequest);

		void discard_review_comments(const std::vector<core::review_submission_comment>& aCarried, const core::git_commit_oid& aHead);

		void delete_pending_comment(const core::review_draft_id aDraft, const std::uint64_t aRevision);

		void leave_deleted_draft();

		void accept_shared_draft(const core::revisioned_review_draft& aAccepted);

		void publish_preflight_selection();

		[[nodiscard]] auto ensure_shared_drafts(const application::accepted_pull_request_handoff& aHandoff) -> bool;

		void complete_shared_draft(const bool aSuccess, std::function<void()> aAfter = { }, bool aPreserveForRetry = false);

		[[nodiscard]] auto conversation_source() const -> std::optional<application::review_permalink_source>;

		void select_published_conversation();

		void publish_review_conversations(const application::github_presentation_update& aUpdate);

		void refresh_review_conversations();

		[[nodiscard]] auto observe_review_source(const core::workbench_file_id aFile) const
			-> std::optional<application::review_mutation_source_observation> override;

		[[nodiscard]] auto off_hunk_facts() const -> std::optional<std::pair<application::off_hunk_fact_scope, application::off_hunk_fact_port*>>;

		[[nodiscard]] auto presented_off_hunk_fact(const bool aReview) const -> core::off_hunk_creation_capability;

		void demote_off_hunk_fact(const application::review_submission_request& aRefused);

		void retire_review_mutation_host(const core::conversation_mutation_retirement aConversation);

		[[nodiscard]] auto thread_resolution_connection_refusal() const -> core::thread_resolution_refusal;

		void compose_review_mutation(
			const application::accepted_pull_request_handoff& aHandoff);

		[[nodiscard]] static auto thread_permission(const application::github_review_thread_record& aThread) noexcept -> core::thread_resolution_permission;

		void present_conversation_state();

		void observe_conversation_state();

		void react_to_post(const qt::reaction_gesture& aGesture);

		void publish_reaction(const application::review_reaction_terminal& aTerminal) override;

		[[nodiscard]] auto description_edit_logins() const -> std::pair<std::string, std::string>;

		void present_description_offer();

		void save_description(const qt::overview_description_save& aSave);

		void               publish_description(const application::review_description_terminal& aTerminal) override;
		[[nodiscard]] auto comment_edit_cards() const -> qt::conversation_cards_view*;

		[[nodiscard]] auto reads_anonymously() const -> bool;

		void present_comment_edit_offer();

		[[nodiscard]] auto accepted_comment(const std::string& aPost) const -> std::optional<std::pair<bool, std::string>>;

		void save_comment_edit(const qt::comment_edit_save& aSave);

		void publish_comment_edit(const application::review_comment_edit_terminal& aTerminal) override;

		void present_reviewer_offer();

		void present_reviewer_picker();

		void reviewer_gesture(const qt::reviewer_picker_gesture& aGesture);

		void debounce_reviewer_search(core::reviewer_search_effect aSearch);

		[[nodiscard]] static auto reviewer_failure(const application::review_mutation_failure aFailure) -> core::reviewer_request_failure;

		void search_reviewers(const core::reviewer_search_effect& aSearch);

		void request_reviewer(const core::reviewer_request_effect& aRequest);

		[[nodiscard]] static auto reviewer_overlay_key(const application::github_pull_request_snapshot& aSnapshot) -> std::string;

		void observe_reviewer_request_state();

		[[nodiscard]] auto projected_overview_review_requests(const application::github_pull_request_snapshot& aSnapshot) const
			-> std::vector<core::projected_reviewer_request>;

		void present_overview_review_requests();

		[[nodiscard]] auto projected_draft_state(const application::github_pull_request_snapshot& aSnapshot) const -> std::optional<core::projected_draft_state>;

		/// \brief [[BPR-1852]]. What the core decides draft-state changes
		/// from: the accepted read's author and viewer (none in an anonymous
		/// read), whether it is open, and the draft state shown.
		struct draft_state_reading
		{
			std::string Author;
			std::string Viewer;
			bool        Open  = false;
			bool        Draft = false;

			[[nodiscard]] auto facts() const noexcept -> core::draft_state_facts;
		};
		[[nodiscard]] auto draft_state_reading_now() const -> std::optional<draft_state_reading>;

		void present_draft_state_offer();

		void observe_draft_state_change();

		void present_overview_draft_state();

		void change_draft_state(const core::draft_state_change aChange);

		void publish_draft_state(const application::review_draft_state_terminal& aTerminal) override;

		void retire_draft_state_change();

		void publish_reviewers(const application::review_reviewer_terminal& aTerminal) override;

		void retire_reviewer_picker();

		void resolve_review_thread(const qt::thread_resolution_gesture& aGesture);

		[[nodiscard]] static auto thread_resolution_failure_text(const application::review_mutation_failure aFailure, const bool aResolve) -> QString;

		void publish_resolution(const application::review_thread_resolution_terminal& aTerminal) override;

		void refresh_review_projection(
			const application::review_mutation_terminal& aTerminal);

		[[nodiscard]] auto review_mutation_gesture_grant(
			const std::optional<application::review_mutation_publication_request>& aTestingRequest,
			const std::function<application::review_mutation_execution_grant(
				core::application_session_id,
				core::request_generation,
				application::review_mutation_target_binding,
				application::review_mutation_acting_identity)>&                    aIssue)
			-> application::review_mutation_execution_grant;

		[[nodiscard]] auto draft_on_thread_line(
			const core::review_comment_draft& aDraft,
			const std::string_view            aIdentity) const -> bool;

		void begin_review_reply(const std::string& aIdentity);

		void present_inline_publication_failure(application::review_mutation_failure                       aFailure,
												core::review_publication_outcome                           aOutcome,
												const std::optional<designated_review_mutation_selection>& aBinding);

		void                                                         dispatch_review_draft(core::revisioned_review_draft_command aRevisionedCommand, std::function<void(bool)> aSettled = { });
		std::optional<std::pair<core::source_line_ref, std::string>> PendingPrivateReply;
		/// [[BPR-1374]]. The reply the editor held when a Refresh retired it.
		std::optional<core::review_draft_id> ResumeReply;
		/// [[BPR-1475]]. The new comment the editor held when a reread retired it.
		std::optional<core::review_draft_id> ResumeComment;
		bool                                 ResumeCommentOpen = false;
		/// [[BPR-1477]]. Why its outcome is unknown, when it awaits a retry.
		application::review_mutation_failure ResumeCommentFailure = application::review_mutation_failure::unavailable;
		/// [[BPR-1475]]. The read in flight, and whether the drawer held a note
		/// being written when it began.
		std::optional<core::pull_request_request_context> DraftReadRequest;
		bool                                              DraftOpenAtRead = false;
		void                                              finish_review_draft_command(const core::revisioned_review_draft_command& aRevisionedCommand, const application::shared_draft_result& aOutcome, core::review_publication_outcome aPriorOutcome);

		void adopt_review_selection(
			const std::optional<core::semantic_selection>& aSelection,
			const qt::review_draft_editor_clear_reason     aReason);

		void observe_review_selection(
			const std::optional<core::semantic_selection>& aSelection);

		void observe_change_navigation(
			const std::optional<core::semantic_selection>& aSelection);

		void present_review_draft(const core::semantic_selection& aSelection);

		void publish(
			const application::review_mutation_terminal& aTerminal) override;
		void finish_review_terminal(const application::review_mutation_terminal& aTerminal, core::review_publication_outcome aPublicationOutcome);

		void retire_review_ledger();

		std::shared_ptr<private_review_workflow>       PrivateReview;
		std::optional<application::private_run_intent> ExplicitPrivateRun;
		core::agent_setting_facts                      PrivateRequested;
		bool                                           PrivateDraftsReady           = false;
		bool                                           PrivateDraftSelectionPending = false;
		bool                                           PrivateDraftCommandPending   = false;
		void                                           observe_private_review();
		void                                           adopt_private_drafts(const application::durable_draft_commit& aCommit, bool aOpen);
		[[nodiscard]] auto                             insights_capture(const application::accepted_pull_request_handoff& aHandoff) const -> application::insights_review_capture;

		void compose_review_ledger(
			const application::accepted_pull_request_handoff& aHandoff);

		void open_private_review(adapters::sqlite::private_review_configuration aConfiguration);

		void refresh_shared_ledger(const application::workbench_pull_request_authority& aSource, ledger_changes aChanges);

		[[nodiscard]] auto matches_review_ledger(
			const application::pull_request_authority_invalidation& aInvalidation) const
			-> bool;

		void mark_presented_file_seen(
			const application::workbench_selected_diff_publication& aPublication,
			const core::file_receipt&                               aReceipt);

		void observe_selected_material(
			const application::diff_publication& aPublication);

		[[nodiscard]] static auto comment_projection_id(
			const application::github_review_comment_record& aComment) -> std::string;

		[[nodiscard]] static auto comment_projection_id(
			const application::github_issue_comment_record& aComment) -> std::string;

		[[nodiscard]] static auto comment_projection_id(
			const application::github_review_record& aReview) -> std::string;

		// [[BPR-1601]]. A PR comment or review summary is a single-post
		// conversation. Its prefixed provider identity shares the existing
		// ledger namespace without colliding with threads or other post kinds.
		template <typename Post>
		[[nodiscard]] static auto post_observation(const Post& aComment, const std::string_view aViewer)
			-> core::conversation_post_observation
		{
			const auto id   = comment_projection_id(aComment);
			const auto body = std::span{
				reinterpret_cast<const std::uint8_t*>(aComment.Body.data()),
				aComment.Body.size(),
			};
			return core::conversation_post_observation{
				.Comment = core::review_comment_id{ defect_identity(id).Conversation },
				.Edit    = core::comment_edit_version{ durable_identity_half(core::sha256(body), 8U) },
				.Mine    = aComment.Author.has_value() && !aViewer.empty()
					&& aComment.Author->Login == aViewer,
				// [[BPR-1699]]. Who wrote it, for Mark all from them as read.
				.Author = aComment.Author.has_value() ? core::review_author_of(aComment.Author->Login) : core::review_author_id{ },
			};
		}

		/// \brief The read state of the accepted snapshot, as the cards take it.
		struct observed_read_state
		{
			std::vector<qt::thread_read_state> Presented;
			std::uint32_t                      UnreadPosts             = 0U;
			std::uint32_t                      ConversationUnreadPosts = 0U;
		};

		// Structural diagnostics for the retained review composition. These are
		// actual visits, including work on hidden panels; no timing inference.
		std::uint64_t OverviewBuilds             = 0U;
		std::uint64_t OverviewReceiptVisits      = 0U;
		std::uint64_t ConversationThreadsVisited = 0U;
		std::uint64_t ConversationPostsVisited   = 0U;
		/// [[BPR-0972]]. The ledger's durable receipt revision the published
		/// overview read completely, absent when it must be read again.
		std::optional<std::uint64_t> OverviewReceiptRevision;
		/// [[BPR-0972]]. The accepted head the published overview was decided
		/// against. A selection no longer rebuilds the overview, so a moved
		/// head republishes it on its own.
		std::optional<core::git_commit_oid> OverviewHead;
		std::optional<core::git_commit_oid> OverviewReviewedHead;
		std::uint64_t                       OverviewHistoryUpdates = 0U;
		/// [[BPR-0972]]. The changed-since-review set was refused or answered
		/// stale and must be asked for again. Every overview publication used
		/// to ask again; now the next epoch update or receipt change does.
		bool OverviewChangedPathsPending = false;
		/// [[BPR-0972]]. The overview as last published, and its receipt index,
		/// so a receipt change moves only its own rows.
		std::optional<application::pull_request_overview_projection> PublishedOverview;
		/// [[BPR-1841]]. The viewer's resolutions `PublishedOverview` counts
		/// over the read's, by GitHub thread id, and the read it was made from.
		std::map<std::string, bool, std::less<>>  OverviewResolutions;
		std::optional<core::provider_snapshot_id> OverviewResolutionsSnapshot;
		application::overview_receipt_index       OverviewReceipts;
		/// [[BPR-1089]]. The accepted snapshot's label buckets ([[BPR-1087]]),
		/// the snapshot they hold, the Overview's request for them, and the
		/// count Discuss lists with the request it was counted under.
		application::feedback_counter_model                                                                FeedbackCounters;
		std::optional<core::provider_snapshot_id>                                                          FeedbackSnapshot;
		std::optional<application::overview_feedback_request>                                              FeedbackRequest;
		std::optional<std::pair<application::overview_drill_down, application::overview_feedback_request>> FeedbackDrillDown;
		std::uint64_t                                                                                      OverviewFeedbackPublications = 0U;
		std::uint64_t                                                                                      FeedbackObservations         = 0U;
		std::uint64_t                                                                                      FeedbackBodiesParsed         = 0U;
		std::uint64_t                                                                                      OverviewReceiptUpdates       = 0U;
		std::uint64_t                                                                                      OverviewReceiptRows          = 0U;

		/// RFC-BPR-0089 M1. The window's commit filter: the model, whether the
		/// Commits tab has asked for the chain's trees, and the pull request
		/// the filter belongs to. A filter is the reviewer's for one pull
		/// request in one window; opening another forgets it.
		application::commit_filter_model                                                                                  CommitFilter;
		bool                                                                                                              CommitFilterWanted = false;
		std::optional<std::tuple<core::git_host_id, core::github_account_id, core::repository_id, core::pull_request_id>> CommitFilterReview;
		/// [[BPR-1688]]. The filtered review as a window's destination names it,
		/// and the filters a restoration brings back for its destination until
		/// that review is adopted.
		std::optional<std::tuple<core::repository_locator, core::pull_request_number, core::github_account_id>> CommitFilterDestination;
		std::optional<std::pair<core::window_review_destination, core::window_review_filters>>                  RestoringFilters;
		std::uint64_t                                                                                           CommitFilterPublications = 0U;
		/// RFC-BPR-0089 M4. The lines last asked for a filtered diff's comments,
		/// and the answer.
		std::optional<application::filtered_comment_lines_request>     FilteredCommentLinesAsked;
		std::optional<application::filtered_comment_lines_observation> FilteredCommentLines;
		/// [[BPR-1735]]. GitHub's own diff (the merge base to the head) of each
		/// file a comment is placed on, by its head path: what was asked, at
		/// which head and merge base, and the answer once it came.
		struct github_diff_lines_entry
		{
			core::git_commit_oid                                      Head;
			core::git_commit_oid                                      MergeBase;
			std::optional<application::github_diff_lines_observation> Answer;
		};
		std::map<std::string, github_diff_lines_entry> GitHubDiffLines;
		/// [[BPR-1736]]. The profile's learned off-hunk facts, opened once per
		/// data root on their own connection to the recovery journal.
		mutable std::unique_ptr<application::off_hunk_fact_port> OffHunkFacts;
		mutable std::filesystem::path                            OffHunkFactsRoot;
		mutable bool                                             OffHunkFactsOpened = false;
		/// [[BPR-1735]]. The comments pending in the review on this head, as
		/// last published to it, which the core holds back when it places one
		/// outside GitHub's diff.
		std::vector<core::review_submission_comment> PendingReviewComments;
		/// [[BPR-1735]]. Testing only: a build that placed nothing on the Whole
		/// PR diff, as before the owner's decision.
		bool WholeDiffPlacementDisabled = false;
		/// [[BPR-1733]]. Each comparison this window showed a file on, with the
		/// commit its right side was read at and the file's path there: what a
		/// kept suggestion on that comparison can be carried from.
		struct shown_comparison
		{
			core::file_comparison_digest Digest;
			core::git_commit_oid         Head;
			std::string                  Path;
		};
		static constexpr std::size_t  maximum_shown_comparisons = 512U;
		std::vector<shown_comparison> ShownComparisons;
		/// [[BPR-1733]]. The lines asked for a kept suggestion from an earlier
		/// head, and the answers, refusals included.
		static constexpr std::size_t                          maximum_head_move_answers = 256U;
		std::vector<application::head_move_lines_request>     HeadMoveLinesAsked;
		std::vector<application::head_move_lines_observation> HeadMoveLines;
		/// [[BPR-1733]]. The kept suggestions the private store holds on other
		/// heads of this pull request than the accepted one, as last read.
		std::vector<private_review_workflow::earlier_suggestion> EarlierSuggestions;
		/// [[BPR-1707]]. The comparison request the accepted comparison was
		/// last resolved for: a resolution asking for it again is a refresh.
		std::optional<core::comparison_selection> SettledFilterRequest;

		[[nodiscard]] auto commit_filter_comparison() -> std::optional<core::filter_comparison>;

		void adopt_commit_filter(const application::accepted_pull_request_handoff&               aHandoff,
								 const std::optional<application::github_pull_request_snapshot>& aSnapshot);

		[[nodiscard]] auto filter_destination(const core::window_review_destination& aDestination) const -> bool;

		void retain_filters();

		void publish_commit_filter();

		void request_commit_filter_trees();

		void request_generated_attributes();

		[[nodiscard]] auto permalink_snippet_scope() const -> std::optional<application::markdown_blob_snippet_scope>;

		void refresh_commit_filter();

		void apply_commit_filter_gesture(const application::commit_filter_gesture& aGesture);

		void apply_commit_rematch(const application::commit_filter_rematch_gesture& aGesture);

		void present_split_strip();

		[[nodiscard]] auto filtered_merge_base() const -> std::optional<core::git_commit_oid>;

		[[nodiscard]] auto selected_diff_filtered() const noexcept -> bool;

		[[nodiscard]] auto filtered_comment_lines_request() const -> std::optional<application::filtered_comment_lines_request>;

		[[nodiscard]] auto filtered_comment_source() const -> std::optional<core::filtered_comment_source>;

		[[nodiscard]] auto whole_diff_commits() const -> std::optional<std::pair<core::git_commit_oid, core::git_commit_oid>>;

		[[nodiscard]] auto whole_diff_source(const std::string& aHeadPath) const -> std::optional<core::whole_diff_comment_source>;

		[[nodiscard]] auto shown_github_diff_lines() -> std::optional<std::optional<core::line_correspondence>>;

		void request_github_diff_lines(const std::vector<std::string>& aHeadPaths);

		[[nodiscard]] auto whole_diff_presentation() -> std::optional<qt::filtered_comment_presentation>;

		void present_review_comment_holds();

		void request_filtered_comment_lines();

		/// \brief [[BPR-1733]]. A kept suggestion written on an earlier head of
		/// the file shown, and the request for its lines there and here.
		struct moved_candidate
		{
			core::revisioned_review_draft        Saved;
			application::head_move_lines_request Request;
		};

		[[nodiscard]] auto shown_right_path() const -> std::string;

		[[nodiscard]] auto moved_candidates() const -> std::vector<moved_candidate>;

		void observe_head_moves();

		void ask_head_move_lines();

		void present_moved_suggestions();

		void discard_carried_suggestion(const core::review_draft_id aDraft, const std::uint64_t aRevision);

		void present_filtered_comment();

		void place_filtered_threads(application::github_pull_request_projection& aProjection) const;

		void apply_file_filter_gesture(const std::vector<std::string>& aCommits);

		void apply_generated_gesture(const application::generated_file_gesture& aGesture);

		void adopt_generated_marks();

		void want_commit_filter_trees();

		[[nodiscard]] static auto receipt_file(const application::workbench_selected_diff_publication& aPublication) -> core::file_identity;

		[[nodiscard]] auto observe_read_state() -> std::optional<observed_read_state>;

		/// \brief [[BPR-0972]]. The snapshot's thread observations and read
		/// state, as the last complete observation left them. A newly accepted
		/// snapshot or a retired ledger drops it.
		struct read_state_cache
		{
			core::provider_snapshot_id                                    Snapshot;
			std::size_t                                                   ThreadCount = 0U;
			std::vector<std::uint32_t>                                    Unread;
			std::uint32_t                                                 ConversationUnreadPosts = 0U;
			std::vector<std::uint32_t>                                    Counted;
			std::vector<application::conversation_thread_observation>     Threads;
			std::vector<std::pair<std::string, std::vector<std::string>>> Names;
			std::vector<std::pair<std::string, std::size_t>>              ByIdentity;
			std::uint32_t                                                 UnreadPosts = 0U;
			bool                                                          Unique      = false;
		};
		std::optional<read_state_cache>                             ReadStateCache;
		std::optional<std::map<local_defect_identity, std::string>> DefectThreadNames;
		std::uint64_t                                               ReadStateUpdates = 0U;

		[[nodiscard]] auto cached_conversation(const std::string_view aIdentity) const -> std::optional<std::size_t>;

		auto publish_thread_read_state(const std::size_t aThread) -> std::optional<std::uint32_t>;

		static void append_unread_names(
			const application::conversation_thread_observation& aThread,
			const std::vector<std::string>&                     aNames,
			const std::vector<core::review_comment_id>&         aUnread,
			std::vector<std::string>&                           aOut);

		auto publish_read_state() -> std::optional<std::uint32_t>;

		void refresh_queue_unseen(const std::uint32_t aUnseen);

#if defined(BPR_BUILD_TESTING)
		auto mark_conversation_read_through(const std::string& aThread, const std::string& aComment) -> bool;
#endif

		[[nodiscard]] auto accepted_read_conversation(const std::string& aIdentity) const
			-> std::optional<application::conversation_thread_observation>;

		void mark_conversation_read(const std::string& aThread);

		[[nodiscard]] auto accepted_read_conversations() const
			-> std::optional<std::vector<application::conversation_thread_observation>>;

		void mark_conversations_read(const std::optional<std::string>& aAuthor);

		void mark_conversation_unread(const std::string& aThread);

		void request_conversation_receipts();

		void publish_overview(const bool aLoadReceipts = true);

		[[nodiscard]] auto projected_overview_resolutions(const application::github_pull_request_snapshot& aSnapshot) const
			-> std::map<std::string, bool, std::less<>>;

		void present_overview_resolutions();

		void observe_feedback_counters();

		[[nodiscard]] auto overview_file_paths() const -> std::vector<std::string>;

		void publish_overview_feedback();

		void open_feedback_drill_down(const application::overview_drill_down& aDrillDown, const application::overview_feedback_request& aRequest, const bool aReveal);

		void refresh_overview_receipts();

		[[nodiscard]] auto changed_since_review(
			const core::git_commit_oid&        aReviewed,
			const core::git_commit_oid&        aHead,
			std::vector<std::filesystem::path> aPaths)
			-> std::optional<std::vector<std::string>>;

		void publish(const application::receipt_projection& aProjection) override;

		void publish(const application::queue_landing_projection& aProjection) override;

		void fail(
			const application::receipt_failure_publication& aFailure) override;

		auto read_cached_source(application::cached_review_source_request aRequest, std::stop_token aStop, application::cached_review_source_completion aCompletion) -> bool override;
		void prepare_runtime() override;
		void accept(application::accepted_pull_request_handoff aHandoff) override;

		void invalidate(
			application::pull_request_authority_invalidation aInvalidation) override;

		[[nodiscard]] auto find_profile(const core::host_profile_id aProfile) const
			-> const adapters::github::validated_github_host_profile*;

		[[nodiscard]] auto native_sign_in_profile(core::host_profile_id aProfile) -> core::host_profile_id;
		[[nodiscard]] auto owner() const -> core::setup_owner_id override;
		[[nodiscard]] auto profiles() const -> std::vector<application::setup_profile_observation> override;

		[[nodiscard]] auto shorthand_hostname() const -> std::string override;
		void               present(const application::setup_destination_projection& aProjection) override;
		void               configure(const core::pending_setup_destination& aPending) override;

		[[nodiscard]] auto register_discovered_cli_account(
			const adapters::github::detail::gh_cli_inventory_account& aAccount)
			-> const adapters::github::validated_github_host_profile*;

		void present_github_cli_accounts();

		void observe_cli_session(const application::github_presentation_update& aUpdate);

		void discover_github_cli_accounts(const bool aConnectSingle = true);

		void               synchronize_setup_servers(const application::queue_landing_projection& aProjection);
		[[nodiscard]] auto next_setup_owner() -> core::setup_owner_id;
		void               retire_setup_effects();
		void               stage_setup_credential(const core::pending_setup_destination& aPending, application::github_credential_material aCredential,
												  const application::github_credential_provenance     aProvenance = application::github_credential_provenance::manual,
												  std::optional<application::device_binding_lifetime> aLifetime = std::nullopt, const bool aSettled = false);
		void               authenticate(const core::pending_setup_destination& aOriginal) override;
		void               authenticate_manual(core::pending_setup_destination aPending);
		void               preview(const core::pending_setup_destination& aPending) override;
		auto               open(const core::host_profile_id aProfile, const core::setup_destination& aDestination) -> application::setup_destination_opening override;

		/// [[BPR-0987]]. A pasted link's location, waiting for this window to
		/// accept its pull request, then its file, then that file's diff.
		struct link_location
		{
			core::setup_destination                           Destination;
			std::optional<core::pull_request_location_target> Target;
			std::optional<core::workbench_file_id>            File;
		};
		std::optional<link_location> PendingLinkLocation;
		bool                         LinkLocationQueued = false;

		[[nodiscard]] static auto same_name(const std::string_view aLeft, const std::string_view aRight) -> bool;
		[[nodiscard]] auto        shows_destination(const core::setup_destination& aDestination) const -> bool;
		/// [[BPR-1475]]. After this window publishes to the pull request (a
		/// review, a comment, a reply or a tour), it reads the pull request
		/// again, as Refresh does, so what was posted shows without
		/// a manual refresh (owner, 2026-10-13). One reread is owed at most: a
		/// read already in flight may predate the publication, so the reread
		/// waits for it to settle rather than being dropped or stacked.
		bool RereadOwed   = false;
		bool RereadQueued = false;
		/// [[BPR-1867]]. The reread owed is only the application's own news
		/// (activity on GitHub, a return to an aged pull request): it waits
		/// while the reviewer writes. A publication's reread clears it.
		bool RereadQuiet = false;
		/// A wait for the reviewer to stop writing is asked again after this.
		bool               RereadRetrying = false;
		void               reread_after_publication();
		void               hand_on_activity(const std::vector<core::pull_request_ref>& aActive);
		void               reread_for_news();
		void               schedule_publication_reread();
		void               pay_publication_reread();
		void               schedule_link_location();
		void               finish_link_location(const core::pull_request_location_refusal aRefusal, const bool aWholeLocation);
		[[nodiscard]] auto comparison_ends_at_head() const -> bool;
		void               advance_link_location();
		void               observe_setup_session(const application::github_presentation_update& aUpdate);

		void retire_cli_discovery();

		void dispatch_intent(application::github_intent aIntent);

		void dispatch_epoch_intent(application::epoch_intent aIntent);

		void admit(
			const application::agent_session_admission&                                                aAdmission,
			std::function<void(std::expected<core::agent_session, application::ledger_store_failure>)> aDone) override;

		void session_catalog(
			std::function<void(std::expected<application::agent_session_catalog_state, application::ledger_store_failure>)> aDone) override;

		[[nodiscard]] auto current_agent_draft_observation() const
			-> std::optional<application::agent_draft_observation>;

		std::uint64_t                               PrivateNoteScope = 0U;
		std::vector<application::local_note_record> AcceptedPrivateNotes;
		/// [[BPR-1592]]. Draft comment on a note at its line, waiting for that
		/// line's draft to be the selected one.
		struct pending_note_promotion
		{
			std::string           Text;
			core::source_line_ref At;
			std::function<void()> Refused;
			std::function<void()> Done;
		};
		std::optional<pending_note_promotion>                     PendingNotePromotion;
		std::optional<std::map<core::file_identity, std::string>> NotePaths;

		[[nodiscard]] auto private_note_file_label(const application::local_note_record& aNote) -> std::string;

		void promote_private_item(std::string aText, std::function<void()> aRefused, std::function<void()> aDone = { });

		void promote_line_note(const application::local_note_record& aNote, std::function<void()> aRefused);

		void resume_note_promotion();

		void dispatch_private_note(const qt::private_note_intent& aIntent);

		void refresh_agent_private_context_choices();

#if defined(BPR_AGENT_INTEGRATIONS)
		[[nodiscard]] auto current_agent_chat_selection() const
			-> std::optional<core::agent_selection>;

		void clear_side_chat();

		void refresh_agent_authority();

		void dispatch_agent_intent(qt::agent_intent aIntent);

		[[nodiscard]] auto dispatch_agent_invocation(
			const core::agent_profile_id                          aProfile,
			const std::vector<application::local_note_id>&        aIncludedNotes,
			const std::optional<application::agent_draft_choice>& aIncludedDraft,
			const bool                                            aExistingCheckoutChosen,
			const std::string_view                                aChatMessage,
			const std::optional<core::agent_session>&             aTurnSession,
			const core::agent_entry_point                         aEntryPoint,
			const std::optional<core::workshop_operation>& aWorkshopOperation, bool aWritingOnly = false, const tour_launch& aTour = { })
			-> bool;

		[[nodiscard]] auto launch_agent_invocation(
			const core::agent_profile_id                   aProfile,
			const core::pull_request_authority&            aAuthority,
			const core::application_session_id             aApplicationSession,
			application::agent_context_observation         aObservation,
			const bool                                     aExistingCheckoutChosen,
			const std::optional<core::agent_session>&      aContinuedSession,
			std::vector<core::agent_finding_snapshot_file> aFrozen, bool aWritingOnly = false, const tour_launch& aTour = { }) -> bool;

		/// \brief Where the reviewer is pointing, as a continuation may say it.
		///
		/// \details A file, a side, and a line number: the position, with
		/// every identity of the revision on screen deliberately left behind.
		/// The comparison digest, the segment, and the group that position
		/// belongs to are the old revision's to state, and they are stated
		/// once the re-alignment has recomputed them.
		struct continuation_anchor
		{
			core::file_identity File;
			core::diff_side     Side = core::diff_side::left;
			std::uint32_t       Line = 0U;
		};

		/// \brief What one continued turn is waiting on its alignment to send.
		struct pending_continuation
		{
			core::agent_profile_id                 Profile;
			core::pull_request_authority           Authority;
			core::application_session_id           ApplicationSession;
			application::agent_context_observation Observation;
			bool                                   ExistingCheckoutChosen = false;
			core::agent_session                    Session;
			std::optional<continuation_anchor>     PointedAt;
		};

		[[nodiscard]] auto begin_continuation_realignment(
			const core::agent_profile_id              aProfile,
			const core::pull_request_authority&       aAuthority,
			const core::application_session_id        aApplicationSession,
			application::agent_context_observation    aObservation,
			const bool                                aExistingCheckoutChosen,
			const core::agent_session&                aTurnSession,
			const std::optional<continuation_anchor>& aPointedAt) -> bool;

		[[nodiscard]] auto refuse_continuation_realignment(
			pending_continuation                         aTurn,
			const application::agent_realignment_refusal aRefusal) -> bool;

		void finish_continuation_realignment(
			pending_continuation                   aTurn,
			application::agent_realignment_outcome aOutcome);

		[[nodiscard]] auto frozen_snapshot_files() const
			-> std::vector<core::agent_finding_snapshot_file>;

		void present_agent_refusal(
			const std::string_view aSubject, const std::string_view aReason);

		void ask_comment_workshop(const qt::comment_workshop_request& aRequest);

		void settle_materialized_comment_workshop(
			const core::revisioned_review_draft&                aPlaceholder,
			const std::optional<core::revisioned_review_draft>& aAccepted);

		void record_comment_workshop_press();

		void settle_comment_workshop(
			const core::agent_session_id         aSession,
			const application::agent_run_outcome aOutcome);

		void present_comment_workshop_settlement(
			const core::review_draft_id aDraft, const std::string& aReason);

		void present_finding_outcome(
			const application::agent_finding_outcome& aOutcome);

		[[nodiscard]] auto next_private_note_id() const
			-> application::local_note_id;

#else
		/// [[BPR-1179]]. What an agent-free review keeps of the authority
		/// refresh: private history follows the review. There is no agent host
		/// to observe it, and no chat or launch surface to scope.
		void refresh_agent_authority()
		{
			observe_private_review();
		}
#endif
		[[nodiscard]] auto current_note_attachment() const
			-> std::optional<application::local_note_attachment>;

#if defined(BPR_AGENT_INTEGRATIONS)
		[[nodiscard]] auto anchored_durable_defect(
			const core::agent_finding& aFinding) const
			-> std::optional<core::defect_record>;

		void dispatch_finding_intent(qt::finding_action_intent aIntent);

		void dispatch_side_chat_intent(qt::side_chat_intent aIntent);

		void present_chat_outcome(const application::agent_chat_outcome& aOutcome);

		[[nodiscard]] auto all_private_note_ids() const
			-> std::vector<application::local_note_id>;

		[[nodiscard]] auto current_agent_draft_choice() const
			-> std::optional<application::agent_draft_choice>;

		[[nodiscard]] auto current_chat_private_prose() const
			-> application::agent_chat_private_prose;

		void present_side_chat(const application::agent_chat_projection& aChat);

		void present_side_chat(const core::agent_revision_binding& aBinding);

		[[nodiscard]] static auto agent_provider_name(
			const core::agent_profile_id aProfile) -> std::string;
#endif

		void publish(
			const application::epoch_presentation_update& aUpdate) override;

		void clear_diff_selection();

		[[nodiscard]] auto shows_selected_diff(const application::workbench_selected_diff_request& aRequest) const -> bool;

		[[nodiscard]] auto revalidates_kept_tab(const application::workbench_selected_diff_request& aRequest) const -> bool;

		void begin(
			const application::workbench_selected_diff_request& aRequest) override;

		void publish(
			const application::workbench_selected_diff_publication& aPublication) override;

		void fail(
			const application::workbench_selected_diff_observation& aFailure) override;

		template <typename Predicate>
		void invalidate_diff_if(Predicate&& aPredicate)
		{
			if (DiffHost == nullptr) {
				return;
			}
			// [[BPR-1344]]. Preparation in flight for a deleted or invalidated
			// review stores nothing after it; the next publication asks again.
			cancel_preparation();
			DiffHost->prepare({ });
			const auto& state   = DiffHost->state();
			const auto* context = state.Pending.has_value()
				? &*state.Pending
				: state.Current.has_value() ? &*state.Current
											: nullptr;
			if (context != nullptr && aPredicate(context->Key.Authority)) {
				DiffHost->invalidate(context->Key.Authority);
			}
		}

		void schedule_preparation();

		[[nodiscard]] auto reviewer_busy() const -> bool;

		void after_reviewer_decision();

		void cancel_preparation();

		void start_preparation();

		void finish_preparation(const std::uint64_t aGeneration, application::epoch_prepared_comparisons aPrepared);

		void request_diff(
			core::file_comparison_key    aKey,
			const core::comparison_pair& aComparison,
			core::diff_options           aOptions,
			const std::string_view       aPath);

		void classify_comparison(
			const core::diff_request_context& aContext,
			const std::string_view            aPath);

		void reclassify_shown_comparison();

		void retain_definition(
			const core::git_blob_oid&                     aBlob,
			const application::syntax_definition_version& aDefinition);

		void adopt_published_classification(
			const application::diff_publication& aPublication);

		void adopt_classification(
			const application::token_span_publication& aSpans);

		[[nodiscard]] static auto native_appearance_observation() noexcept
			-> core::appearance_observation;

		void observe_native_appearance();

		void dispatch_appearance_intent(application::appearance_intent aIntent);

		void adopt_appearance_settings(const bool aPaletteCommitted = false);

		void preview_imported_theme(const core::appearance_palette& aPalette);

		void report_appearance_refusal(
			const application::appearance_failure_publication& aFailure);

		void apply_appearance();

		void begin_shutdown(window_retirement aMode = window_retirement::forced);

		void begin_other_shutdown();

		void schedule_shutdown_poll();

		void poll_shutdown();

		decltype(github_profile_runtime::OwnedDiagnosticLog)& OwnedDiagnosticLog = Profile->OwnedDiagnosticLog;
		decltype(github_profile_runtime::DiagnosticLog)&      DiagnosticLog      = Profile->DiagnosticLog;

		decltype(github_profile_runtime::RememberDeviceSignIn)& RememberDeviceSignIn = Profile->RememberDeviceSignIn;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::DeviceWallOffsetSeconds)& DeviceWallOffsetSeconds = Profile->DeviceWallOffsetSeconds;
#endif
		decltype(github_profile_runtime::DeviceBindingEffects)&        DeviceBindingEffects        = Profile->DeviceBindingEffects;
		decltype(github_profile_runtime::DeviceBindingStore)&          DeviceBindingStore          = Profile->DeviceBindingStore;
		decltype(github_profile_runtime::DevicePersistence)&           DevicePersistence           = Profile->DevicePersistence;
		decltype(github_profile_runtime::DeviceRefreshGeneration)&     DeviceRefreshGeneration     = Profile->DeviceRefreshGeneration;
		decltype(github_profile_runtime::UnregisteredDeviceTransport)& UnregisteredDeviceTransport = Profile->UnregisteredDeviceTransport;
		// The registered platforms' real wire, and the executor its background
		// work runs on. Declared executor-first so the transport can never
		// outlive the lanes it posts to. Both stay empty on unregistered
		// platforms.
		decltype(github_profile_runtime::DeviceTransportEffects)& DeviceTransportEffects = Profile->DeviceTransportEffects;
		decltype(github_profile_runtime::DeviceWireTransport)&    DeviceWireTransport    = Profile->DeviceWireTransport;
		// What the composition actually selected, for the status report: the
		// shipped capability where the real wire composed, the refusal where
		// it did not, and the fixture course's override in a tests-enabled
		// build.
		decltype(github_profile_runtime::DeviceCapabilityRegistered)& DeviceCapabilityRegistered = Profile->DeviceCapabilityRegistered;
		// [[BPR-0797]]. Which browser capability the run bound, as opposed to
		// whether it bound one.
		decltype(github_profile_runtime::ComposedDeviceBrowser)& ComposedDeviceBrowser = Profile->ComposedDeviceBrowser;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::RegisteredDeviceCourse)&                     RegisteredDeviceCourse                     = Profile->RegisteredDeviceCourse;
		decltype(github_profile_runtime::DeviceRefreshCourse)&                        DeviceRefreshCourse                        = Profile->DeviceRefreshCourse;
		decltype(github_profile_runtime::SuppressDeviceBrowser)&                      SuppressDeviceBrowser                      = Profile->SuppressDeviceBrowser;
		decltype(github_profile_runtime::DisableDeviceAuthorizationPanelComposition)& DisableDeviceAuthorizationPanelComposition = Profile->DisableDeviceAuthorizationPanelComposition;
		// [[BPR-0515]]. Whether this run composes its device wire against the
		// closed loopback port rather than the discovered system proxy.
		decltype(github_profile_runtime::NetworkDenied)& NetworkDenied = Profile->NetworkDenied;
#endif
#if defined(BPR_BUILD_TESTING)
		// RFC-BPR-0017 courses. The first forces the shipped selection
		// available against the recorded fixture so the composed anonymous
		// authority can be driven end to end; the second does the same and
		// then activates an enterprise profile, where the refusal is the
		// core policy's rather than the composition's.
		decltype(github_profile_runtime::PublicPreviewEnabledCourse)& PublicPreviewEnabledCourse = Profile->PublicPreviewEnabledCourse;
		decltype(github_profile_runtime::PublicPreviewGhesCourse)&    PublicPreviewGhesCourse    = Profile->PublicPreviewGhesCourse;
#endif
		// Whether a browser capability was bound at all, which is the fact the
		// handoff's `unavailable` answer reports and the construction cut
		// removes.
		decltype(github_profile_runtime::DeviceBrowserComposed)& DeviceBrowserComposed = Profile->DeviceBrowserComposed;
		decltype(github_profile_runtime::SystemBrowser)&         SystemBrowser         = Profile->SystemBrowser;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::RecordingBrowser)& RecordingBrowser = Profile->RecordingBrowser;
#endif
		decltype(github_profile_runtime::DeviceTimer)&       DeviceTimer       = Profile->DeviceTimer;
		decltype(github_profile_runtime::DeviceProjections)& DeviceProjections = Profile->DeviceProjections;
		// The window's panel, adopted rather than owned: the window built it
		// and keeps it alive. Null under the construction cut, and read on
		// every publication, which is why the fan-out asks rather than binds.
		qt::device_authorization_panel*                           DevicePanel            = nullptr;
		decltype(github_profile_runtime::DeviceProjectionFanOut)& DeviceProjectionFanOut = Profile->DeviceProjectionFanOut;
		// The vault slot this run's renewable material belongs to, and what
		// composition did with it. Held so a republication of the same
		// acceptance cannot bind twice or renew on every update.
		decltype(github_profile_runtime::DeviceRefreshAuthority)& DeviceRefreshAuthority = Profile->DeviceRefreshAuthority;
		// [[BPR-0550]]. Whether the device presentation last seen was
		// `authorized`, so the memo above is dropped on the transition into it.
		decltype(github_profile_runtime::DeviceStageAuthorized)&     DeviceStageAuthorized     = Profile->DeviceStageAuthorized;
		decltype(github_profile_runtime::DeviceRefreshBindings)&     DeviceRefreshBindings     = Profile->DeviceRefreshBindings;
		decltype(github_profile_runtime::DeviceRefreshResumes)&      DeviceRefreshResumes      = Profile->DeviceRefreshResumes;
		decltype(github_profile_runtime::DeviceRefreshPublications)& DeviceRefreshPublications = Profile->DeviceRefreshPublications;
		decltype(github_profile_runtime::DeviceRefreshObservation)&  DeviceRefreshObservation  = Profile->DeviceRefreshObservation;
		decltype(github_profile_runtime::DeviceClock)&               DeviceClock               = Profile->DeviceClock;
		decltype(github_profile_runtime::DeviceEnrollment)&          DeviceEnrollment          = Profile->DeviceEnrollment;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::CountedDeviceEnrollment)& CountedDeviceEnrollment = Profile->CountedDeviceEnrollment;
#endif
		decltype(github_profile_runtime::DeviceHost)&            DeviceHost            = Profile->DeviceHost;
		decltype(github_profile_runtime::ProfileCatalog)&        ProfileCatalog        = Profile->ProfileCatalog;
		decltype(github_profile_runtime::SetupDeviceEnrollment)& SetupDeviceEnrollment = Profile->SetupDeviceEnrollment;
		std::optional<core::device_authorization_generation>     SetupDeviceGeneration;
		std::unique_ptr<application::setup_destination_host>     SetupDestination;
		std::optional<core::setup_request_id>                    SetupConfigurationRequest;
		bool                                                     SetupQueueReady              = false;
		bool                                                     SetupConfigurationSubmitted  = false;
		bool                                                     GithubCliDiscoveryPending    = false;
		std::uint64_t                                            GithubCliDiscoveryGeneration = 0U;
		/// [[BPR-1373]]. The pinned profile whose refusal last re-checked gh.
		std::optional<core::host_profile_id>            GithubCliRecheckedFor;
		core::setup_owner_id                            SetupOwner{ 1U };
		std::optional<core::setup_request_id>           SetupDeviceRequest;
		std::optional<core::github_identity_generation> SetupAcceptedGeneration;

		struct published_conversation_selection
		{
			application::review_mutation_target_binding  Target;
			application::review_mutation_acting_identity Actor;
			core::source_line_ref                        Anchor;
			std::string                                  Comment;
		};
		std::optional<published_conversation_selection> PendingPublishedConversation;

		application::review_comment_placement ReviewCommentPlacement{ adapters::github::detail::parse_review_permalink_body };
		/// [[BPR-1338]]. The read host's last window publication, kept across the placement's re-projection.
		std::optional<core::markdown_context>                       PublishedMarkdownContext;
		std::string                                                 PublishedHostname;
		std::optional<application::accepted_pull_request_handoff>   LastAcceptedHandoff;
		std::optional<application::review_mutation_source_material> ReviewSourceMaterial;

		decltype(github_profile_runtime::Profiles)&                 Profiles                 = Profile->Profiles;
		decltype(github_profile_runtime::ProfileOptions)&           ProfileOptions           = Profile->ProfileOptions;
		decltype(github_profile_runtime::ProviderEffects)&          ProviderEffects          = Profile->ProviderEffects;
		decltype(github_profile_runtime::VaultEffects)&             VaultEffects             = Profile->VaultEffects;
		decltype(github_profile_runtime::ReviewMutationEffects)&    ReviewMutationEffects    = Profile->ReviewMutationEffects;
		decltype(github_profile_runtime::ReviewLedgerEffects)&      ReviewLedgerEffects      = Profile->ReviewLedgerEffects;
		decltype(github_profile_runtime::ReviewLedgerStoreEffects)& ReviewLedgerStoreEffects = Profile->ReviewLedgerStoreEffects;
		decltype(github_profile_runtime::ReviewLedgerFile)&         ReviewLedgerFile         = Profile->ReviewLedgerFile;
		decltype(github_profile_runtime::ReviewSubmissionEffects)&  ReviewSubmissionEffects  = Profile->ReviewSubmissionEffects;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::Fixture)&                Fixture                = Profile->Fixture;
		decltype(github_profile_runtime::ReviewSubmissionServer)& ReviewSubmissionServer = Profile->ReviewSubmissionServer;
		std::unique_ptr<gated_review_mutation_decision_executor>  ReviewSubmissionDecisionGate;
		gated_review_mutation_decision_executor*                  ReviewLedgerDecisionGate = nullptr;
		std::unique_ptr<gated_review_mutation_decision_executor>
														 ReviewMutationDecisionGate;
		std::size_t                                      EpochPruneIntentDispatchCount  = 0U;
		std::size_t                                      EpochDeleteIntentDispatchCount = 0U;
		std::size_t                                      EpochCancelIntentDispatchCount = 0U;
		std::optional<appearance_theme_probe_state>      AppearanceThemeProbe;
		std::optional<workbench_composition_probe_state> WorkbenchCompositionProbe;
		std::optional<uiforetw_selected_diff_probe_state>
			UIforEtwSelectedDiffProbe;
		std::optional<native_appearance_selected_diff_probe_state>
			NativeAppearanceSelectedDiffProbe;
		std::optional<review_ledger_composition_probe_state>
			ReviewLedgerCompositionProbe;
		std::optional<review_round_trip_composition_probe_state>
			ReviewRoundTripCompositionProbe;

		std::optional<threaded_reply_probe_state> ThreadedReplyProbe;
		std::optional<fixture_file_reveal_state>  FileReveal;
		// [[BPR-1084]]. The line an attended look's seeded comment is on.
		std::optional<std::uint32_t>                     AttendedDraftLine;
		std::optional<label_authoring_probe_state>       LabelAuthoringProbe;
		std::optional<published_file_switch_probe_state> PublishedFileSwitchProbe;
		std::optional<reply_draft_probe_state>           ReplyDraftProbe;
		std::optional<refresh_pick_probe_state>          RefreshPickProbe;
		std::optional<draft_delete_probe_state>          DraftDeleteProbe;
		std::optional<start_review_probe_state>          StartReviewProbe;
		std::optional<review_progress_push_probe_state>  ReviewProgressPushProbe;
		std::optional<defect_composition_probe_state>    DefectCompositionProbe;
		std::size_t                                      WorkbenchIntentDispatches = 0U;
		std::size_t                                      WorkbenchChangeDispatches = 0U;
		std::size_t                                      WorkbenchWidthDispatches  = 0U;
		std::size_t                                      WorkbenchInvalidations    = 0U;
		std::size_t                                      SelectedDiffBegins        = 0U;
		std::size_t                                      SelectedDiffBeginsKept    = 0U;
		std::size_t                                      SelectedDiffPublications  = 0U;
		std::size_t                                      SelectedDiffFailures      = 0U;
		std::optional<application::workbench_selected_diff_publication>
																				  LastSelectedDiff;
		std::size_t                                                               FixtureDiffHelperInvocations           = 0U;
		decltype(github_profile_runtime::DisableWorkbenchSelectionWire)&          DisableWorkbenchSelectionWire          = Profile->DisableWorkbenchSelectionWire;
		decltype(github_profile_runtime::DisableReviewLedgerGestureWire)&         DisableReviewLedgerGestureWire         = Profile->DisableReviewLedgerGestureWire;
		decltype(github_profile_runtime::DisableAgentProcessLauncherComposition)& DisableAgentProcessLauncherComposition = Profile->DisableAgentProcessLauncherComposition;
		decltype(github_profile_runtime::DisableAgentHostComposition)&            DisableAgentHostComposition            = Profile->DisableAgentHostComposition;
		decltype(github_profile_runtime::DisableAgentGestureWire)&                DisableAgentGestureWire                = Profile->DisableAgentGestureWire;
		decltype(github_profile_runtime::DisableAgentFindingHostComposition)&     DisableAgentFindingHostComposition     = Profile->DisableAgentFindingHostComposition;
		decltype(github_profile_runtime::DisableChatSessionHostComposition)&      DisableChatSessionHostComposition      = Profile->DisableChatSessionHostComposition;
		decltype(github_profile_runtime::DisableCommentWorkshopComposition)&      DisableCommentWorkshopComposition      = Profile->DisableCommentWorkshopComposition;
		decltype(github_profile_runtime::DisableMcpHelperComposition)&            DisableMcpHelperComposition            = Profile->DisableMcpHelperComposition;
		decltype(github_profile_runtime::DisableSideChatPanelComposition)&        DisableSideChatPanelComposition        = Profile->DisableSideChatPanelComposition;
		decltype(github_profile_runtime::DisableFindingQueueComposition)&         DisableFindingQueueComposition         = Profile->DisableFindingQueueComposition;
		decltype(github_profile_runtime::ExternalAgentExecutable)&                ExternalAgentExecutable                = Profile->ExternalAgentExecutable;
		decltype(github_profile_runtime::SparseAgentSetupLook)&                   SparseAgentSetupLook                   = Profile->SparseAgentSetupLook;
		decltype(github_profile_runtime::CodexAgentExecutable)&                   CodexAgentExecutable                   = Profile->CodexAgentExecutable;
		decltype(github_profile_runtime::ClaudeAgentExecutable)&                  ClaudeAgentExecutable                  = Profile->ClaudeAgentExecutable;
		decltype(github_profile_runtime::ClaudeSearch)&                           ClaudeSearch                           = Profile->ClaudeSearch;
		decltype(github_profile_runtime::CodexSearch)&                            CodexSearch                            = Profile->CodexSearch;
		const bool&                                                               AttendedAgentSearch                    = Profile->AttendedAgentSearch;
		bool                                                                      ReviewLedgerWasPresentOnEntry          = false;
		bool                                                                      ReviewLedgerRoundTripPassed            = false;
		bool                                                                      FixtureReviewMutationHostComposed      = false;
#endif
		decltype(github_profile_runtime::OwnedVault)&       OwnedVault       = Profile->OwnedVault;
		decltype(github_profile_runtime::TransportFactory)& TransportFactory = Profile->TransportFactory;
		// [[BPR-0316]]. What the anonymous provider was constructed with, kept
		// so the live-candidate evidence can compare it against the shipped
		// factory this composition built rather than assert a call site.
		decltype(github_profile_runtime::AnonymousTransportFactory)& AnonymousTransportFactory = Profile->AnonymousTransportFactory;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::ShippedAnonymousTransports)&      ShippedAnonymousTransports      = Profile->ShippedAnonymousTransports;
		decltype(github_profile_runtime::PublicPreviewSelectionAvailable)& PublicPreviewSelectionAvailable = Profile->PublicPreviewSelectionAvailable;
#endif
		decltype(github_profile_runtime::Provider)& Provider = Profile->Provider;
		// RFC-BPR-0017's credential-free authorities. Declared beside the
		// authenticated provider they are constructed beside, and after it, so
		// they are destroyed first and the counting wrapper can never outlive
		// the provider it forwards to.
		decltype(github_profile_runtime::AnonymousProvider)&            AnonymousProvider            = Profile->AnonymousProvider;
		decltype(github_profile_runtime::CountedAnonymousPullRequests)& CountedAnonymousPullRequests = Profile->CountedAnonymousPullRequests;
		decltype(github_profile_runtime::AnonymousSmartGit)&            AnonymousSmartGit            = Profile->AnonymousSmartGit;
		// Whether the selection handed those authorities to the host. False on
		// every shipped platform in this slice, which is what makes the mode
		// answer `public_preview_unavailable` with nothing sent.
		decltype(github_profile_runtime::PublicPreviewComposed)& PublicPreviewComposed = Profile->PublicPreviewComposed;
		// Sign-in gestures the host forwarded. Evidence that the refusal
		// leaves the reviewer somewhere to go rather than nowhere.
		std::size_t                                                      PublicPreviewSignInRequests   = 0U;
		std::size_t                                                      PublicPreviewWriteRefusals    = 0U;
		decltype(github_profile_runtime::PublicPreviewProjections)&      PublicPreviewProjections      = Profile->PublicPreviewProjections;
		decltype(github_profile_runtime::PublicPreviewProjectionFanOut)& PublicPreviewProjectionFanOut = Profile->PublicPreviewProjectionFanOut;
		decltype(github_profile_runtime::DecisionLane)&                  DecisionLane                  = Profile->DecisionLane;
		/// [[BPR-1344]]. The lane this window's epoch, diff and workbench hosts post to.
		reviewer_decision_lane                ReviewerDecisions{ DecisionLane, [this] { after_reviewer_decision(); } };
		std::unique_ptr<qt::workbench_window> Window;
#if defined(_WIN32)
		// [[BPR-0653]]. Declared after the window so it lets go of the window
		// first.
		std::unique_ptr<windows_title_row_host> TitleRow;
#endif
		std::unique_ptr<review_submission_workflow>                  ReviewSubmission;
		decltype(github_profile_runtime::CredentialBindings)&        CredentialBindings  = Profile->CredentialBindings;
		decltype(github_profile_runtime::GitCaches)&                 GitCaches           = Profile->GitCaches;
		decltype(github_profile_runtime::EpochCatalogs)&             EpochCatalogs       = Profile->EpochCatalogs;
		decltype(github_profile_runtime::ApplicationDataRoot)&       ApplicationDataRoot = Profile->ApplicationDataRoot;
		std::unique_ptr<application::review_ledger_application_host> ReviewLedgerHost;
		/// [[BPR-1211]]. Advances whenever a ledger is composed or retired, so
		/// an answer from a retired ledger never reaches its successor.
		std::uint64_t ReviewLedgerGeneration = 0U;

		/// [[BPR-1085]]. The snapshot and account the preflight last read.
		std::optional<std::pair<core::provider_snapshot_id, core::github_account_id>> PreflightSnapshot;
		/// Conversation receipts are being loaded for this ledger.
		bool ConversationReceiptsLoading = false;
		/// The overview asked for receipts it does not hold.
		bool OverviewReceiptsLoading = false;
		bool OverviewReceiptsFailed  = false;
		bool PrivateRecordsFailed    = false;

		mutable std::mutex                                           ReviewLedgerStateMutex;
		std::optional<application::workbench_pull_request_authority> ReviewLedgerAuthority;
		// [[BPR-0654]]. The overview's changed-since-review set: asked of the
		// epoch host once per (last-reviewed head, accepted head, file list)
		// and kept until one of them moves. `Changed` is empty while the host
		// is answering and after it could not answer.
		struct changed_since_review_state final
		{
			application::changed_paths_request      Request;
			std::optional<std::vector<std::string>> Changed;
		};
		std::optional<changed_since_review_state> ChangedSinceReview;
		std::optional<core::file_receipt>         ReviewLedgerReceipt;
		/// [[BPR-0647]]. The update rows the epoch host published, for the overview.
		application::epoch_row_ledger                   EpochRows;
		bool                                            ReviewLedgerAvailable = false;
		std::vector<application::durable_defect_record> DurableDefects;
		std::unique_ptr<application::review_mutation_application_host>
																	  ReviewMutationHost;
		decltype(github_profile_runtime::LiveReviewMutationGrant)&    LiveReviewMutationGrant = Profile->LiveReviewMutationGrant;
		std::optional<designated_review_mutation_selection>           InlineReviewBinding;
		std::optional<designated_review_mutation_selection>           PublishingBinding;
		core::review_publication_outcome                              PublishingPriorOutcome     = core::review_publication_outcome::not_published;
		decltype(github_profile_runtime::AttendedReviewMutationMode)& AttendedReviewMutationMode = Profile->AttendedReviewMutationMode;
		/// [[BPR-0657]]. The thread the next draft answers, named by a card's Reply.
		/// [[BPR-0657]]. The thread resolution in flight and what came of them.
		std::size_t ThreadResolutionGestures    = 0U;
		std::size_t ThreadResolutionRefusals    = 0U;
		std::size_t ThreadResolutionCompletions = 0U;
		std::size_t ThreadResolutionFailures    = 0U;
		/// [[BPR-1841]]. Overview counts moved in place by a resolution.
		std::size_t OverviewResolutionUpdates = 0U;
		/// [[BPR-1741]]. The viewer's reaction and resolution changes
		/// reads have not caught up with, the pull request they are on, and
		/// each unanswered request's subject.
		core::conversation_overlay                          ConversationOverlay;
		core::conversation_mutation_state                   ConversationMutations;
		std::optional<designated_review_mutation_selection> ConversationMutationBinding;
		std::size_t                                         ReactionGestures    = 0U;
		std::size_t                                         ReactionCompletions = 0U;
		std::size_t                                         ReactionFailures    = 0U;
		/// [[BPR-1774]]. The description save in flight, and what came of saves.
		std::optional<application::review_mutation_request_context> PendingDescriptionEdit;
		std::size_t                                                 DescriptionEditGestures    = 0U;
		std::size_t                                                 DescriptionEditRefusals    = 0U;
		std::size_t                                                 DescriptionEditCompletions = 0U;
		std::size_t                                                 DescriptionEditConflicts   = 0U;
		std::size_t                                                 DescriptionEditFailures    = 0U;
		/// [[BPR-1804]]. The reviewer picker's state, the searches and
		/// requests in flight by host request, the typing debounce, and what
		/// came of gestures.
		static constexpr int                        reviewer_search_delay = 250;
		core::reviewer_picker_state                 ReviewerPicker;
		std::map<std::uint64_t, std::uint64_t>      PendingReviewerSearches;
		std::map<std::uint64_t, std::uint64_t>      PendingReviewerRequests;
		std::unique_ptr<QTimer>                     ReviewerSearchDebounce;
		std::optional<core::reviewer_search_effect> DebouncedReviewerSearch;
		std::size_t                                 ReviewerSearches           = 0U;
		std::size_t                                 ReviewerChoices            = 0U;
		std::size_t                                 ReviewerRequestsAsked      = 0U;
		std::size_t                                 ReviewerRequestCompletions = 0U;
		std::size_t                                 ReviewerRequestFailures    = 0U;
		/// [[BPR-1849]]. The review requests the viewer asked for that reads
		/// have not caught up with, the pull request they were asked on, what
		/// the Overview shows of them, and how often People moved in place.
		core::reviewer_request_overlay                ReviewerOverlay;
		std::optional<std::string>                    ReviewerOverlayPullRequest;
		std::vector<core::projected_reviewer_request> OverviewReviewRequests;
		std::size_t                                   OverviewReviewRequestUpdates = 0U;
		/// [[BPR-1852]]. The viewer's draft-state change that reads have not
		/// caught up with, the pull request it was asked on, the change in
		/// flight, and what came of changes.
		struct pending_draft_state
		{
			application::review_mutation_request_context Context;
			std::uint64_t                                Request = 0U;
			core::draft_state_change                     Change  = core::draft_state_change::ready_for_review;
		};
		core::draft_state_overlay          DraftOverlay;
		std::optional<std::string>         DraftOverlayPullRequest;
		std::optional<pending_draft_state> PendingDraftState;
		std::uint64_t                      DraftStateRequests        = 0U;
		std::size_t                        DraftStateGestures        = 0U;
		std::size_t                        DraftStateRefusals        = 0U;
		std::size_t                        DraftStateCompletions     = 0U;
		std::size_t                        DraftStateFailures        = 0U;
		std::size_t                        OverviewDraftStateUpdates = 0U;
		/// [[BPR-1775]]. The comment save in flight, and what came of saves.
		std::optional<application::review_mutation_request_context> PendingCommentEdit;
		std::size_t                                                 CommentEditGestures    = 0U;
		std::size_t                                                 CommentEditRefusals    = 0U;
		std::size_t                                                 CommentEditCompletions = 0U;
		std::size_t                                                 CommentEditConflicts   = 0U;
		std::size_t                                                 CommentEditFailures    = 0U;

		std::vector<std::unique_ptr<application::review_mutation_application_host>>
			RetiredReviewMutationHosts;
		std::optional<application::review_mutation_request_context>
			PendingReviewMutation;
		std::optional<std::optional<core::semantic_selection>>
																	 DeferredReviewSelection;
		std::optional<core::semantic_selection>                      SelectedSemanticSelection;
		std::optional<core::review_comment_draft>                    ReviewDraft;
		std::optional<retryable_review_draft_record>                 RetryableReviewDraft;
		std::uint64_t                                                ReviewDraftRevision = 0U;
		std::vector<application::review_comment_projection>          ReviewProjection;
		application::reconciliation_result                           ReviewReconciliation;
		core::bpr_window_context                                     DraftWindow;
		core::application_session_id                                 WindowSession;
		core::review_draft_scope_id                                  DraftScope;
		std::shared_ptr<application::shared_draft_application_host>  SharedDrafts;
		application::shared_draft_consumer                           DraftConsumer;
		std::optional<core::revisioned_review_draft>                 PublishingDraft;
		decltype(github_profile_runtime::WorkbenchSettings)&         WorkbenchSettings = Profile->WorkbenchSettings;
		composition_workbench_review                                 WorkbenchReview;
		std::unique_ptr<application::snapshot_workbench_observation> WorkbenchObservations;
		decltype(github_profile_runtime::AppearanceSettings)&        AppearanceSettings = Profile->AppearanceSettings;
		decltype(github_profile_runtime::ThemeDocuments)&            ThemeDocuments     = Profile->ThemeDocuments;
		// Built once and shared. The catalog is immutable after construction
		// and the classifier holds no theme, palette, or font, so neither has
		// to be rebuilt when appearance changes.
		decltype(github_profile_runtime::SyntaxDefinitions)&              SyntaxDefinitions = Profile->SyntaxDefinitions;
		std::unique_ptr<composition_blob_source>                          SyntaxBlobs;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>     SyntaxClassification;
		composition_appearance_projections                                AppearanceProjections;
		std::unique_ptr<qt::appearance_profile>                           AppearanceProfile;
		std::string                                                       AppliedSelectionColor;
		std::string                                                       AppliedBaseColor;
		bool                                                              AppliedFromProfile             = false;
		decltype(github_profile_runtime::WorkbenchPersistenceAvailable)&  WorkbenchPersistenceAvailable  = Profile->WorkbenchPersistenceAvailable;
		decltype(github_profile_runtime::AppearancePersistenceAvailable)& AppearancePersistenceAvailable = Profile->AppearancePersistenceAvailable;
		decltype(github_profile_runtime::EpochEntropy)&                   EpochEntropy                   = Profile->EpochEntropy;
		decltype(github_profile_runtime::EpochClock)&                     EpochClock                     = Profile->EpochClock;
		decltype(github_profile_runtime::EpochEffects)&                   EpochEffects                   = Profile->EpochEffects;
		decltype(github_profile_runtime::DiffEffects)&                    DiffEffects                    = Profile->DiffEffects;
		decltype(github_profile_runtime::PrepareEffects)&                 PrepareEffects                 = Profile->PrepareEffects;
#if defined(BPR_AGENT_INTEGRATIONS)
		decltype(github_profile_runtime::AgentEffects)&     AgentEffects     = Profile->AgentEffects;
		decltype(github_profile_runtime::TourAgentEffects)& TourAgentEffects = Profile->TourAgentEffects;
		decltype(github_profile_runtime::McpEffects)&       McpEffects       = Profile->McpEffects;
#endif
		decltype(github_profile_runtime::WorkbenchEffects)&  WorkbenchEffects  = Profile->WorkbenchEffects;
		decltype(github_profile_runtime::AppearanceEffects)& AppearanceEffects = Profile->AppearanceEffects;
		decltype(github_profile_runtime::QueueEffects)&      QueueEffects      = Profile->QueueEffects;
		std::unique_ptr<application::epoch_application_host> EpochHost;
		/// [[BPR-1485]]. Linked xdiff, every diff's line matcher; declared before
		/// the hosts that hold it. A matcher thread a stop abandoned holds its own share.
		std::shared_ptr<application::diff_line_matcher_port>                LineMatcher                 = core::make_xdiff_line_matcher();
		decltype(github_profile_runtime::AgentWorkspaceRoot)&               AgentWorkspaceRoot          = Profile->AgentWorkspaceRoot;
		decltype(github_profile_runtime::AgentWorkspaceNeighbourhood)&      AgentWorkspaceNeighbourhood = Profile->AgentWorkspaceNeighbourhood;
		std::string                                                         LastAgentWorkspacePath;
		decltype(github_profile_runtime::AgentWorkspaceRecoveryPending)&    AgentWorkspaceRecoveryPending    = Profile->AgentWorkspaceRecoveryPending;
		decltype(github_profile_runtime::AgentWorkspaceRecoveryPostFailed)& AgentWorkspaceRecoveryPostFailed = Profile->AgentWorkspaceRecoveryPostFailed;
		decltype(github_profile_runtime::AgentWorkspaceParent)&             AgentWorkspaceParent             = Profile->AgentWorkspaceParent;
#if defined(BPR_AGENT_INTEGRATIONS)
		std::unique_ptr<adapters::agent::agent_process_launcher> AgentProcess;
	#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<composition_agent_process_launcher_probe> AgentProcessProbe;
	#endif
		/// [[BPR-1301]]. The profile's Claude process wrapper as last
		/// published, and the one port that points every Claude launch of
		/// this window's agent host at it. Declared before the host that
		/// holds the port, so the port outlives it.
		std::shared_ptr<application::claude_process_wrapper_source> ClaudeWrapperSource = std::make_shared<application::claude_process_wrapper_source>();
		std::unique_ptr<application::wrapped_claude_process>        ClaudeProcess;
		/// [[BPR-1748]]. The Codex and plain-text harness programs this
		/// window's profile saved, and the port that gives those launches an
		/// absolute program.
		std::shared_ptr<application::agent_program_source> AgentProgramSource = std::make_shared<application::agent_program_source>();
		/// [[BPR-1749]]. The host policies the agent host decides by,
		/// kept so setup's rows carry the same verdicts.
		std::vector<core::agent_host_policy>                AgentPolicies;
		std::unique_ptr<application::located_agent_process> LocatedProcess;
		std::unique_ptr<application::claude_wrapper_editor> ClaudeWrapperEditor;
		/// [[BPR-1749]]. The profile's agent setup: its store, the
		/// check's own launcher and lane, and the host. Declared in that order
		/// so the host goes first.
		std::unique_ptr<application::agent_setup_store_port>     AgentSetupStore;
		std::unique_ptr<adapters::agent::agent_process_launcher> AgentSetupLauncher;
		std::unique_ptr<worker_executor>                         AgentSetupLane;
		std::unique_ptr<application::agent_setup_host>           AgentSetup;
		/// [[BPR-1750]]. Clone folders and links: store, then host.
		std::unique_ptr<application::clone_link_store_port> CloneLinkStore;
		std::unique_ptr<application::clone_link_host>       CloneLinks;
		/// [[BPR-1752]]. The review skill: store, then host.
		std::unique_ptr<application::review_skill_store_port> ReviewSkillStore;
		std::unique_ptr<application::review_skill_host>       ReviewSkill;
		/// [[BPR-1753]]. Each pull request's conversation: store, then host.
		std::unique_ptr<application::native_agent_session_store_port> NativeAgentSessions;
		std::unique_ptr<application::agent_conversation_store_port>   AgentConversationStore;
		std::unique_ptr<application::agent_conversation_host>         AgentConversation;
		/// [[BPR-1751]]. Why the last workspace probe could not make the
		/// workspace, for its words; absent when it was not that.
		std::optional<application::agent_workspace_unavailable_reason> LastWorkspaceProbeReason;
		/// [[BPR-1773]]. The conversation the Agent tab's next questions are
		/// about, by its card identity, and the pull request it is in.
		struct asked_conversation
		{
			std::string   Identity;
			std::uint64_t PullRequest = 0U;
			/// A private note, which is sent as a Private history copy.
			std::optional<application::local_note_id> Note;
			/// A review thread the viewer may reply to.
			bool Replies = false;
		};
		std::optional<asked_conversation> AskedConversation;
		/// [[BPR-1773]]. Where each sent question's answer would be drafted,
		/// by the key its turn holds, as core placed it when it was sent.
		struct agent_answer_draft
		{
			core::answer_draft_place Place = core::answer_draft_place::pull_request_comment;
			std::string              Thread;
			std::string              Path;
			core::diff_side          Side  = core::diff_side::right;
			std::uint32_t            First = 0U;
			std::uint32_t            Last  = 0U;
			/// [[BPR-1862]]. The draft Draft comment wrote, while the turn
			/// says Go to draft.
			std::optional<core::review_draft_id> Drafted;
		};
		std::map<std::string, agent_answer_draft> AgentAnswerDrafts;
		std::uint64_t                             NextAgentAnswerDraft = 0U;
		std::stop_source                          AgentSetupWorkspaceStop;
		std::uint64_t                             AgentSetupWorkspaceGeneration = 0U;
		/// [[BPR-1375]]. The profile's saved Claude model, as last published.
		std::string                                              ClaudeModel;
		std::unique_ptr<composition_agent_invocation_projection> AgentProjections;
		/// [[BPR-1753]], owner amendment 2026-10-23. The agents the
		/// reviewer added, for the launches, and the process that starts them.
		std::shared_ptr<application::agent_entry_source>  AgentEntries = std::make_shared<application::agent_entry_source>();
		std::unique_ptr<application::agent_entry_process> EntryProcess;
		/// [[BPR-1827]]. The entry process, recorded to diagnostics; declared
		/// after it and before the host that runs through it.
		std::unique_ptr<application::diagnosed_agent_process> DiagnosedProcess;
		/// The profiles the host last offered, before the list chose among them.
		std::vector<core::agent_profile> HostOfferedProfiles;
		/// A test that put its own program on a built-in profile keeps it offered.
		bool                                                BuiltInProfilesOffered = false;
		std::unique_ptr<application::agent_invocation_host> AgentHost;
		/// [[BPR-1900]]. The tour session: its own launcher (so Cancel stops
		/// only a tour run), the same Claude wrapper, program and added-agent
		/// ports, its own projection and host. Declared in that order so the
		/// host goes first.
		std::unique_ptr<adapters::agent::agent_process_launcher> TourProcess;
	#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<composition_agent_process_launcher_probe> TourProcessProbe;
	#endif
		std::unique_ptr<application::wrapped_claude_process>  TourClaudeProcess;
		std::unique_ptr<application::located_agent_process>   TourLocatedProcess;
		std::unique_ptr<application::agent_entry_process>     TourEntryProcess;
		std::unique_ptr<application::diagnosed_agent_process> TourDiagnosedProcess;
		std::unique_ptr<tour_session_projection>              TourProjection;
		std::unique_ptr<application::agent_invocation_host>   TourSession;
		/// [[BPR-1900]]. The session's runs and transcript, the run in
		/// flight with its invocation, the workspace it ran in, and the
		/// whole-tour proposal waiting on the author.
		core::tour_agent_session                                         TourAgent;
		std::optional<core::tour_agent_run_id>                           TourRun;
		std::optional<application::agent_invocation_projection_identity> TourInvocation;
		std::string                                                      TourWorkspacePath;
		/// [[BPR-1900]] follow-up. The head the tour run's workspace holds.
		std::optional<core::git_commit_oid> TourWorkspaceHead;
		std::optional<core::review_tour>    TourSubmittedDraft;
		[[nodiscard]] auto                  capture_tour_agent_context(const core::tour_thread& aThread = { }) const -> application::tour_agent_context;
		bool                                TourDraftPending = false;
		/// [[BPR-1914]]. An ask turn is in flight, and the tour its
		/// conversation belongs to.
		bool                                                TourAskPending = false;
		std::optional<core::tour_draft_id>                  TourConversationDraft;
		std::map<std::string, std::string>                  TourSelectedConversations;
		std::map<std::string, core::agent_revision_binding> TourConversationBindings;
		std::map<std::string, core::agent_session>          TourNativeSessions;
		struct tour_continuation
		{
			std::string                  Scope;
			std::string                  Conversation;
			core::agent_session          Session;
			core::agent_revision_binding Observed;
		};
		std::optional<tour_continuation>             TourContinuation;
		[[nodiscard]] auto                           tour_observed_binding() const -> std::optional<core::agent_revision_binding>;
		[[nodiscard]] auto                           continued_tour_session(const core::tour_thread&) const -> std::optional<core::agent_session>;
		void                                         choose_tour_continuation(const core::tour_thread&);
		void                                         present_tour_continuation();
		application::native_agent_history_projection TourHistory;
		std::string                                  TourHistoryKey;
		std::string                                  TourRunScope;
		application::native_agent_history_projection AgentNativeHistory;
		void                                         present_agent_native_history(const application::native_agent_history_projection&);
		void                                         observe_agent_native_history(std::optional<std::string> aChoice = std::nullopt);
		void                                         observe_tour_native_history(const core::tour_thread&, std::optional<std::string> aChoice = std::nullopt);
		void                                         present_tour_native_history(const application::native_agent_history_projection&);
		[[nodiscard]] auto                           tour_native_scope(const core::tour_thread&) const -> std::string;
		[[nodiscard]] auto                           tour_native_agent() const -> std::string;

		/// [[BPR-1914]] v003. When the run in flight started, for "Worked for Ns".
		std::chrono::steady_clock::time_point TourRunStarted;
		/// [[BPR-1941]]. What the run in flight is doing now, as its live
		/// line in the tour chat says it.
		std::optional<qt::agent_run_status> TourLive;
		std::optional<core::tour_proposal>  TourProposal;
		/// [[BPR-1900]]. The files the proposal was anchored against.
		std::vector<core::tour_anchor_file> TourAnchorFiles;
		/// [[BPR-1931]]. What the last Reset tour cleared, for its Undo, while
		/// the tour is still as the reset left it. The window's life only.
		struct tour_reset_undo
		{
			core::review_tour                   Draft;
			core::tour_agent_session            Session;
			std::optional<core::tour_proposal>  Proposal;
			std::vector<core::tour_anchor_file> Anchors;
			std::uint64_t                       ResetRevision = 0U;
			/// The session's last run when it reset; a later run lets Undo go.
			std::uint64_t ResetRun = 0U;
		};
		std::optional<tour_reset_undo> TourResetUndo;
		// BPR-0532. Declared after the epoch host it resolves and materializes
		// through, so it is destroyed before that host is.
		std::unique_ptr<application::agent_realignment_port> Realigner;
		/// The stop handle of the one outstanding continuation re-alignment.
		/// Held so shutdown and the wall-clock bound can stop the same request
		/// the completion is waiting on, and reset by that completion so a
		/// late timer stops nothing.
		std::shared_ptr<std::stop_source> ContinuationRealignmentStop;
		/// Counted rather than flagged, and atomic because the worker that
		/// posts the completion decrements it when the lane refuses the post.
		/// Shutdown waits on this: a re-alignment in flight holds ports that
		/// close_and_drain would otherwise pull out from under it.
		std::atomic_size_t ContinuationRealignmentsInFlight{ 0U };
		bool               ContinuationRealignmentDeadlineFired = false;
		/// Why the last continued turn froze what it froze: `none` when the
		/// old revision was aligned again, `deadline` when the wait bound
		/// stopped it, and the refusal's own name otherwise.
		std::string LastContinuationRealignment;
		/// Whether that re-alignment asked about a different pair than the one
		/// on screen.
		bool LastContinuationRealignmentRetargeted = false;
		// RFC-BPR-0012. Composed beside the agent host and driven by the
		// shipped window's own gestures. BPR-0513 moved the two panels into
		// the window's conversations drawer, which owns them as it owns every
		// other widget it places, so these are observers of what is on screen
		// rather than a second lifetime for it. They hold no process,
		// provider, credential, or store of their own; the only way out of
		// either is an intent this composition answers.
		qt::side_chat_panel*                                        SideChat     = nullptr;
		qt::finding_queue_view*                                     FindingQueue = nullptr;
		std::unique_ptr<composition_agent_finding_projection>       FindingProjections;
		std::unique_ptr<application::agent_finding_host>            FindingHost;
		std::unique_ptr<composition_agent_chat_projection>          ChatProjections;
		std::unique_ptr<application::agent_chat_session_host>       ChatHost;
		std::unique_ptr<composition_comment_workshop_projection>    WorkshopProjections;
		std::unique_ptr<application::agent_comment_workshop_host>   WorkshopHost;
		std::unique_ptr<adapters::agent::agent_process_launcher>    McpProcess;
		std::unique_ptr<application::mcp_context_helper_supervisor> McpHelper;
		std::filesystem::path                                       McpSnapshotRoot;
#endif
#if defined(BPR_BUILD_TESTING)
		/// [[BPR-1369]]. The diff host's blob port; declared before the host,
		/// so it outlives every read the host makes.
		std::unique_ptr<counting_diff_blob_port> DiffBlobReads;
#endif
		std::size_t CommentWorkshopProposals = 0U;
		/// The operation the reviewer last asked the workshop for, and the
		/// turn started to answer it, spent by that turn's outcome and by no
		/// other. Absent is the ordinary state, and it is what makes a
		/// promoted finding a proposal nobody workshopped.
		std::optional<pending_comment_workshop>             PendingWorkshopOperation;
		std::optional<core::agent_session>                  LatestAgentSession;
		std::optional<core::agent_session>                  NativeFreshAtCurrentRevision;
		std::size_t                                         FindingRefusalsPresented = 0U;
		std::size_t                                         SideChatIntents          = 0U;
		std::size_t                                         SideChatPresentations    = 0U;
		std::size_t                                         SideChatTurns            = 0U;
		std::size_t                                         SideChatRefusals         = 0U;
		std::size_t                                         FrozenSnapshotWrites     = 0U;
		std::unique_ptr<composition_diff_projection>        DiffProjections;
		std::unique_ptr<application::diff_application_host> DiffHost;
		/// [[BPR-1431]]. Source search's RE2 engine, declared before both finders so it outlives them.
		const core::source_regex_engine&                                                      SourceRegex = core::default_source_regex_engine();
		std::unique_ptr<pane_find_controller>                                                 PaneFind;
		std::unique_ptr<pr_search_controller>                                                 PrFind;
		std::optional<std::pair<core::source_search_scope, application::source_corpus_entry>> SearchOpenFile;
		/// [[BPR-1693]]. A set-aside file asked for, until a comparison context retains it.
		std::optional<application::workbench_file_observation_entry> SetAsideOpenFile;
		bool                                                         SearchOpening = false;
		std::optional<core::document_tab_id>                         SearchOpeningBinding;
		std::optional<core::document_tab_id>                         SearchCreatedBinding;
		bool                                                         SearchComparisonGesture = false;
		std::optional<core::document_tab_id>                         SearchCreatedTab;
		std::optional<core::comparison_pair>                         SearchCreatedComparison;
		/// [[BPR-1327]]. The window being prepared, its cancellation and the
		/// preparations whose completion has not yet returned to the lane.
		std::vector<application::workbench_selected_diff_request> PreparedWindow;
		std::stop_source                                          PreparationStop;
		std::uint64_t                                             PreparationGeneration = 0U;
		bool                                                      PreparationScheduled  = false;
		bool                                                      PreparationDeferred   = false;
		std::atomic_size_t                                        PreparationsInFlight{ 0U };
		/// [[BPR-1871]]. The opened file's last refusal or failure, and
		/// whether a missing-comparison refusal was kept from the status
		/// line because the pull request was still loading.
		std::optional<application::workbench_failure_publication> OpenedFileFailure;
		std::optional<core::workbench_file_id>                    OpenedFileFailed;
		bool                                                      OpenedFileFailureWithheld = false;
#if defined(BPR_BUILD_TESTING)
		std::size_t PreparationsPreempted               = 0U;
		std::size_t PreparationsStartedBeforeOpenedFile = 0U;
		/// [[BPR-1212]]. Answers to turn into `cancelled` before they apply.
		std::size_t ForcedBaseNumberingCancellations = 0U;
		/// [[BPR-1570]]. The label-authoring course makes the kept base
		/// numbering answer one about another question after its refresh.
		bool        StaleBaseNumberingQuestion = false;
		bool        PreparationDisabled        = false;
		std::size_t PreparationRounds          = 0U;
		std::size_t PreparedResolutions        = 0U;
		std::size_t PreparedRemoteBlobs        = 0U;
		std::size_t PreparedRemoteBatches      = 0U;
#endif
		std::unique_ptr<application::workbench_settings_port>     SessionWorkbenchSettings;
		profile_window_layout_port*                               DurableWindowLayout       = nullptr;
		window_retirement                                         Retirement                = window_retirement::forced;
		bool                                                      LayoutEraseStarted        = false;
		bool                                                      LayoutEraseComplete       = false;
		std::shared_ptr<std::atomic_bool>                         LayoutEraseDeliveryFailed = std::make_shared<std::atomic_bool>(false);
		std::unique_ptr<application::workbench_application_host>  WorkbenchHost;
		std::unique_ptr<application::appearance_application_host> AppearanceHost;
		/// [[BPR-1562]]. The readers' fenced code: its own texts, classifier
		/// and host over the shared definitions and the appearance effect
		/// lane, so it never waits behind or reorders the panes' work.
		application::markdown_code_texts                                MarkdownCodeTexts;
		std::unique_ptr<markdown_code_blob_source>                      MarkdownCodeBlobs;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>   MarkdownCodeClassifier;
		std::unique_ptr<application::markdown_code_syntax_host>         MarkdownCode;
		std::unique_ptr<permalink_snippet_source>                       MarkdownSnippetSource;
		std::unique_ptr<application::markdown_blob_snippet_host>        MarkdownSnippets;
		std::uint64_t                                                   MotionSubscription       = 0U;
		decltype(github_profile_runtime::QueueStore)&                   QueueStore               = Profile->QueueStore;
		decltype(github_profile_runtime::QueueRefresh)&                 QueueRefresh             = Profile->QueueRefresh;
		decltype(github_profile_runtime::QueueHost)&                    QueueHost                = Profile->QueueHost;
		decltype(github_profile_runtime::Discovery)&                    Discovery                = Profile->Discovery;
		decltype(github_profile_runtime::DiscoveryClock)&               DiscoveryClock           = Profile->DiscoveryClock;
		decltype(github_profile_runtime::QueueActivityBaseline)&        QueueActivityBaseline    = Profile->QueueActivityBaseline;
		decltype(github_profile_runtime::QueueActivity)&                QueueActivity            = Profile->QueueActivity;
		decltype(github_profile_runtime::ActiveQueueNotifications)&     ActiveQueueNotifications = Profile->ActiveQueueNotifications;
		decltype(github_profile_runtime::GithubProjectionFanOut)&       GithubProjectionFanOut   = Profile->GithubProjectionFanOut;
		decltype(github_profile_runtime::Host)&                         Host                     = Profile->Host;
		std::optional<application::workbench_selected_diff_request>     SelectedDiffRequest;
		std::optional<application::workbench_selected_diff_publication> SelectedDiffPublication;
		/// One automatic receipt attempt for each successfully opened selection.
		std::optional<application::workbench_selected_diff_request> AutoSeenPresentedRequest;
		/// The exact source opened by a Tour gesture; never inferred from tray visibility.
		std::optional<application::workbench_selected_diff_request> TourSourceRequest;
		/// [[BPR-1212]]. The last question asked about the selected comparison's
		/// base numbering, and its answer once the epoch host gave one.
		struct base_numbering_state
		{
			application::github_base_numbering_request               Request;
			std::optional<application::github_base_numbering_status> Status;
		};
		std::optional<base_numbering_state> BaseNumbering;
		/// [[BPR-1212]]. Cancelled answers in a row for the asked question.
		std::size_t BaseNumberingCancellations = 0U;
		struct agent_review_facts
		{
			core::git_host_id                Host;
			core::github_account_id          Account;
			core::github_identity_generation Generation;
			core::repository_id              Repository;
			core::pull_request_id            PullRequest;
			core::pull_request_number        Number;
			std::string                      Hostname;
			core::repository_locator         Locator;
			/// [[BPR-1873]]. Read with selected objects through a lease that
			/// can: the review holds no blobs, so without a clone its agent sees
			/// only the pull request's files.
			bool Sparse = false;
			/// [[BPR-1879]]. GitHub's repository `size`, in bytes, when it said.
			std::optional<std::uint64_t> ApproximateBytes;
		};
		std::optional<agent_review_facts>                   AgentReview;
		decltype(github_profile_runtime::ExistingCheckout)& ExistingCheckout      = Profile->ExistingCheckout;
		bool                                                Closing               = false;
		bool                                                OtherShutdownBegun    = false;
		bool                                                ShutdownPollScheduled = false;
		bool                                                ShutdownComplete      = false;
		bool                                                ShutdownCleanupFailed = false;
		// [[BPR-0875]]. Which cleanup refused, where, and what the platform said.
		// The first refusal keeps the record: a later one cannot overwrite the
		// cause that started the failure.
		std::string                                                       ShutdownCleanupDetail;
		void                                                              note_cleanup_failure(std::string aDetail);
		decltype(github_profile_runtime::AgentWorkspaceCleanupAttempted)& AgentWorkspaceCleanupAttempted = Profile->AgentWorkspaceCleanupAttempted;
		decltype(github_profile_runtime::AgentWorkspaceCleanupFailed)&    AgentWorkspaceCleanupFailed    = Profile->AgentWorkspaceCleanupFailed;
		decltype(github_profile_runtime::NativePlatformVaultSelected)&    NativePlatformVaultSelected    = Profile->NativePlatformVaultSelected;
		bool                                                              EpochProjectionDeliveryFailed  = false;
		// Classifications the panes actually took. Counted because "the
		// classifier is linked" and "a pane received its answer" are different
		// claims, and only the second one shows a reviewer anything.
		std::size_t AdoptedClassifications = 0U;
		std::size_t DroppedClassifications = 0U;
		// Adoptions taken on the display turn from what the host already held,
		// counted apart from the publication path so neither number absorbs the
		// other. BPR-0390.
		std::size_t DisplayAdoptedClassifications = 0U;
		// Two blobs per comparison, so eight comparisons' worth of remembered
		// definition choices. The oldest is dropped past that; a blob whose
		// choice has been forgotten is simply not adopted on display, and its
		// classification still publishes through the ordinary path.
		static constexpr auto MaximumClassifiedDefinitions = std::size_t{ 16U };
		std::vector<
			std::pair<core::git_blob_oid, application::syntax_definition_version>>
			ClassifiedDefinitions;
		// Times the presentation profile refused typography the host accepted.
		// Both validate with the same core function, so this must stay zero;
		// it exists so that if it ever does not, something says so.
		std::size_t                                                      AppearanceDivergences         = 0U;
		decltype(github_profile_runtime::NotificationSink)&              NotificationSink              = Profile->NotificationSink;
		decltype(github_profile_runtime::SystemNotificationSink)&        SystemNotificationSink        = Profile->SystemNotificationSink;
		decltype(github_profile_runtime::Notifications)&                 Notifications                 = Profile->Notifications;
		decltype(github_profile_runtime::NotificationRequestsAttempted)& NotificationRequestsAttempted = Profile->NotificationRequestsAttempted;
		decltype(github_profile_runtime::NotificationSelections)&        NotificationSelections        = Profile->NotificationSelections;
		decltype(github_profile_runtime::NotificationSelectionFailures)& NotificationSelectionFailures = Profile->NotificationSelectionFailures;
#if defined(BPR_BUILD_TESTING)
		decltype(github_profile_runtime::FixtureNotificationSink)&          FixtureNotificationSink          = Profile->FixtureNotificationSink;
		decltype(github_profile_runtime::UseGrantedNotificationSink)&       UseGrantedNotificationSink       = Profile->UseGrantedNotificationSink;
		decltype(github_profile_runtime::CaptureSystemNotificationDisplay)& CaptureSystemNotificationDisplay = Profile->CaptureSystemNotificationDisplay;
		decltype(github_profile_runtime::DisableNotificationRequestWire)&   DisableNotificationRequestWire   = Profile->DisableNotificationRequestWire;
		decltype(github_profile_runtime::DisableNotificationSelectionWire)& DisableNotificationSelectionWire = Profile->DisableNotificationSelectionWire;
		decltype(github_profile_runtime::CapturedNotificationDisplays)&     CapturedNotificationDisplays     = Profile->CapturedNotificationDisplays;

		/// The last turn this composition handed to the launch path, kept so a
		/// course can read what was dispatched. It cannot be read back off the
		/// chat host afterwards: the launch path admits a session of its own,
		/// so by the time a turn has run the host is holding a different one.
		std::optional<application::agent_chat_turn_request> LastChatTurn;
		std::optional<core::supplement_conversation>        LastAgentConversation;
#endif
	};

	/// [[BPR-1867]]. How soon a reread waiting for the reviewer to stop
	/// writing asks again.
	constexpr auto reread_writing_retry = std::chrono::milliseconds{ 2'000 };
}
#endif
