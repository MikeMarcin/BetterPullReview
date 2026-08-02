#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP

#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <memory>

namespace bpr::application {
	/// \brief Provides the epoch application host contract.
	class epoch_application_host final : public github_epoch_handoff_port
	{
	public:
		// Ingress and state/quiescence inspection are confined to the serialized
		// decision lane supplied at construction. Worker effects communicate
		// only by posting completions to that lane; this object is intentionally
		// not a general-purpose concurrently callable container.
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
			epoch_projection_port& aProjections);
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
		void shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::epoch_state&;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP
