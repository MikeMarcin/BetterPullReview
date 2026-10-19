#ifndef BPR_APPLICATION_INSIGHTS_HOST_HPP
#define BPR_APPLICATION_INSIGHTS_HOST_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/insights_observer.hpp"
#include "bpr/application/insights_projection.hpp"
#include "bpr/application/port.hpp"

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
		void attach(std::shared_ptr<insights_storage_port> aStorage);
		void record(core::insights_event aEvent) noexcept override;
		void select(core::insights_scope aScope) noexcept override;
		void record_review(core::insights_review_event aEvent) noexcept override;

	private:
		void                                     schedule();
		void                                     drain();
		effect_executor&                         Effects;
		std::mutex                               Mutex;
		std::shared_ptr<insights_storage_port>   Storage;
		std::vector<core::insights_event>        Events;
		std::vector<core::insights_review_event> ReviewEvents;
		std::optional<core::insights_scope>      Selected;
		bool                                     Pending = false;
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
