#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_WORKER_EXECUTOR_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_WORKER_EXECUTOR_HPP

#include "bpr/application/port.hpp"

#include <cstddef>
#include <memory>

namespace bpr::app {
	/// \brief Provides the worker executor contract.
	class worker_executor final : public application::effect_executor
	{
	public:
		worker_executor();
		~worker_executor() override;

		worker_executor(const worker_executor&)                    = delete;
		auto operator=(const worker_executor&) -> worker_executor& = delete;
		worker_executor(worker_executor&&)                         = delete;
		auto operator=(worker_executor&&) -> worker_executor&      = delete;

		void post(std::function<void()> aTask) override;

		// Stop accepting work, execute every successfully posted task, and join
		// the owned worker. This operation is idempotent and is intended to run
		// from a thread other than the owned worker.
		void close_and_drain() noexcept;

		[[nodiscard]] auto accepting() const noexcept -> bool;

		// Idle means that no task is queued or currently executing.
		[[nodiscard]] auto idle() const noexcept -> bool;
		[[nodiscard]] auto queued_tasks() const noexcept -> std::size_t;

	private:
		class implementation;
		std::unique_ptr<implementation> Implementation;
	};
}

#endif // BPR_SRC_APP_INCLUDE_BPR_APP_WORKER_EXECUTOR_HPP
