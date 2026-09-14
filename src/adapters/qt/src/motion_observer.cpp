#include "bpr/qt/motion_observer.hpp"

#include <QMetaObject>

#include <algorithm>
#include <limits>
#include <utility>

namespace bpr::qt {
	motion_observer::motion_observer(
		std::unique_ptr<motion_observation_source> aSource, QObject* aParent)
		: QObject{ aParent }
		, Source{ std::move(aSource) }
	{
		if (!Source) {
			return;
		}
		const auto self = QPointer<motion_observer>{ this };
		Source->set_change_handler([self] {
			if (self) {
				QMetaObject::invokeMethod(self, [self] {
					if (self) {
						self->refresh();
					} }, Qt::QueuedConnection);
			}
		});
		Current = Source->read();
	}

	motion_observer::~motion_observer()
	{
		if (Source) {
			Source->set_change_handler({ });
		}
		Listeners.clear();
	}

	auto motion_observer::subscribe(QObject& aLifetime, sink aSink) -> std::uint64_t
	{
		if (!aSink || NextId == 0U || NextId == std::numeric_limits<std::uint64_t>::max()) {
			return 0U;
		}
		const auto id = NextId++;
		Listeners.push_back(listener{ id, &aLifetime, std::move(aSink) });
		Listeners.back().Publish(Current);
		return id;
	}

	void motion_observer::unsubscribe(const std::uint64_t aId) noexcept
	{
		std::erase_if(Listeners, [aId](const listener& aListener) {
			return aListener.Id == aId || !aListener.Lifetime;
		});
	}

	auto motion_observer::current() const noexcept -> core::motion_observation
	{
		return Current;
	}

	void motion_observer::refresh()
	{
		if (!Source) {
			return;
		}
		const auto observation = Source->read();
		if (observation == Current) {
			return;
		}
		Current = observation;
		std::erase_if(Listeners, [](const listener& aListener) {
			return !aListener.Lifetime;
		});
		const auto listeners = Listeners;
		const auto self      = QPointer<motion_observer>{ this };
		for (const auto& item : listeners) {
			if (!self) {
				return;
			}
			if (item.Lifetime && std::ranges::any_of(Listeners, [id = item.Id](const listener& aListener) {
					return aListener.Id == id;
				})) {
				item.Publish(observation);
			}
		}
	}

#if !defined(__APPLE__)
	namespace {
		class unsupported_motion_source final : public motion_observation_source
		{
		public:
			[[nodiscard]] auto read() noexcept -> core::motion_observation override
			{
				return core::motion_observation::unsupported;
			}
			void set_change_handler(std::function<void()>) override { }
		};
	} // namespace

	auto make_native_motion_source() -> std::unique_ptr<motion_observation_source>
	{
		return std::make_unique<unsupported_motion_source>();
	}
#endif
} // namespace bpr::qt
