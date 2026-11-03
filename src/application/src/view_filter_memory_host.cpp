#include "bpr/application/view_filter_memory_host.hpp"

#include <cstddef>
#include <utility>

namespace bpr::application {
	struct view_filter_memory_host::implementation : std::enable_shared_from_this<implementation>
	{
		view_filter_memory_port&   Store;
		effect_executor&           Effects;
		decision_executor&         Decisions;
		core::view_filter_defaults Defaults;
		std::size_t                InFlight  = 0U;
		bool                       Writable  = true;
		bool                       Recovered = false;
		bool                       Durable   = true;
		bool                       Pending   = false;
		bool                       Closing   = false;

		implementation(view_filter_memory_port& aStore, effect_executor& aEffects, decision_executor& aDecisions, const core::view_filter_memory_read& aLoaded)
			: Store{ aStore }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Defaults{ aLoaded.Defaults }
			, Writable{ aLoaded.Writable }
			, Recovered{ aLoaded.Recovered }
		{ }

		/// Posts one write of the memory as it is now.
		void post()
		{
			Pending = false;
			++InFlight;
			try {
				Effects.post([self = shared_from_this(), snapshot = Defaults] {
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

		/// One write at a time; a choice made while one runs waits, and the
		/// next write carries the latest memory.
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
	};

	view_filter_memory_host::view_filter_memory_host(view_filter_memory_port& aStore, effect_executor& aEffects, decision_executor& aDecisions, core::view_filter_memory_read aLoaded)
		: Impl{ std::make_shared<implementation>(aStore, aEffects, aDecisions, aLoaded) }
	{ }

	view_filter_memory_host::~view_filter_memory_host()
	{
		shutdown();
	}

	auto view_filter_memory_host::defaults() const noexcept -> const core::view_filter_defaults&
	{
		return Impl->Defaults;
	}

	void view_filter_memory_host::remember(const core::view_filter_remember& aChoice)
	{
		if (Impl->Closing) {
			return;
		}
		const auto next = core::remember(Impl->Defaults, aChoice);
		if (next == Impl->Defaults) {
			return;
		}
		Impl->Defaults = next;
		Impl->write();
	}

	auto view_filter_memory_host::recovered() const noexcept -> bool
	{
		return Impl->Recovered;
	}

	auto view_filter_memory_host::durable() const noexcept -> bool
	{
		return Impl->Durable;
	}

	auto view_filter_memory_host::quiescent() const noexcept -> bool
	{
		return Impl->InFlight == 0U && !Impl->Pending;
	}

	void view_filter_memory_host::shutdown() noexcept
	{
		if (Impl->Closing) {
			return;
		}
		Impl->Closing = true;
		// The effect lane runs its writes in order before it closes, so a
		// waiting choice is written now rather than lost.
		if (Impl->Pending) {
			Impl->post();
		}
	}
}
