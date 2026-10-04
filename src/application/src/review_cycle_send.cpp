#include "bpr/application/review_cycle_send.hpp"

#include <cstdint>
#include <span>
#include <utility>

namespace bpr::application {
	auto record_send(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_send_record& aRecord, std::stop_token aStop)
		-> cycle_store_result<cycle_send_recorded>
	{
		const auto observations = core::send_observations(aRecord.Round, aRecord.Output, aRecord.Protocol, aRecord.Result);
		if (aRecord.Operations.size() < observations.size() + 1U) {
			return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_target });
		}
		auto recorded = cycle_send_recorded{ };
		for (std::size_t index = 0; index < observations.size(); ++index) {
			auto receipt = aStore.record_delivery(aRecord.Operations[index], aSubject, observations[index], aStop);
			if (!receipt) {
				return std::unexpected(receipt.error());
			}
			recorded.Deliveries.push_back(*receipt);
		}
		if (aRecord.Result.End != core::cycle_send_end::completed) {
			return recorded;
		}
		const auto document = core::reply_response(aRecord.Result.Reply);
		if (!document) {
			return recorded;
		}
		const auto bytes  = std::span{ reinterpret_cast<const std::uint8_t*>(document->data()), document->size() };
		auto       answer = import_cycle_response(aStore, aSubject, aRecord.Round.Id.Id, bytes, aRecord.Operations[observations.size()], std::move(aStop), core::cycle_response_channel::agent_adapter);
		if (!answer) {
			return std::unexpected(answer.error());
		}
		recorded.Response = std::move(*answer);
		return recorded;
	}
}
