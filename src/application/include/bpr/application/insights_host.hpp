#ifndef BPR_APPLICATION_INSIGHTS_HOST_HPP
#define BPR_APPLICATION_INSIGHTS_HOST_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/insights_observer.hpp"
#include "bpr/application/insights_projection.hpp"
#include "bpr/application/port.hpp"

#include <atomic>
#include <mutex>

#include <chrono>
#include <memory>

namespace bpr::application {
	/// \brief Sole local measurement capability; implementations never launch a process or read remotely.
	class insights_storage_port
	{
	public:
		virtual ~insights_storage_port()                          = default;
		[[nodiscard]] virtual auto measure() -> insights_snapshot = 0;
		virtual void               record(const std::vector<core::insights_event>&, std::optional<core::insights_scope>) { }
		virtual void               record_energy(const std::vector<core::insights_energy_sample>&) { }
		[[nodiscard]] virtual auto energy(std::int64_t) -> core::insights_energy_snapshot { return { }; }
		virtual void               record_reviews(const std::vector<core::insights_review_event>&) { }
		[[nodiscard]] virtual auto reviews(std::int64_t) -> core::insights_reviews_snapshot { return { }; }
		[[nodiscard]] virtual auto github(std::int64_t) -> core::insights_github_snapshot { return { }; }
	};

	/// \brief Tallies failed outcomes independently of diagnostic recording preferences.
	[[nodiscard]] auto observe_insights_problems(std::shared_ptr<diagnostic_log_port> aLog, std::shared_ptr<insights_observer> aObserver) -> std::shared_ptr<diagnostic_log_port>;

	/// \brief Bounded nonblocking intake; one effect task drains observations to private storage.
	class insights_recorder final : public insights_observer, public std::enable_shared_from_this<insights_recorder>
	{
	public:
		explicit insights_recorder(effect_executor& aEffects);
		void               attach(std::shared_ptr<insights_storage_port> aStorage);
		void               record(core::insights_event aEvent) noexcept override;
		void               select(core::insights_scope aScope) noexcept override;
		void               record_review(core::insights_review_event aEvent) noexcept override;
		void               record_energy(core::insights_energy_sample aSample) noexcept override;
		[[nodiscard]] auto foreground() const noexcept -> bool override { return Foreground.load(); }
		void               set_foreground(bool aForeground) noexcept { Foreground.store(aForeground); }

	private:
		void                                      schedule();
		void                                      drain();
		effect_executor&                          Effects;
		std::mutex                                Mutex;
		std::shared_ptr<insights_storage_port>    Storage;
		std::vector<core::insights_event>         Events;
		std::vector<core::insights_review_event>  ReviewEvents;
		std::vector<core::insights_energy_sample> EnergySamples;
		std::atomic_bool                          Foreground = false;
		std::optional<core::insights_scope>       Selected;
		bool                                      Pending = false;
	};

	class process_metrics_port
	{
	public:
		virtual ~process_metrics_port()                                                     = default;
		[[nodiscard]] virtual auto read() -> std::optional<core::insights_process_counters> = 0;
	};
	/// \brief Sampling schedule and interval ownership; all observations execute on the Insights effect lane.
	class insights_energy_sampler final : public std::enable_shared_from_this<insights_energy_sampler>
	{
	public:
		insights_energy_sampler(std::shared_ptr<process_metrics_port> aMetrics, std::shared_ptr<insights_recorder> aRecorder, effect_executor& aEffects);
		void                      observe(bool aForeground);
		void                      shutdown() noexcept;
		[[nodiscard]] static auto interval_seconds(bool aForeground) noexcept -> int { return aForeground ? 30 : 60; }

	private:
		void                                           collect(bool aForeground);
		std::shared_ptr<process_metrics_port>          Metrics;
		std::shared_ptr<insights_recorder>             Recorder;
		effect_executor&                               Effects;
		std::optional<core::insights_process_counters> Previous;
		bool                                           WasForeground = false;
		std::atomic_bool                               Closing       = false;
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
