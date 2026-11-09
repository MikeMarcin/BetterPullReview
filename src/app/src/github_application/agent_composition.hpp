#ifndef BPR_APP_GITHUB_APPLICATION_AGENT_COMPOSITION_HPP
#define BPR_APP_GITHUB_APPLICATION_AGENT_COMPOSITION_HPP
#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/private_review_application_host.hpp"
#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_finding_promotion.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "skill_runs.hpp"

#include <QString>

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/agent_chat_session_host.hpp"
	#include "bpr/application/agent_comment_workshop_host.hpp"
	#include "bpr/application/agent_finding_host.hpp"
	#include "bpr/core/agent_finding.hpp"
	#include "bpr/qt/finding_queue_view.hpp"
#endif
#if defined(BPR_BUILD_TESTING) && defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/agent/agent_process_launcher.hpp"
	#include <atomic>
	#include <cstddef>
	#include <stop_token>
#endif

// Agent integration as the composition root wires it: invocation, chat,
// finding and comment-workshop projections, tour sessions, frozen context
// and workspace refusal text.
namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	/// [[BPR-1752]], RFC-BPR-0092 D1. Writes the edited review skill
	/// to a new file in Downloads (else Documents or home) before Restore
	/// default or Use version N removes it, and names the file. Never
	/// replaces a file that is already there.
	[[nodiscard]] auto save_review_skill_copy(const std::string& aText) -> std::optional<std::string>;
#endif
	// [[BPR-0875]]. The workspace-ownership refusal in words, for the one
	// cleanup name a quit reports.
	[[nodiscard]] auto named_workspace_ownership_failure(const int aFailure) -> std::string;

	/// [[BPR-1857]]. What a startup refusal says to the reviewer: which check
	/// refused, the folder or file it refused, and what they can do. The
	/// general sentence said "needs repair or has reached its limit" for a
	/// dozen different findings and named no folder. Nothing here is moved,
	/// changed or deleted; the reviewer decides.
	[[nodiscard]] auto agent_workspace_refusal_text(const adapters::git::workspace_recovery_refusal& aRefusal) -> QString;
#if defined(BPR_BUILD_TESTING) && defined(BPR_AGENT_INTEGRATIONS)
	/// \brief Observes the real composed launcher without replacing its effect.
	class composition_agent_process_launcher_probe final
		: public application::agent_process_port
	{
	public:
		explicit composition_agent_process_launcher_probe(
			adapters::agent::agent_process_launcher& aLauncher) noexcept;

		[[nodiscard]] auto run(
			const application::agent_launch_request& aRequest,
			const int                                aTimeoutMilliseconds,
			const application::agent_output_sink&    aSink = { },
			const std::stop_token                    aStop = { })
			-> application::agent_run_result override;

		void cancel() noexcept override;

		[[nodiscard]] auto invocations() const noexcept -> std::size_t;

		[[nodiscard]] auto native_connections_quiescent() const noexcept -> bool override { return Launcher.native_connections_quiescent(); }

	private:
		adapters::agent::agent_process_launcher& Launcher;
		std::atomic_size_t                       Invocations = 0U;
	};
