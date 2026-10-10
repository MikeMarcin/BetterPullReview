#include "bpr/application/reply_thread_observation.hpp"

#include <string>

namespace bpr::application {
	auto observe_reply_threads(const github_pull_request_snapshot& aSnapshot) -> core::reply_thread_observation
	{
		auto observation     = core::reply_thread_observation{ };
		observation.Complete = aSnapshot.ReviewThreadsAvailability == github_collection_availability::available;
		if (!observation.Complete) {
			return observation;
		}
		observation.Threads.reserve(aSnapshot.ReviewThreads.size());
		for (const auto& thread : aSnapshot.ReviewThreads) {
			observation.Threads.push_back({ .Thread = std::string{ thread.Thread.value() }, .Resolved = thread.Resolved });
		}
		return observation;
	}
}
