#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <cstddef>
#include <memory>

namespace bpr::application {
	/// \brief Provides the epoch application host contract.
	class epoch_application_host final
		: public github_epoch_handoff_port,
		  public diff_blob_port,
		  public workbench_selected_diff_port,
		  public agent_workspace_port
	{
	public:
		// Semantic ingress and state/quiescence inspection are confined to the
		// serialized decision lane supplied at construction. materialize() is the
		// sole concurrent ingress seam: diff workers enter through a close-aware
		// call gate and the bounded job ledger. Worker effects otherwise communicate
		// only by posting completions to the decision lane.
		// shutdown() closes ingress without waiting. Composition keeps every
		// referenced port, executor, and projection alive until is_quiescent().
		epoch_application_host(
			core::epoch_state      aInitialState,
			git_cache_port&        aCaches,
			epoch_catalog_port&    aCatalogs,
			epoch_entropy_port&    aEntropy,
			epoch_clock_port&      aClock,
			effect_executor&       aEffects,
			decision_executor&     aDecisionLane,
			epoch_projection_port& aProjections,
			epoch_host_limits      aLimits = { });
		~epoch_application_host() override;

		epoch_application_host(const epoch_application_host&) = delete;
		auto operator=(const epoch_application_host&)
			-> epoch_application_host&                   = delete;
		epoch_application_host(epoch_application_host&&) = delete;
		auto operator=(epoch_application_host&&)
			-> epoch_application_host& = delete;

		void accept(accepted_pull_request_handoff aHandoff) override;
		// The issuer revokes its lease before notification. The epoch host also
		// revokes its retained handle idempotently before dispatching core
		// invalidation and canceling old-generation work.
		void invalidate(
			pull_request_authority_invalidation aInvalidation) override;
		void dispatch(epoch_intent aIntent);
		// The diff worker calls this synchronous concurrent seam. It is serialized
		// onto the same retained authority/cache actor used by epoch work, so no
		// competing lifetime-exclusive Git cache session is opened. shutdown()
		// closes new calls, cancels admitted work, and quiescence includes calls
		// that entered before closure.
		[[nodiscard]] auto materialize(
			const diff_blob_request& aRequest,
			std::stop_token          aStop) -> diff_blob_observation override;
		[[nodiscard]] auto materialize_agent_workspace(
			const agent_workspace_materialization_request& aRequest,
			std::stop_token                                aStop)
			-> std::expected<owned_agent_workspace, agent_workspace_error> override;
		[[nodiscard]] auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			std::stop_token              aStop)
			-> std::expected<void, agent_workspace_error> override;
		[[nodiscard]] auto context(
			const workbench_pull_request_authority& aAuthority) const
			-> std::optional<workbench_selected_diff_context> override;
		[[nodiscard]] auto resolve(
			const workbench_selected_diff_request& aRequest,
			std::stop_token                        aStop)
			-> workbench_selected_diff_observation override;
		/// \brief Authorizes one re-alignment of an older pair of this review.
		///
		/// \details BPR-0532. `context` above answers for the comparison the
		/// reviewer is looking at; this answers for the pair an agent session
		/// the reviewer kept was started against, so a turn continued after
		/// the head moved can be resolved through the same port as a fresh
		/// one. The returned context is the caller's whole warrant: the host
		/// mints it, so no caller names a comparison the host did not decide
		/// to serve, and it is bounded to this review's own host, repository,
		/// and pull request. Authorized beside `context`'s grant rather than
		/// over it, so the reviewer's own selected diff keeps resolving.
		[[nodiscard]] auto realignment_context(
			const workbench_pull_request_authority& aAuthority,
			const core::agent_revision_binding&     aBinding) const
			-> std::optional<workbench_selected_diff_context>;
		void shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::epoch_state&;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto outstanding_jobs() const noexcept -> std::size_t;
		[[nodiscard]] auto queued_actor_work() const noexcept -> std::size_t;
		[[nodiscard]] auto deferred_ordinary_ingress() const noexcept
			-> std::size_t;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		[[nodiscard]] static auto mint_selected_diff_endpoint(
			workbench_selected_diff_request aRequest,
			core::diff_side                 aSide,
			core::git_blob_oid              aBlob,
			std::filesystem::path           aPath)
			-> workbench_selected_diff_endpoint;

		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP
