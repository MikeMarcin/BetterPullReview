#include "bpr/application/device_binding_store.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <optional>
#include <utility>

namespace bpr::application {
	auto valid(const device_binding_lifetime& aLifetime) noexcept -> bool
	{
		return aLifetime.IssuedAt > 0 && aLifetime.AccessExpiresAt > aLifetime.IssuedAt
			&& aLifetime.RefreshExpiresAt > aLifetime.IssuedAt
			&& aLifetime.AccessExpiresAt - aLifetime.IssuedAt <= std::numeric_limits<std::uint32_t>::max()
			&& aLifetime.RefreshExpiresAt - aLifetime.IssuedAt <= std::numeric_limits<std::uint32_t>::max();
	}
	auto valid(const device_binding_record& aRecord) noexcept -> bool
	{
		return valid(aRecord.Authority) && valid(aRecord.Lifetime);
	}
	auto remaining_access_lifetime(const device_binding_lifetime& aLifetime, const std::int64_t aNow) noexcept -> std::uint32_t
	{
		if (!valid(aLifetime) || aNow < aLifetime.IssuedAt || aNow >= aLifetime.AccessExpiresAt || aNow >= aLifetime.RefreshExpiresAt) {
			return 0U;
		}
		return static_cast<std::uint32_t>(std::min(aLifetime.AccessExpiresAt, aLifetime.RefreshExpiresAt) - aNow);
	}
	auto remaining_refresh_lifetime(const device_binding_lifetime& aLifetime, const std::int64_t aNow) noexcept -> std::uint32_t
	{
		if (!valid(aLifetime) || aNow < aLifetime.IssuedAt || aNow >= aLifetime.RefreshExpiresAt) {
			return 0U;
		}
		return static_cast<std::uint32_t>(aLifetime.RefreshExpiresAt - aNow);
	}

	struct device_binding_persistence::implementation : std::enable_shared_from_this<implementation>
	{
		struct load_request
		{
			std::function<void(device_binding_load_result)> Completion;
		};
		struct write_request
		{
			device_binding_record                      Record;
			std::function<void(device_binding_result)> Completion;
		};
		struct erase_request
		{
			github_credential_authority_key            Authority;
			std::function<void(device_binding_result)> Completion;
		};
		using request = std::variant<load_request, write_request, erase_request>;
		using result  = std::variant<device_binding_load_result, device_binding_result>;
		device_binding_store_port& Store;
		effect_executor&           Effects;
		decision_executor&         Decisions;
		struct job
		{
			request          Request;
			std::stop_source Stop;
		};
		std::deque<job> Requests;
		std::size_t     PendingRefusals   = 0U;
		std::size_t     ActiveCompletions = 0U;
		bool            Closing           = false;

		implementation(device_binding_store_port& aStore, effect_executor& aEffects, decision_executor& aDecisions)
			: Store{ aStore }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
		{ }
		static void complete(request aEnvelope, result aResult) noexcept
		{
			try {
				std::visit([&aResult](auto& aRequest) {
					using request_type = std::decay_t<decltype(aRequest)>;
					if constexpr (std::is_same_v<request_type, load_request>) {
						aRequest.Completion(std::move(std::get<device_binding_load_result>(aResult)));
					} else {
						aRequest.Completion(std::move(std::get<device_binding_result>(aResult)));
					}
				},
						   aEnvelope);
			} catch (...) {
				// Completion observers cannot strand the ordered effect lane.
			}
		}
		static auto failure_for(const request& aRequest, device_binding_failure aFailure) -> result
		{
			if (std::holds_alternative<load_request>(aRequest)) {
				return device_binding_load_result{ std::unexpected(aFailure) };
			}
			return device_binding_result{ std::unexpected(aFailure) };
		}
		void enqueue(request aRequest)
		{
			if (Requests.size() >= maximum_device_bindings || (Closing && !std::holds_alternative<erase_request>(aRequest))) {
				auto failure = failure_for(aRequest, Closing ? device_binding_failure::cancelled : device_binding_failure::capacity);
				++PendingRefusals;
				try {
					Decisions.post([self = shared_from_this(), request = std::move(aRequest), outcome = std::move(failure)]() mutable {
						complete(std::move(request), std::move(outcome));
						--self->PendingRefusals;
					});
				} catch (...) {
					--PendingRefusals;
					throw;
				}
				return;
			}
			Requests.push_back(job{ std::move(aRequest), { } });
			if (Requests.size() == 1U) {
				try {
					submit();
				} catch (...) {
					// [[BPR-0884]]. Neither the effect nor its terminal was posted. The
					// request is withdrawn and the caller sees the refusal, the same
					// boundary an effect executor gives by throwing before it retains.
					Requests.pop_back();
					throw;
				}
			}
		}
		auto perform(const job& aJob) -> result
		{
			return std::visit([this, &aJob](const auto& aRequest) -> result {
				using request_type = std::decay_t<decltype(aRequest)>;
				if constexpr (std::is_same_v<request_type, load_request>) {
					return Store.load(aJob.Stop.get_token());
				} else if constexpr (std::is_same_v<request_type, write_request>) {
					return Store.store(aRequest.Record, aJob.Stop.get_token());
				} else {
					return Store.erase(aRequest.Authority, { });
				}
			},
							  aJob.Request);
		}
		void cancel_pending()
		{
			Closing = true;
			std::array<std::optional<std::stop_source>, maximum_device_bindings> stops;
			std::size_t                                                          count = 0U;
			for (const auto& job : Requests) {
				if (!std::holds_alternative<erase_request>(job.Request)) {
					stops[count++].emplace(job.Stop);
				}
			}
			// Stop callbacks may reenter; no queue iterator survives this boundary.
			for (std::size_t index = 0U; index < count; ++index) {
				stops[index]->request_stop();
			}
		}

		void finish(result aResult)
		{
			++ActiveCompletions;
			for (;;) {
				auto request = std::move(Requests.front().Request);
				Requests.pop_front();
				// Start queued effects before invoking a callback that may enqueue again.
				auto started = true;
				if (!Requests.empty()) {
					try {
						submit();
					} catch (...) {
						started = false;
					}
				}
				complete(std::move(request), std::move(aResult));
				if (started) {
					break;
				}
				// [[BPR-0884]]. Neither the successor's effect nor its terminal was
				// posted. This is already its lane, so its terminal is delivered here,
				// after its predecessor's, and the job after it is started in turn.
				aResult = failure_for(Requests.front().Request, device_binding_failure::unavailable);
			}
			--ActiveCompletions;
		}
		// Posts the front job's effect or, failing that, its `unavailable` terminal.
		// Throws only when neither was posted, and then nothing was retained.
		void submit()
		{
			auto self = shared_from_this();
			try {
				Effects.post([self, work = Requests.front()] {
					auto result = failure_for(work.Request, device_binding_failure::unavailable);
					try {
						result = self->perform(work);
					} catch (...) { }
					self->Decisions.post([self, outcome = std::move(result)]() mutable { self->finish(std::move(outcome)); });
				});
			} catch (...) {
				auto result = failure_for(Requests.front().Request, device_binding_failure::unavailable);
				Decisions.post([self, outcome = std::move(result)]() mutable { self->finish(std::move(outcome)); });
			}
		}
	};
	device_binding_persistence::device_binding_persistence(device_binding_store_port& aStore, effect_executor& aEffects, decision_executor& aDecisions)
		: Implementation{ std::make_shared<implementation>(aStore, aEffects, aDecisions) }
	{ }
	device_binding_persistence::~device_binding_persistence() = default;
	void device_binding_persistence::load(std::function<void(device_binding_load_result)> aCompletion) { Implementation->enqueue(implementation::load_request{ std::move(aCompletion) }); }
	void device_binding_persistence::store(device_binding_record aRecord, std::function<void(device_binding_result)> aCompletion) { Implementation->enqueue(implementation::write_request{ std::move(aRecord), std::move(aCompletion) }); }
	void device_binding_persistence::erase(github_credential_authority_key aAuthority, std::function<void(device_binding_result)> aCompletion) { Implementation->enqueue(implementation::erase_request{ std::move(aAuthority), std::move(aCompletion) }); }
	void device_binding_persistence::cancel_pending() { Implementation->cancel_pending(); }
	auto device_binding_persistence::is_quiescent() const noexcept -> bool { return Implementation->Requests.empty() && Implementation->PendingRefusals == 0U && Implementation->ActiveCompletions == 0U; }
}
