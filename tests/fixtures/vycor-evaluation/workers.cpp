#include <thread>
#include <utility>

namespace probe {
	void worker_body() { }

	std::thread spawn_worker()
	{
		return std::thread(worker_body);
	}

	std::thread* thread_reference(std::thread& aThread)
	{
		return &aThread;
	}

	std::thread empty_handle()
	{
		return std::thread();
	}

	std::thread move_handle(std::thread&& aThread)
	{
		return std::thread(std::move(aThread));
	}
}
