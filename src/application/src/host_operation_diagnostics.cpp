#include "bpr/application/host_operation_diagnostics.hpp"
#include "bpr/application/operation_diagnostics.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace bpr::application {
	aggregated_operation_log::aggregated_operation_log(std::shared_ptr<diagnostic_log_port> aLog, const clock::duration aWindow)
		: Log{ std::move(aLog) }
		, Window{ aWindow }
	{
	}

	aggregated_operation_log::~aggregated_operation_log() { flush(); }

	void aggregated_operation_log::record(const core::operation_diagnosis& aDiagnosis) noexcept
	{
		record(aDiagnosis, clock::now());
	}

	void aggregated_operation_log::record(const core::operation_diagnosis& aDiagnosis, const clock::time_point aNow) noexcept
	{
		if (!Log) {
			return;
		}
		auto line = std::optional<core::operation_diagnosis>{ };
		try {
			const auto lock  = std::scoped_lock{ Mutex };
			auto&      entry = Groups[key{ aDiagnosis.Action, aDiagnosis.Failure, aDiagnosis.Store }];
			entry.Longest    = std::max(entry.Longest, aDiagnosis.DurationMilliseconds);
			entry.Schema     = aDiagnosis.Schema;
			if (entry.Held < std::numeric_limits<std::uint32_t>::max()) {
				++entry.Held;
			}
			if (!entry.Recorded || aNow - *entry.Recorded >= Window) {
				line                       = aDiagnosis;
				line->Count                = std::exchange(entry.Held, 0U);
				line->DurationMilliseconds = std::exchange(entry.Longest, 0U);
				entry.Recorded             = aNow;
			}
		} catch (...) {
			return;
		}
		// Outside the lock: the log may take its own.
		if (line) {
			record_operation(Log.get(), *line);
		}
	}

	void aggregated_operation_log::flush() noexcept { flush(clock::now()); }

	void aggregated_operation_log::flush(const clock::time_point aNow) noexcept
	{
		if (!Log) {
			return;
		}
		auto lines = std::vector<core::operation_diagnosis>{ };
		try {
			const auto lock = std::scoped_lock{ Mutex };
			for (auto& [identity, entry] : Groups) {
				if (entry.Held == 0U) {
					continue;
				}
				const auto& [action, failure, store] = identity;
				lines.push_back({ .Action = action, .Failure = failure, .DurationMilliseconds = std::exchange(entry.Longest, 0U), .Count = std::exchange(entry.Held, 0U), .Store = store, .Schema = entry.Schema });
				entry.Recorded = aNow;
			}
		} catch (...) {
			return;
		}
		for (const auto& line : lines) {
			record_operation(Log.get(), line);
		}
	}

	auto operation_failure_of(const core::source_search_status aStatus) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		using status  = core::source_search_status;
		switch (aStatus) {
		case status::complete:
		case status::partial:
		case status::result_limit:        return failure::none;
		case status::cancelled:           return failure::cancelled;
		case status::unavailable:         return failure::unavailable;
		case status::resource_limit:      return failure::resource_limit;
		case status::identity_exhausted:  return failure::identity_exhausted;
		case status::idle:
		case status::searching:
		case status::failure:
		case status::invalid_input:
		case status::invalid_pattern:
		case status::unsupported_pattern: return failure::search_failed;
		}
		return failure::search_failed;
	}

	auto operation_failure_of(const core::markdown_image_state aState) noexcept -> core::operation_failure
	{
		using failure = core::operation_failure;
		using state   = core::markdown_image_state;
		switch (aState) {
		case state::loaded:      return failure::none;
		case state::too_large:   return failure::too_large;
		case state::blocked:     return failure::blocked;
		case state::unsupported: return failure::unsupported;
		case state::over_limit:  return failure::over_limit;
		case state::loading:
		case state::failed:      return failure::fetch_failed;
		}
		return failure::fetch_failed;
	}

	diagnosed_image_fetch::diagnosed_image_fetch(markdown_image_fetch_port& aFetch, std::shared_ptr<aggregated_operation_log> aLog)
		: Fetch{ aFetch }
		, Log{ std::move(aLog) }
	{
	}

	void diagnosed_image_fetch::fetch(markdown_image_fetch_request aRequest, std::stop_token aStop, std::function<void(markdown_image_fetch_result)> aCompletion)
	{
		if (!Log) {
			Fetch.fetch(std::move(aRequest), std::move(aStop), std::move(aCompletion));
			return;
		}
		const auto action = aRequest.Policy.AvatarOrigins ? core::operation_action::avatar_fetch : core::operation_action::image_fetch;
		auto       ended  = [log = Log, action, stop = aStop, watch = operation_stopwatch{ }, completion = std::move(aCompletion)](markdown_image_fetch_result aResult) {
			const auto failure = aResult ? core::operation_failure::none : stop.stop_requested() ? core::operation_failure::cancelled
																								 : operation_failure_of(aResult.error());
			log->record({ .Action = action, .Failure = failure == core::operation_failure::none && !aResult ? core::operation_failure::fetch_failed : failure, .DurationMilliseconds = watch.milliseconds() });
			if (completion) {
				completion(std::move(aResult));
			}
		};
		Fetch.fetch(std::move(aRequest), std::move(aStop), std::move(ended));
	}
} // namespace bpr::application
