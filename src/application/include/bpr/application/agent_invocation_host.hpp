#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_INVOCATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_INVOCATION_HOST_HPP

#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/private_review_application_host.hpp"
#include "bpr/core/agent_profile.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace bpr::application {
	struct agent_invocation_request
	{
		core::application_session_id        ApplicationSession;
		core::pull_request_authority        Authority;
		core::agent_profile_id              Profile;
		agent_context_observation           Context;
		bool                                ExistingCheckoutChosen = false;
		std::optional<checkout_observation> ExistingCheckout;
		/// The frozen per-path alignment facts a returned finding is checked
		/// against: the comparison each path was aligned under, its line counts
		/// on both sides, and the segment, group, and move identities that
		/// alignment contained.
		///
		/// Stated here, as values, rather than read from a live alignment when
		/// a result arrives. That is the whole point: an alignment consulted at
		/// delivery is whichever one exists then, and measuring an older
		/// answer against it is the silent re-anchoring RFC-BPR-0012 forbids.
		/// The host pairs these with the admitted session's own binding to make
		/// the `core::agent_finding_snapshot` it validates against; the binding
		/// is deliberately not caller-supplied, so a caller cannot name a
		/// revision the session was not started against.
		///
		/// An empty list is a complete answer, not a missing one: every finding
		/// then arrives unanchored, which is what a caller that froze nothing
		/// has evidence for.
		std::vector<core::agent_finding_snapshot_file> FrozenFiles;
		/// The already-admitted session this turn continues, when it continues
		/// one instead of starting one.
		///
		/// \details RFC-BPR-0012's `continue_against_old_revision`. A reviewer
		/// who chooses it is asking about the revision their session was bound
		/// to, which is no longer the revision under review, so the host may
		/// not answer under a session bound to the head that moved.
		///
		/// Engaged, this turn runs under exactly this session: no identity is
		/// allocated, and the frozen snapshot's `Binding` is read out of this
		/// value. That keeps the property the empty case has for a different
		/// reason — **the snapshot's binding is the session's binding, always**
		/// — while letting the session be an older one. What a caller may not
		/// do is disagree with itself: `Context.Binding` must equal this
		/// session's own `Binding`, and a turn whose two revisions differ is
		/// refused as `agent_continuation_binding_mismatch` rather than run
		/// under either of them.
		///
		/// Absent is the ordinary case and the only one that reaches the
		/// catalog: the turn is about the revision under review, the host
		/// admits a new identity, and a caller-named revision is refused as
		/// `stale_or_mismatched_agent_context` exactly as before.
		std::optional<core::agent_session> ContinuedSession;
		bool                               WritingOnly = false;
		core::agent_setting_facts          RequestedSettings;
		std::optional<private_run_intent>  PrivateRun;
		/// [[BPR-PENDING-AQ4]], RFC-BPR-0091 Q7. The reviewer's linked clone,
		/// whose history the session's workspace borrows read-only.
		std::optional<std::filesystem::path> HistoryClone;

		auto operator<=>(const agent_invocation_request&) const = default;
	};

	/// \brief Exact identity admitted for one invocation's UI projections.
	struct agent_invocation_projection_identity
	{
		core::application_session_id ApplicationSession;
		core::pull_request_authority Authority;
		core::agent_session          Session;
		std::uint64_t                InvocationGeneration = 0U;

		auto operator<=>(const agent_invocation_projection_identity&) const
			= default;
	};

	/// \brief Non-sensitive latest-session presentation for the current review.
	struct agent_session_catalog_projection
	{
		std::optional<core::agent_session>        Latest;
		std::optional<core::agent_session_status> LatestStatus;
		std::optional<core::agent_session_id>     Next;

		auto operator<=>(const agent_session_catalog_projection&) const = default;
	};

	class agent_invocation_projection_port
	{
	public:
		virtual ~agent_invocation_projection_port() = default;

		virtual void offer(std::span<const core::agent_profile> aProfiles) = 0;
		virtual void refuse(std::string_view aReason)                      = 0;
		virtual void admit(
			const agent_invocation_projection_identity& aIdentity) = 0;
		virtual void start(
			const agent_invocation_projection_identity& aIdentity,
			const prepared_workspace&                   aWorkspace) = 0;
		virtual void stream(
			const agent_invocation_projection_identity& aIdentity,
			const agent_stream_event&                   aEvent) = 0;
		virtual void finish(
			const agent_invocation_projection_identity& aIdentity,
			const agent_run_result&                     aResult) = 0;
		/// \brief Reports one run's validated findings on the completion lane.
		///
		/// \details Called at most once per run, immediately before `finish`,
		/// and only when the turn returned a payload. The delivery carries the
		/// session's own binding and one core verdict per finding; a
		/// presentation reads verdicts rather than deciding them.
		///
		/// Deliberately not pure. Presenting findings is RFC-BPR-0012's widget
		/// slice, and making every existing implementor answer a question it
		/// has no queue for would be a change to presentation dressed as a
		/// change to the host. The default presents nothing and refuses
		/// nothing.
		virtual void deliver_findings(
			const agent_invocation_projection_identity& aIdentity,
			const agent_finding_delivery&               aDelivery)
		{
			static_cast<void>(aIdentity);
			static_cast<void>(aDelivery);
		}
		virtual void private_changed(const agent_invocation_projection_identity&, const private_review_delta&) { }
		virtual void private_refused(const agent_invocation_projection_identity&, const private_run_error&) { }
		virtual void present_session_catalog(
			const agent_session_catalog_projection& aProjection) = 0;
	};

	struct agent_invocation_host_statistics
	{
		std::uint64_t CoreDecisions            = 0U;
		std::uint64_t WorkspaceStarts          = 0U;
		std::uint64_t ProcessStarts            = 0U;
		std::uint64_t WorkspaceCleanups        = 0U;
		std::uint64_t WorkspaceCleanupFailures = 0U;
		std::uint64_t SessionAdmissions        = 0U;
		std::uint64_t SessionAdmissionFailures = 0U;

		auto operator<=>(const agent_invocation_host_statistics&) const = default;
	};

	/// \brief Composes core legality, exact context, workspace and child effects.
	class agent_invocation_host final
	{
	public:
		agent_invocation_host(
			std::vector<core::agent_profile>     aProfiles,
			std::vector<core::agent_host_policy> aPolicies,
			std::filesystem::path                aWorkspaceParent,
			agent_session_catalog_port&          aSessions,
			agent_workspace_port&                aWorkspaces,
			agent_process_port&                  aProcesses,
			environment_reader                   aEnvironment,
			effect_executor&                     aEffects,
			decision_executor&                   aDecisionLane,
			agent_invocation_projection_port&    aProjection,
			private_agent_run_port*              aPrivateRuns = nullptr);
		~agent_invocation_host();

		agent_invocation_host(const agent_invocation_host&) = delete;
		auto operator=(const agent_invocation_host&)
			-> agent_invocation_host& = delete;

		void observe(
			core::application_session_id aApplicationSession,
			core::pull_request_authority aAuthority,
			core::agent_revision_binding aBinding);
		void clear();
		void dispatch(agent_invocation_request aRequest);
		void cancel();
		void shutdown();
		void retry_cleanup();

		/// \brief [[BPR-PENDING-AQ4]], RFC-BPR-0091 Check it works. Makes the
		/// workspace an Ask on this binding would make, through the same
		/// workspace port, then removes it, and answers on the decision lane.
		/// \details No session is admitted and no agent runs: the probe's
		/// session identity is from a reserved range no durable session uses.
		/// It counts toward quiescence until it answers.
		using workspace_probe_answer = std::function<void(std::expected<prepared_workspace, agent_workspace_error>)>;
		void probe_workspace(core::application_session_id aApplicationSession, core::pull_request_authority aAuthority, core::agent_revision_binding aBinding,
							 std::optional<std::filesystem::path> aHistoryClone, workspace_probe_answer aAnswer);

		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto statistics() const noexcept
			-> agent_invocation_host_statistics;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_INVOCATION_HOST_HPP
