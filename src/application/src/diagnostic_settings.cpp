#include "bpr/application/diagnostic_settings.hpp"
#include <utility>
namespace bpr::application {
	struct diagnostic_settings_host::implementation : std::enable_shared_from_this<implementation>
	{
		diagnostic_control_port&       Control;
		diagnostic_settings_port&      Settings;
		effect_executor&               Effects;
		decision_executor&             Decisions;
		diagnostic_settings_projection Projection;
		observer                       Observer;
		bool                           Closing = false;
		implementation(diagnostic_control_port& aControl, diagnostic_settings_port& aSettings, effect_executor& aEffects,
					   decision_executor& aDecisions, diagnostic_preference_load aLoaded)
			: Control{ aControl }
			, Settings{ aSettings }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
		{
			Projection.Capture.Preferences = aLoaded.Preferences;
			Projection.Writable            = aLoaded.Available;
			Projection.Recovered           = aLoaded.Recovered;
		}
		void publish() noexcept
		{
			try {
				if (Observer) {
					Observer(Projection);
				}
			} catch (...) { }
		}
		void submit(std::function<diagnostic_control_result()> aOperation)
		{
			if (Closing || Projection.Busy) {
				return;
			}
			Projection.Busy = true;
			publish();
			try {
				Effects.post([self = shared_from_this(), operation = std::move(aOperation), before = Projection.Capture] {
					auto result  = diagnostic_control_result::refused;
					auto capture = before;
					try {
						result  = operation();
						capture = self->Control.capture_state();
					} catch (...) { }
					self->Decisions.post([self, result, capture] {
						self->Projection.Capture = capture;
						if (result != diagnostic_control_result::none) {
							self->Projection.Result = result;
						}
						if (result == diagnostic_control_result::applied) {
							self->Projection.Recovered = false;
						}
						self->Projection.Busy = false;
						self->publish();
					});
				});
			} catch (...) {
				Projection.Busy   = false;
				Projection.Result = diagnostic_control_result::refused;
				publish();
			}
		}
	};
	diagnostic_settings_host::diagnostic_settings_host(diagnostic_control_port& aControl, diagnostic_settings_port& aSettings,
													   effect_executor& aEffects, decision_executor& aDecisions, diagnostic_preference_load aLoaded)
		: Impl{ std::make_shared<implementation>(aControl, aSettings, aEffects, aDecisions, aLoaded) }
	{ }
	diagnostic_settings_host::~diagnostic_settings_host() { shutdown(); }
	void diagnostic_settings_host::observe(observer aObserver)
	{
		Impl->Observer = std::move(aObserver);
		Impl->publish();
	}
	void diagnostic_settings_host::apply(diagnostic_preferences aPreferences)
	{
		if (!Impl->Projection.Writable || !valid_diagnostic_preferences(aPreferences)) {
			return;
		}
		Impl->submit([self = Impl, aPreferences] {
			const auto old = self->Control.capture_state().Preferences;
			if (!self->Settings.store(aPreferences)) {
				return diagnostic_control_result::refused;
			}
			if (!self->Control.configure(aPreferences)) {
				static_cast<void>(self->Settings.store(old));
				return diagnostic_control_result::refused;
			}
			return diagnostic_control_result::applied;
		});
	}
	void diagnostic_settings_host::clear()
	{
		Impl->submit([self = Impl] { switch (self->Control.clear()) {
                case diagnostic_clear_result::cleared: return diagnostic_control_result::cleared;
                case diagnostic_clear_result::busy: return diagnostic_control_result::clear_busy;
                case diagnostic_clear_result::unavailable: return diagnostic_control_result::clear_unavailable;
                case diagnostic_clear_result::partial: return diagnostic_control_result::clear_partial;
            }
            return diagnostic_control_result::clear_unavailable; });
	}
	void diagnostic_settings_host::open_folder()
	{
		Impl->submit([self = Impl] { return self->Settings.open_folder() ? diagnostic_control_result::opened : diagnostic_control_result::refused; });
	}
	void diagnostic_settings_host::refresh()
	{
		Impl->submit([] { return diagnostic_control_result::none; });
	}
	void diagnostic_settings_host::shutdown()
	{
		Impl->Closing  = true;
		Impl->Observer = { };
	}
	auto diagnostic_settings_host::quiescent() const -> bool { return !Impl->Projection.Busy; }
}
