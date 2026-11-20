#include "bpr/application/discuss_preferences_host.hpp"

#include <cstddef>
#include <utility>

namespace bpr::application {
	struct discuss_preferences_host::implementation : std::enable_shared_from_this<implementation>
	{
		discuss_preferences_port& Store;
		effect_executor&          Effects;
		decision_executor&        Decisions;
		core::discuss_preferences Preferences;
		std::size_t               InFlight  = 0U;
		bool                      Writable  = true;
		bool                      Recovered = false;
		bool                      Durable   = true;
		bool                      Pending   = false;
		bool                      Closing   = false;

		implementation(discuss_preferences_port& aStore, effect_executor& aEffects, decision_executor& aDecisions, core::discuss_preferences_read aLoaded)
			: Store{ aStore }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Preferences{ std::move(aLoaded.Preferences) }
			, Writable{ aLoaded.Writable }
			, Recovered{ aLoaded.Recovered }
		{ }

		void post()
		{
			Pending = false;
			++InFlight;
			try {
				Effects.post([self = shared_from_this(), snapshot = Preferences] {
					auto stored = false;
					try {
						stored = self->Store.store(snapshot);
					} catch (...) { }
					self->Decisions.post([self, stored] {
						--self->InFlight;
						self->Durable = stored;
						if (self->Pending && self->InFlight == 0U && !self->Closing) {
							self->post();
						}
					});
				});
			} catch (...) {
				--InFlight;
				Durable = false;
			}
		}

		void write()
		{
			if (Closing || !Writable) {
				return;
			}
			if (InFlight != 0U) {
				Pending = true;
				return;
			}
			post();
		}

		auto change(core::discuss_preferences aNext) -> bool
		{
			if (Closing || aNext == Preferences) {
				return false;
			}
			Preferences = std::move(aNext);
			write();
			return true;
		}
	};

	discuss_preferences_host::discuss_preferences_host(discuss_preferences_port& aStore, effect_executor& aEffects, decision_executor& aDecisions, core::discuss_preferences_read aLoaded)
		: Impl{ std::make_shared<implementation>(aStore, aEffects, aDecisions, std::move(aLoaded)) }
	{ }

	discuss_preferences_host::~discuss_preferences_host()
	{
		shutdown();
	}

	auto discuss_preferences_host::preferences() const noexcept -> const core::discuss_preferences&
	{
		return Impl->Preferences;
	}

	auto discuss_preferences_host::mark(const std::string_view aRepository, const std::string_view aLogin, const bool aBot) -> bool
	{
		return Impl->change(core::mark_author(Impl->Preferences, aRepository, aLogin, aBot));
	}

	auto discuss_preferences_host::recovered() const noexcept -> bool
	{
		return Impl->Recovered;
	}

	auto discuss_preferences_host::durable() const noexcept -> bool
	{
		return Impl->Durable;
	}

	auto discuss_preferences_host::quiescent() const noexcept -> bool
	{
		return Impl->InFlight == 0U && !Impl->Pending;
	}

	void discuss_preferences_host::shutdown() noexcept
	{
		if (Impl->Closing) {
			return;
		}
		Impl->Closing = true;
		if (Impl->Pending) {
			Impl->post();
		}
	}
}
