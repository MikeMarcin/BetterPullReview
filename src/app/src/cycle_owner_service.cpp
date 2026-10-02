#include "cycle_owner_service.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/sha256.hpp"
#include <algorithm>
namespace bpr::app {
	namespace {
		auto encoded(application::cycle_owner_response aResponse) -> application::tour_channel_bytes
		{
			return adapters::sqlite::encode_tour_response({ .Cycle = std::move(aResponse) });
		}
		auto refusal(const application::cycle_owner_request& aRequest, std::string aCode, std::string aMessage, std::string aNext = { }) -> application::tour_channel_bytes
		{
			return encoded({ .Operation = aRequest.Operation, .Step = aRequest.Step, .Ok = false, .Code = std::move(aCode), .Message = std::move(aMessage), .Next = std::move(aNext) });
		}
	}

	cycle_owner_service::cycle_owner_service(std::filesystem::path aRoot, const core::bpr_profile_id aProfile)
		: Root{ std::move(aRoot) }
		, Profile{ aProfile }
	{
	}

	auto cycle_owner_service::serve(const std::span<const std::uint8_t> aPayload, const application::cycle_owner_request& aRequest, const draft_guard& aGuard, const std::function<bool()>& aCancelled) -> answer
	{
		const auto digest = core::sha256(aPayload);
		{
			const auto lock  = std::scoped_lock{ Mutex };
			const auto found = std::ranges::find_if(Answers, [&](const remembered& aAnswer) { return aAnswer.Operation == aRequest.Operation && aAnswer.Step == aRequest.Step; });
			if (found != Answers.end()) {
				if (found->Digest == digest) {
					// The same request again: the same answer, and nothing run twice.
					return { found->Bytes, std::nullopt };
				}
				return { refusal(aRequest, "conflict", "That operation ID was already used for another request; nothing was applied. Use a new --operation-id."), std::nullopt };
			}
		}
		auto result = cycle_result<cycle_output>{ };
		auto run    = cycle_invocation_of(aRequest);
		try {
			const auto seed = cycle_operation_seed(run.Operation, run.Step);
			const auto host = cycle_host{
				.Open = [&]() -> cycle_result<std::unique_ptr<cycle_session>> { return std::make_unique<cycle_session>(Root, Profile, seed, aCancelled); },
				.Draft = aGuard,
			};
			result = execute_cycle_command(run, host);
		} catch (...) {
			result = std::unexpected(cycle_failure{ "operation_failed", "The owner could not finish the command; no completion is claimed. Read the cycle before repeating it.", { }, { } });
		}
		auto bytes = encoded(cycle_response_of(run, result));
		if (bytes.empty()) {
			// Over the channel's bound: said plainly, never cut short.
			bytes = refusal(aRequest, "over_limit", "The answer is larger than the owner's channel carries (" + application::grouped_count(application::maximum_tour_channel_bytes) + " bytes). Page it, or export to a file. Anything the command wrote stands.");
		}
		// A refusal that says "not now" is not remembered: repeating the request may succeed.
		constexpr auto passing   = std::array<std::string_view, 5>{ "draft_busy", "cancelled", "storage_unavailable", "profile_unavailable", "operation_failed" };
		const auto     transient = !result && std::ranges::find(passing, std::string_view{ result.error().Code }) != passing.end();
		if (!transient) {
			const auto lock = std::scoped_lock{ Mutex };
			Answers.push_back({ run.Operation, run.Step, digest, bytes });
			if (Answers.size() > remembered_answers) {
				Answers.pop_front();
			}
		}
		return { std::move(bytes), result ? result->Changed : std::nullopt };
	}
}
