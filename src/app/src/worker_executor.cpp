#include "bpr/app/worker_executor.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace bpr::app {
	class worker_executor::implementation final
	{
	public:
		implementation()
			: Worker{ [this] { run(); } }
		{
		}

		void post(std::function<void()> aTask)
		{
			{
				const std::scoped_lock lock{ Mutex };
				if (!Accepting) {
					throw std::runtime_error{
						"Cannot post to a closed worker executor"
					};
				}

				Tasks.push_back(std::move(aTask));
			}

			WorkAvailable.notify_one();
		}

		void close_and_drain() noexcept
		{
			const std::scoped_lock closeLock{ CloseMutex };

			{
				const std::scoped_lock lock{ Mutex };
				Accepting = false;
			}
			WorkAvailable.notify_one();

			if (Worker.joinable()) {
				Worker.join();
			}
		}

		[[nodiscard]] auto accepting() const noexcept -> bool
		{
			const std::scoped_lock lock{ Mutex };
			return Accepting;
		}

		[[nodiscard]] auto idle() const noexcept -> bool
		{
			const std::scoped_lock lock{ Mutex };
			return Tasks.empty() && !Executing;
		}

	private:
		void run() noexcept
		{
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock lock{ Mutex };
					WorkAvailable.wait(
						lock,
						[this] { return !Accepting || !Tasks.empty(); });

					if (Tasks.empty()) {
						return;
					}

					task = std::move(Tasks.front());
					Tasks.pop_front();
					Executing = true;
				}

				try {
					task();
				} catch (...) {
					// A failed effect must not prevent later accepted work from
					// reaching its terminal path.
				}

				{
					const std::scoped_lock lock{ Mutex };
					Executing = false;
				}
			}
		}

		mutable std::mutex                Mutex;
		std::mutex                        CloseMutex;
		std::condition_variable           WorkAvailable;
		std::deque<std::function<void()>> Tasks;
		bool                              Accepting = true;
		bool                              Executing = false;
		std::thread                       Worker;
	};

	worker_executor::worker_executor()
		: Implementation{ std::make_unique<implementation>() }
	{
	}

	worker_executor::~worker_executor()
	{
		Implementation->close_and_drain();
	}

	void worker_executor::post(std::function<void()> aTask)
	{
		Implementation->post(std::move(aTask));
	}

	void worker_executor::close_and_drain() noexcept
	{
		Implementation->close_and_drain();
	}

	auto worker_executor::accepting() const noexcept -> bool
	{
		return Implementation->accepting();
	}

	auto worker_executor::idle() const noexcept -> bool
	{
		return Implementation->idle();
	}
}