#endif
	/// \brief Names one core promotion refusal for a reviewer to read.
	///
	/// \details Presentation wording lives at this boundary because the core
	/// publishes none: `core::agent_finding_promotion_refusal` is a decision,
	/// and how it is worded to a reviewer is not the core's business. Every
	/// declared enumerator has a case, so a refusal added later fails to
	/// compile here rather than reaching the reviewer as an empty string.
	[[nodiscard]] constexpr auto promotion_refusal_name(
		const core::agent_finding_promotion_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case core::agent_finding_promotion_refusal::none:
			return "none";
		case core::agent_finding_promotion_refusal::unknown_choice:
			return "unknown_choice";
		case core::agent_finding_promotion_refusal::dismissed:
			return "dismissed";
		case core::agent_finding_promotion_refusal::unanchored:
			return "unanchored";
		case core::agent_finding_promotion_refusal::stale:
			return "stale";
		case core::agent_finding_promotion_refusal::foreign_verdict:
			return "foreign_verdict";
		case core::agent_finding_promotion_refusal::malformed_finding:
			return "malformed_finding";
		case core::agent_finding_promotion_refusal::unnamed_target:
			return "unnamed_target";
		case core::agent_finding_promotion_refusal::anchor_elsewhere:
			return "anchor_elsewhere";
		case core::agent_finding_promotion_refusal::boundary_anchor:
			return "boundary_anchor";
		case core::agent_finding_promotion_refusal::no_suggested_comment:
			return "no_suggested_comment";
		}
		return "unknown";
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	/// \brief The Qt action a composed finding gesture asks the host for.
	[[nodiscard]] constexpr auto composed_finding_action(
		const qt::finding_queue_action aAction) noexcept
		-> application::agent_finding_action
	{
		switch (aAction) {
		case qt::finding_queue_action::open_evidence:
			return application::agent_finding_action::open_evidence;
		case qt::finding_queue_action::ask_follow_up:
			return application::agent_finding_action::ask_follow_up;
		case qt::finding_queue_action::convert_to_draft_comment:
			return application::agent_finding_action::convert_to_draft_comment;
		case qt::finding_queue_action::convert_to_draft_defect:
			return application::agent_finding_action::convert_to_draft_defect;
		case qt::finding_queue_action::add_to_private_notes:
			return application::agent_finding_action::add_to_private_notes;
		case qt::finding_queue_action::dismiss:
			return application::agent_finding_action::dismiss;
		}
		return application::agent_finding_action::open_evidence;
	}

	/// \brief Writes one frozen review projection the MCP helper may serve.
	///
	/// \details The same values the invocation request freezes for anchor
	/// validation, written once so the helper answers about exactly what the
	/// agent was asked about. The layout is the one
	/// `adapters::mcp::mcp_context_helper` reads: a `binding` line, an
	/// `alignment` record, and a `files` directory the helper resolves inside.
	[[nodiscard]] auto freeze_mcp_snapshot(
		const std::filesystem::path&                          aRoot,
		const std::string&                                    aBinding,
		const std::vector<core::agent_finding_snapshot_file>& aFiles) -> bool;
