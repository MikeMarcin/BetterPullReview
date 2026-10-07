#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_PREFERENCES_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_PREFERENCES_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include <memory>

namespace bpr::application {
	/// One profile owns durable behaviour and defaults; consumers retain geometry.
	/// Settings and executors must outlive shutdown followed by quiescence.
	/// One active write and one coalesced candidate bound pending edits.
	class workbench_preferences_host final
	{
	public:
		using consumer = std::function<void(const workbench_profile_preferences&)>;
		workbench_preferences_host(workbench_settings_port& aSettings, effect_executor& aEffects,
								   decision_executor& aDecision, core::bpr_profile_id aProfile = core::default_bpr_profile_id);
		~workbench_preferences_host();
		workbench_preferences_host(const workbench_preferences_host&)                                  = delete;
		auto               operator=(const workbench_preferences_host&) -> workbench_preferences_host& = delete;
		[[nodiscard]] auto subscribe(core::bpr_profile_id aProfile, consumer aConsumer) -> std::uint64_t;
		void               unsubscribe(std::uint64_t aSubscription);
		void               restore();
		/// Compatibility defaults load is already a durable observation.
		void               adopt_loaded(const workbench_profile_preferences& aPreferences);
		[[nodiscard]] auto apply_behaviour(review_behaviour_settings aBehaviour) -> bool;
		[[nodiscard]] auto apply_defaults(workbench_window_layout aDefaults) -> bool;
		[[nodiscard]] auto apply_writing_hook(core::writing_hook aHook, std::string aInstructions) -> bool;
		/// [[BPR-1301]]. Writes the profile's Claude process wrapper, empty to
		/// clear it. Only storable text is admitted; whether the file exists
		/// and runs is asked before this, on the effect lane, and again at
		/// every launch.
		[[nodiscard]] auto apply_claude_wrapper(std::string aPath) -> bool;
		void               cancel();
		void               shutdown();
		[[nodiscard]] auto accepted() const noexcept -> const std::optional<workbench_profile_preferences>&;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto is_busy() const noexcept -> bool;
		[[nodiscard]] auto delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
