#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REMEMBERED_CONNECTION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REMEMBERED_CONNECTION_HOST_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/remembered_connection_store.hpp"
#include <memory>
namespace bpr::application {
	enum class connection_metadata_action : std::uint8_t { remember,
														   disable,
														   forget };
	struct connection_metadata_observation
	{
		connection_metadata_action Action;
		bool                       Saved = false;
	};
	/// \brief Profile-owned serialized metadata writer with independent session and durable state.
	/// \details Construct from an off-lane load. All methods run on the decision lane;
	/// effects never mutate session selections. Each operation rebases on the last
	/// committed snapshot, so refused writes cannot skip the store's CAS revision.
	class remembered_connection_host final
	{
	public:
		using observer = std::function<void(connection_metadata_observation)>;
		remembered_connection_host(remembered_connection_store_port&                                     aStore,
								   remembered_connection_store_result<core::remembered_connection_state> aLoaded,
								   core::bpr_profile_id aProfile, effect_executor& aEffects, decision_executor& aDecisions, observer aObserver);
		~remembered_connection_host();
		[[nodiscard]] auto state() const noexcept -> const core::remembered_connection_state&;
		[[nodiscard]] auto available() const noexcept -> bool;
		auto               remember(core::remembered_connection aRecord) -> bool;
		auto               disable(std::string aHostname) -> bool;
		auto               forget(std::string aHostname) -> bool;
		/// \brief Closes ingress and drains every admitted operation without cancellation.
		void               shutdown();
		[[nodiscard]] auto quiescent() const noexcept -> bool;

	private:
		class implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