#endif
	/// \brief [[BPR-1900]]. How a tour session run launches: read-only in the
	/// workspace, and the paths whose lines the host counts first.
	struct tour_launch
	{
		bool                     ReadOnly = false;
		std::vector<std::string> Measure;
		/// [[BPR-1916]]. The skills the run's prompt names, inline.
		skill_run Skills;
	};

	/// \brief [[BPR-1900]]. The tour session's projection: its own, so a tour
	/// run never reaches the Agent tab's launch panel or chat, and nothing of
	/// the chat reaches here. Offers and the session catalog are the Agent
	/// tab's, so they are read past.
	class tour_session_projection final : public application::agent_invocation_projection_port
	{
	public:
		using identity_sink = std::function<void(const application::agent_invocation_projection_identity&)>;
		using start_sink    = std::function<void(const application::agent_invocation_projection_identity&, const application::prepared_workspace&)>;
		using run_sink      = std::function<void(const application::agent_invocation_projection_identity&, const application::agent_run_result&)>;
		using refuse_sink   = std::function<void(std::string_view)>;

		/// \brief [[BPR-1941]]. `aStream` hears each output event of the run
		/// in flight, for the tour chat's live line; none ignores them.
		tour_session_projection(identity_sink aAdmit, start_sink aStart, run_sink aFinish, refuse_sink aRefuse, identity_sink aStream = { });
		void offer(std::span<const core::agent_profile>) override;
		void refuse(const std::string_view aReason) override;
		void admit(const application::agent_invocation_projection_identity& aIdentity) override;
		void start(const application::agent_invocation_projection_identity& aIdentity, const application::prepared_workspace& aWorkspace) override;
		void stream(const application::agent_invocation_projection_identity&, const application::agent_stream_event&) override;
		void finish(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult) override;
		void present_session_catalog(const application::agent_session_catalog_projection&) override;

	private:
		identity_sink Admit;
		start_sink    Start;
		run_sink      Finish;
		refuse_sink   Refuse;
		identity_sink Stream;
	};

	/// \brief Carries every invocation projection onward, findings included.
	///
	/// \details The launch panel stays the invocation surface it already is;
	/// this adds one edge, from the host's typed finding delivery to the
	/// composed finding host. It is deliberately the *only* new edge out of the
	/// agent host: there is no path from here to the review-mutation
	/// coordinator, and RFC-BPR-0012's "no private backchannel" is that absence
	/// rather than a check somewhere downstream.
	class composition_agent_invocation_projection final
		: public application::agent_invocation_projection_port
	{
	public:
		using finding_sink   = std::function<void(
			const application::agent_invocation_projection_identity&,
			const application::agent_finding_delivery&)>;
		using session_sink   = std::function<void(const core::agent_session&)>;
		using admit_sink     = std::function<void(
			const application::agent_invocation_projection_identity&)>;
		using workspace_sink = std::function<void(const application::prepared_workspace&)>;
		using start_sink     = std::function<void(
			const application::agent_invocation_projection_identity&)>;
		/// \brief BPR-0538. One turn, ended, with how it ended.
		///
		/// \details A press learns that its turn is over from the turn itself. A poll
		/// would have to decide how often to ask and would still be wrong
		/// between two asks.
		using run_sink = std::function<void(
			const application::agent_invocation_projection_identity&,
			const application::agent_run_result&)>;

		composition_agent_invocation_projection(
			application::agent_invocation_projection_port& aPanel,
			finding_sink                                   aFindings,
			session_sink                                   aSessions,
			admit_sink                                     aAdmissions,
			run_sink                                       aRuns,
			workspace_sink                                 aWorkspaces,
			start_sink aStarts, std::function<void(std::string_view)> aRefused);

		void offer(const std::span<const core::agent_profile> aProfiles) override;

		void set_offer_filter(std::function<std::vector<core::agent_profile>(std::span<const core::agent_profile>)> aFilter);

		void refuse(const std::string_view aReason) override;

		void admit(
			const application::agent_invocation_projection_identity& aIdentity)
			override;

		void start(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::prepared_workspace&                   aWorkspace) override;

		void stream(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_stream_event&                   aEvent) override;

		void finish(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_run_result&                     aResult) override;

		void deliver_findings(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_finding_delivery&               aDelivery) override;

		void restore_native_session(const core::agent_session& aSession) override;
		void present_session_catalog(
			const application::agent_session_catalog_projection& aProjection)
			override;

		void set_private_sink(std::function<void(const application::private_review_delta&)> aChanged, std::function<void(const application::private_run_error&)> aFailed);
		void private_changed(const application::agent_invocation_projection_identity&, const application::private_review_delta& aDelta) override;
		void private_refused(const application::agent_invocation_projection_identity&, const application::private_run_error& aError) override;

	private:
		std::function<void(const application::private_review_delta&)>                         PrivateChanged;
		std::function<void(const application::private_run_error&)>                            PrivateFailed;
		application::agent_invocation_projection_port&                                        Panel;
		finding_sink                                                                          Findings;
		session_sink                                                                          Sessions;
		admit_sink                                                                            Admissions;
		run_sink                                                                              Runs;
		workspace_sink                                                                        Workspaces;
		start_sink                                                                            Starts;
		std::function<void(std::string_view)>                                                 Refused;
		std::function<std::vector<core::agent_profile>(std::span<const core::agent_profile>)> OfferFilter;
	};
