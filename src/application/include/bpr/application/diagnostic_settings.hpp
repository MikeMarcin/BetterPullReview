#ifndef BPR_APPLICATION_DIAGNOSTIC_SETTINGS_HPP
#define BPR_APPLICATION_DIAGNOSTIC_SETTINGS_HPP
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/port.hpp"
#include <functional>
#include <memory>
namespace bpr::application {
	struct diagnostic_preference_load
	{
		diagnostic_preferences Preferences;
		bool                   Available = false;
		bool                   Recovered = false;
	};
	/// \brief Effect-owned profile settings and host-selected directory handoff.
	class diagnostic_settings_port
	{
	public:
		virtual ~diagnostic_settings_port()                                           = default;
		[[nodiscard]] virtual auto load() -> diagnostic_preference_load               = 0;
		[[nodiscard]] virtual auto store(diagnostic_preferences aPreferences) -> bool = 0;
		[[nodiscard]] virtual auto open_folder() -> bool                              = 0;
	};
	enum class diagnostic_control_result { none,
										   applied,
										   cleared,
										   opened,
										   refused,
										   clear_busy,
										   clear_unavailable,
										   clear_partial };
	struct diagnostic_settings_projection
	{
		diagnostic_capture_state  Capture;
		bool                      Busy      = false;
		bool                      Writable  = false;
		bool                      Recovered = false;
		diagnostic_control_result Result    = diagnostic_control_result::none;
	};
	/// \brief Serializes a single profile's effectful diagnostic controls and accepted projection.
	class diagnostic_settings_host final
	{
	public:
		using observer = std::function<void(const diagnostic_settings_projection&)>;
		diagnostic_settings_host(diagnostic_control_port& aControl, diagnostic_settings_port& aSettings,
								 effect_executor& aEffects, decision_executor& aDecisions, diagnostic_preference_load aLoaded);
		~diagnostic_settings_host();
		void               observe(observer aObserver);
		void               apply(diagnostic_preferences aPreferences);
		void               clear();
		void               open_folder();
		void               refresh();
		void               shutdown();
		[[nodiscard]] auto quiescent() const -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Impl;
	};
}
#endif
