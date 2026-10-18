#ifndef BPR_APPLICATION_INSIGHTS_HOST_HPP
#define BPR_APPLICATION_INSIGHTS_HOST_HPP

#include "bpr/application/insights_projection.hpp"
#include "bpr/application/port.hpp"

#include <chrono>
#include <memory>

namespace bpr::application {
	/// \brief Sole local measurement capability; implementations never launch a process or read remotely.
	class insights_storage_port
	{
	public:
		virtual ~insights_storage_port()                          = default;
		[[nodiscard]] virtual auto measure() -> insights_snapshot = 0;
	};

	/// \brief Serial snapshot owner. Effects run off-lane, one at a time; storage is sampled at most once per minute.
	class insights_host final : public std::enable_shared_from_this<insights_host>
	{
	public:
		insights_host(std::shared_ptr<insights_storage_port> aStorage, effect_executor& aEffects, decision_executor& aDecisions,
					  std::function<void(const insights_snapshot&)> aPublish);
		/// \brief Requests a snapshot for the current page; callers stop requesting when hidden.
		void               refresh(bool aStorageVisible, std::string aBackgroundWork);
		void               shutdown() noexcept;
		[[nodiscard]] auto quiescent() const noexcept -> bool;

	private:
		std::shared_ptr<insights_storage_port>               Storage;
		effect_executor&                                     Effects;
		decision_executor&                                   Decisions;
		std::function<void(const insights_snapshot&)>        Publish;
		insights_snapshot                                    Snapshot;
		std::optional<std::chrono::steady_clock::time_point> LastStorage;
		bool                                                 Pending = false;
		bool                                                 Closing = false;
	};
}

#endif