#if defined(BPR_AGENT_INTEGRATIONS)
	/// \brief Forwards chat state and chat outcomes to their surfaces.
	///
	/// \details The same shape `composition_agent_finding_projection` already
	/// uses: the host decides, this carries the decision to the widget that
	/// shows it. There is no branch here that starts anything.
	class composition_agent_chat_projection final
		: public application::agent_chat_projection_port
	{
	public:
		using state_sink
			= std::function<void(const application::agent_chat_projection&)>;
		using outcome_sink
			= std::function<void(const application::agent_chat_outcome&)>;
		using clear_sink = std::function<void()>;

		composition_agent_chat_projection(
			state_sink aState, outcome_sink aOutcome, clear_sink aClear);

		void present(
			const application::agent_chat_projection& aProjection) override;

		void present(const application::agent_chat_outcome& aOutcome) override;

		void clear() override;

	private:
		state_sink   State;
		outcome_sink Outcome;
		clear_sink   Clear;
	};

	/// \brief Forwards finding deliveries and outcomes to their surfaces.
	class composition_agent_finding_projection final
		: public application::agent_finding_projection_port
	{
	public:
		using delivery_sink
			= std::function<void(const application::agent_finding_delivery&)>;
		using outcome_sink
			= std::function<void(const application::agent_finding_outcome&)>;
		using clear_sink = std::function<void()>;

		composition_agent_finding_projection(
			delivery_sink aDelivered, outcome_sink aAnswered, clear_sink aCleared);

		void present(const application::agent_finding_delivery& aDelivery) override;

		void present(const application::agent_finding_outcome& aOutcome) override;

		void clear() override;

	private:
		delivery_sink Delivered;
		outcome_sink  Answered;
		clear_sink    Cleared;
	};
#endif
	/// \brief One comment-workshop press, and the turn it started.
	///
	/// \details BPR-0533. The reviewer's question, held from the press until
	/// the answer to *that* turn arrives. `Draft` and `Operation` are what was
	/// asked and about what; `Session` is which agent turn was started to
	/// answer it, learned from the one place a session identity is allocated
	/// and absent only in the window between the dispatch and that admission.
	///
	/// The session is what makes the pairing explicit. Without it, the next
	/// revision to arrive from anywhere — a finding a reviewer promoted while
	/// the workshop turn was still running — would be labelled with an
	/// operation that never produced it, and the reviewer would read a
	/// before/after answering a question nobody asked about that text.
	struct pending_comment_workshop
	{
		core::review_draft_id                 Draft;
		core::workshop_operation              Operation = core::workshop_operation::clarify;
		std::optional<core::agent_session_id> Session;

		/// BPR-0538. True while the turn this press started is still running.
		/// It is what shuts the six controls, and it stops being true when
		/// that turn ends — not when its answer is converted, which may never
		/// happen. A record that outlives its turn still names which operation
		/// the outcome came from; it no longer claims a turn is running.
		bool Running = true;

		/// True once that turn has delivered a finding of its own: something
		/// the reviewer can still convert into the revision this press asked
		/// for. A turn that ends without one has produced no revision and none
		/// is coming, which is the whole of what this flag decides.
		bool Delivered = false;

		/// [[BPR-1245]]. True while the press's draft is being recorded in
		/// Private history, before any turn is dispatched for it. A session
		/// admitted then is another gesture's and is not taken.
		bool Recording = true;

		/// [[BPR-1526]]. A first save replaces this exact placeholder before
		/// the press can record it. Its acknowledgement, never another
		/// draft's later save, supplies the durable identity.
		std::optional<core::revisioned_review_draft> AwaitingMaterialization;

		auto operator==(const pending_comment_workshop&) const -> bool = default;
	};
#if defined(BPR_AGENT_INTEGRATIONS)
	/// \brief Forwards one proposed revision, or the reason there is none.
	class composition_comment_workshop_projection final
		: public application::agent_comment_workshop_projection_port
	{
	public:
		using proposal_sink = std::function<void(
			const application::agent_comment_workshop_proposal&)>;
		using refusal_sink
			= std::function<void(application::agent_comment_workshop_refusal)>;

		composition_comment_workshop_projection(
			proposal_sink aProposed, refusal_sink aRefused);

		void present(
			const application::agent_comment_workshop_proposal& aProposal) override;

		void refuse(
			const application::agent_comment_workshop_refusal aRefusal) override;

	private:
		proposal_sink Proposed;
		refusal_sink  Refused;
	};
#endif
}
#endif
