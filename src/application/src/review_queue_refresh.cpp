#include "bpr/application/review_queue_refresh.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr std::uint32_t InitialBackoffSeconds = 15U;
	} // namespace

	auto name(const queue_refresh_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case queue_refresh_status::changed:
			return "changed";
		case queue_refresh_status::unchanged:
			return "unchanged";
		case queue_refresh_status::offline:
			return "offline";
		case queue_refresh_status::rate_limited:
			return "rate limited";
		case queue_refresh_status::secondary_rate_limited:
			return "secondary rate limited";
		case queue_refresh_status::cancelled:
			return "cancelled";
		case queue_refresh_status::forbidden:
			return "forbidden";
		case queue_refresh_status::gone:
			return "gone";
		}
		return "unchanged";
	}

	auto name(const refresh_disposition aDisposition) noexcept -> std::string_view
	{
		switch (aDisposition) {
		case refresh_disposition::applied:
			return "applied";
		case refresh_disposition::unchanged:
			return "unchanged";
		case refresh_disposition::stale_session:
			return "stale session";
		case refresh_disposition::deferred:
			return "deferred";
		case refresh_disposition::withdrawn:
			return "withdrawn";
		}
		return "unchanged";
	}

	queue_refresh_scheduler::queue_refresh_scheduler(
		queue_session_id aSession) noexcept
		: Session{ std::move(aSession) }
	{
	}

	auto queue_refresh_scheduler::session() const noexcept
		-> const queue_session_id&
	{
		return Session;
	}

	void queue_refresh_scheduler::adopt(queue_session_id aSession)
	{
		if (aSession == Session) {
			return;
		}
		Session = std::move(aSession);
		// Validators belong to the account that obtained them. Carrying them
		// across would send one account's entity tag under another's authority,
		// and a "not modified" answer would then be about a pull request this
		// account may not even be able to read.
		Known.clear();
		Backoff = 0U;
		Online  = true;
	}

	auto queue_refresh_scheduler::find(const core::pull_request_ref& aRef)
		-> known_pull_request*
	{
		const auto found = std::ranges::find(
			Known, aRef, &known_pull_request::Ref);
		return found == Known.end() ? nullptr : &*found;
	}

	auto queue_refresh_scheduler::request_for(const core::pull_request_ref& aRef)
		-> queue_refresh_request
	{
		++Statistics.Requested;
		auto request = queue_refresh_request{
			.Session   = Session,
			.Ref       = aRef,
			.Validator = std::nullopt,
		};
		if (const auto* const known = find(aRef);
			known != nullptr && !known->Validator.empty()) {
			request.Validator = known->Validator;
			++Statistics.ConditionalAsked;
		}
		return request;
	}

	auto queue_refresh_scheduler::apply(
		const queue_refresh_completion& aCompletion) -> refresh_disposition
	{
		// Identity first, before anything else is read. A completion from a
		// session that has been replaced must not be able to change the
		// validator, the backoff, or the online state — those belong to the
		// session that is current, and a stale answer knows nothing about it.
		if (aCompletion.Session != Session) {
			++Statistics.RefusedAsStale;
			return refresh_disposition::stale_session;
		}

		switch (aCompletion.Status) {
		case queue_refresh_status::offline:
			Online = false;
			++Statistics.Deferred;
			return refresh_disposition::deferred;

		case queue_refresh_status::rate_limited:
		case queue_refresh_status::secondary_rate_limited:
			Online = true;
			// The provider's own figure is preferred when it gives one: it
			// knows when the allowance resets and this does not. Doubling is
			// what happens when it says nothing.
			if (aCompletion.RetryAfterSeconds) {
				Backoff = std::min(
					*aCompletion.RetryAfterSeconds, MaximumBackoffSeconds);
			} else {
				Backoff = Backoff == 0U
					? InitialBackoffSeconds
					: std::min(Backoff * 2U, MaximumBackoffSeconds);
			}
			++Statistics.Deferred;
			return refresh_disposition::deferred;

		case queue_refresh_status::cancelled:
			// Cancelling publishes nothing. It is not a failure of the host and
			// must not push the scheduler into backoff, or a reviewer switching
			// quickly between reviews would slow their own queue down.
			return refresh_disposition::unchanged;

		case queue_refresh_status::forbidden:
		case queue_refresh_status::gone:
			Online  = true;
			Backoff = 0U;
			if (auto* const known = find(aCompletion.Ref); known != nullptr) {
				Known.erase(Known.begin() + (known - Known.data()));
			}
			return refresh_disposition::withdrawn;

		case queue_refresh_status::unchanged:
			Online  = true;
			Backoff = 0U;
			return refresh_disposition::unchanged;

		case queue_refresh_status::changed:
			break;
		}

		Online  = true;
		Backoff = 0U;
		if (auto* const known = find(aCompletion.Ref); known != nullptr) {
			known->Validator = aCompletion.Validator.value_or(std::string{ });
		} else {
			Known.push_back(known_pull_request{
				.Ref       = aCompletion.Ref,
				.Validator = aCompletion.Validator.value_or(std::string{ }),
			});
		}
		++Statistics.Applied;
		return refresh_disposition::applied;
	}

	auto queue_refresh_scheduler::backoff_seconds() const noexcept
		-> std::uint32_t
	{
		return Backoff;
	}

	auto queue_refresh_scheduler::online() const noexcept -> bool
	{
		return Online;
	}

	auto queue_refresh_scheduler::statistics() const noexcept
		-> queue_refresh_statistics
	{
		return Statistics;
	}

	auto queue_refresh_scheduler::validator_for(
		const core::pull_request_ref& aRef) const -> std::optional<std::string>
	{
		const auto found = std::ranges::find(
			Known, aRef, &known_pull_request::Ref);
		if (found == Known.end() || found->Validator.empty()) {
			return std::nullopt;
		}
		return found->Validator;
	}
} // namespace bpr::application
