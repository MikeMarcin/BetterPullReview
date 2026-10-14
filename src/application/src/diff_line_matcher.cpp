#include "bpr/application/diff_line_matcher.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto request_steps(const core::diff_primary_request& aRequest) noexcept -> std::uint64_t
		{
			return 1U + static_cast<std::uint64_t>(aRequest.LeftClasses.size()) + static_cast<std::uint64_t>(aRequest.RightClasses.size());
		}

		/// One matcher call and what it leaves behind, shared by the waiting
		/// caller and the matcher's own thread; the thread may outlive the
		/// caller.
		struct detached_match
		{
			std::shared_ptr<diff_line_matcher_port>       Matcher;
			std::optional<core::decoded_text_document>    Left;
			std::optional<core::decoded_text_document>    Right;
			core::diff_options                            Options;
			std::mutex                                    Mutex;
			std::condition_variable                       Done;
			bool                                          Finished = false;
			std::optional<std::vector<core::diff_match>>  Result;
		};

		/// How long the caller waits between two polls of the computation's
		/// stop: a stop returns within this, well inside the 10 ms budget.
		constexpr auto stop_poll_interval = std::chrono::milliseconds{ 1 };
	} // namespace

	diff_line_matching::diff_line_matching(diff_line_matcher_port& aMatcher) noexcept
		: Matcher{ &aMatcher }
	{
	}

	auto diff_line_matching::parts() const noexcept -> core::diff_component_parts
	{
		return { .Primary = true };
	}

	auto diff_line_matching::primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
		-> std::optional<std::vector<core::diff_match>>
	{
		aMeter.tick(1U);
		auto result = Matcher->match({ .Left = aRequest.Left, .Right = aRequest.Right, .Options = aRequest.Options });
		aMeter.tick(request_steps(aRequest));
		return result;
	}

	abandonable_diff_line_matching::abandonable_diff_line_matching(std::shared_ptr<diff_line_matcher_port> aMatcher) noexcept
		: Matcher{ std::move(aMatcher) }
	{
	}

	auto abandonable_diff_line_matching::parts() const noexcept -> core::diff_component_parts
	{
		return { .Primary = Matcher != nullptr };
	}

	auto abandonable_diff_line_matching::primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
		-> std::optional<std::vector<core::diff_match>>
	{
		aMeter.poll();
		auto call     = std::make_shared<detached_match>();
		call->Matcher = Matcher;
		call->Options = aRequest.Options;
		if (aRequest.Left != nullptr) {
			call->Left = *aRequest.Left;
		}
		if (aRequest.Right != nullptr) {
			call->Right = *aRequest.Right;
		}
		aMeter.tick(request_steps(aRequest));
		try {
			std::thread{ [call] {
				auto result = std::optional<std::vector<core::diff_match>>{ };
				try {
					result = call->Matcher->match({
						.Left    = call->Left ? &*call->Left : nullptr,
						.Right   = call->Right ? &*call->Right : nullptr,
						.Options = call->Options,
					});
				} catch (...) {
					result.reset();
				}
				{
					const auto lock = std::scoped_lock{ call->Mutex };
					call->Result    = std::move(result);
					call->Finished  = true;
				}
				call->Done.notify_all();
			} }.detach();
		} catch (const std::system_error&) {
			// No thread to run it on: the comparison is refused, not computed
			// where a stop could not reach it.
			return std::nullopt;
		}
		for (;;) {
			{
				auto lock = std::unique_lock{ call->Mutex };
				if (call->Done.wait_for(lock, stop_poll_interval, [&call] { return call->Finished; })) {
					auto result = std::move(call->Result);
					lock.unlock();
					aMeter.tick(request_steps(aRequest));
					return result;
				}
			}
			// Does not return when the computation was stopped: the matcher's
			// thread is abandoned with its own copies.
			aMeter.poll();
		}
	}
} // namespace bpr::application
