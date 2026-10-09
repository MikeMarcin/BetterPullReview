#include "bpr/application/home_open_host.hpp"

#include <utility>

namespace bpr::application {
	home_open_host::home_open_host(home_open_wait aWait, publication aPublication)
		: Wait{ std::move(aWait) }
		, Publication{ std::move(aPublication) }
		, Lifetime{ std::make_shared<int>(0) }
	{
	}

	home_open_host::~home_open_host() = default;

	void home_open_host::observe(const core::github_transition_output& aTransition, const core::github_state& aLane)
	{
		const auto previous = State;
		auto       next     = core::reconcile_home_open(core::apply_home_open(State, aTransition), aLane);
		// A read that started here is the one the wait is armed for. The core
		// names it; the clock is this host's.
		const auto started = next.has_value() && next->Phase == core::home_open_phase::opening
			&& (!previous.has_value() || previous->Request != next->Request);
		adopt(std::move(next));
		if (started && Wait) {
			++WaitsArmed;
			Wait(core::home_open_wait_milliseconds,
				 [this, alive = std::weak_ptr<int>{ Lifetime }, request = State->Request] {
					 if (!alive.expired()) {
						 elapsed(request);
					 }
				 });
		}
	}

	void home_open_host::reconcile(const core::github_state& aLane)
	{
		adopt(core::reconcile_home_open(State, aLane));
	}

	auto home_open_host::admit_cancel(const core::request_id aRequest, const core::github_state& aLane) const -> bool
	{
		return core::admit_home_open_cancel(State, aLane, aRequest);
	}

	auto home_open_host::admit_retry(const core::request_id aRequest) const
		-> std::optional<core::open_pull_request_intent>
	{
		if (!core::home_open_retryable(State) || State->Request != aRequest) {
			return std::nullopt;
		}
		return core::open_pull_request_intent{ .Repository = State->Repository, .Number = State->Number };
	}

	auto home_open_host::state() const noexcept -> const std::optional<core::home_open_state>&
	{
		return State;
	}

	auto home_open_host::projection() const -> home_open_projection
	{
		return home_open_projection{
			.State       = State,
			.Cancellable = core::home_open_cancellable(State),
			.Retryable   = core::home_open_retryable(State),
		};
	}

	auto home_open_host::waits_armed() const noexcept -> std::uint64_t
	{
		return WaitsArmed;
	}

	void home_open_host::adopt(std::optional<core::home_open_state> aNext)
	{
		if (aNext == State) {
			return;
		}
		State = std::move(aNext);
		if (Publication) {
			Publication(projection());
		}
	}

	void home_open_host::elapsed(const core::request_id aRequest)
	{
		adopt(core::apply_home_open(State, core::home_open_waited{ .Request = aRequest }));
	}
} // namespace bpr::application
