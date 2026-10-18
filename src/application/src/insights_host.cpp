#include "bpr/application/insights_host.hpp"

#include <utility>

namespace bpr::application {
	insights_host::insights_host(std::shared_ptr<insights_storage_port> aStorage, effect_executor& aEffects, decision_executor& aDecisions,
								 std::function<void(const insights_snapshot&)> aPublish)
		: Storage{ std::move(aStorage) }
		, Effects{ aEffects }
		, Decisions{ aDecisions }
		, Publish{ std::move(aPublish) }
	{ }

	void insights_host::refresh(const bool aStorageVisible, std::string aBackgroundWork)
	{
		if (Closing || Pending) {
			return;
		}
		const auto now     = std::chrono::steady_clock::now();
		const auto measure = aStorageVisible && (!LastStorage || now - *LastStorage >= std::chrono::seconds{ 60 });
		Pending            = true;
		try {
			Effects.post([self = shared_from_this(), measure, now, work = std::move(aBackgroundWork)] {
				auto snapshot = insights_snapshot{ };
				if (measure) {
					try {
						snapshot = self->Storage->measure();
					} catch (...) { }
				}
				const auto built = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				self->Decisions.post([self, measure, now, built, measuredSnapshot = std::move(snapshot), work]() mutable {
					self->Pending = false;
					if (self->Closing) {
						return;
					}
					if (measure) {
						self->Snapshot                    = std::move(measuredSnapshot);
						self->Snapshot.StorageUnixSeconds = built;
						self->LastStorage                 = now;
					}
					self->Snapshot.BuiltUnixSeconds = built;
					self->Snapshot.BackgroundWork   = work;
					self->Publish(self->Snapshot);
				});
			});
		} catch (...) {
			Pending = false;
		}
	}

	void insights_host::shutdown() noexcept { Closing = true; }
	auto insights_host::quiescent() const noexcept -> bool { return !Pending; }
}
