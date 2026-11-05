#include "bpr/application/pull_request_activity_host.hpp"

#include "bpr/application/operation_diagnostics.hpp"

#include <utility>

namespace bpr::application {
	namespace {
		/// Poll identities apart from every other read's.
		constexpr std::uint64_t first_activity_request = std::uint64_t{ 0xA1U } << 52U;

		[[nodiscard]] auto account_key(const personal_discovery_session& aSession) -> std::string
		{
			return aSession.Queue.Host + "/" + std::to_string(aSession.Queue.ViewerAccount.value());
		}
	} // namespace

	struct pull_request_activity_host::implementation : std::enable_shared_from_this<implementation>
	{
		pull_request_activity_port&               Provider;
		pull_request_activity_store_port&         Store;
		effect_executor&                          Effects;
		decision_executor&                        Decisions;
		pull_request_activity_timer_port&         Timer;
		pull_request_activity_sink_port&          Sink;
		pull_request_activity_clock               Clock;
		pull_request_activity_focus               Focus;
		std::function<void()>                     Observer;
		pull_request_activity_refresh             Refresh;
		std::shared_ptr<diagnostic_log_port>      Log;
		core::activity_memory                     Memory;
		core::activity_session                    Session;
		core::activity_schedule                   Schedule;
		std::optional<personal_discovery_session> Account;
		pull_request_activity_statistics          Statistics;
		std::optional<std::uint32_t>              Scheduled;
		std::uint64_t                             NextRequest = first_activity_request;
		/// The poll in flight, by its request identity.
		std::optional<std::uint64_t> InFlight;
		std::size_t                  Writes       = 0U;
		bool                         Writable     = true;
		bool                         WritePending = false;
		bool                         Closing      = false;

		implementation(pull_request_activity_port& aProvider, pull_request_activity_store_port& aStore, effect_executor& aEffects, decision_executor& aDecisions,
					   pull_request_activity_timer_port& aTimer, pull_request_activity_sink_port& aSink, pull_request_activity_clock aClock,
					   pull_request_activity_focus aFocus, const core::activity_memory_read& aLoaded, std::shared_ptr<diagnostic_log_port> aLog)
			: Provider{ aProvider }
			, Store{ aStore }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Timer{ aTimer }
			, Sink{ aSink }
			, Clock{ std::move(aClock) }
			, Focus{ std::move(aFocus) }
			, Log{ std::move(aLog) }
			, Memory{ aLoaded.Memory }
			, Writable{ aLoaded.Writable }
		{ }

		[[nodiscard]] auto now() const -> std::int64_t
		{
			return Clock ? Clock() : 0;
		}

		[[nodiscard]] auto may_poll() const noexcept -> bool
		{
			return !Closing && Memory.Enabled && Account.has_value() && !Schedule.Stopped
				&& Account->ProfileAuthority.Authentication == github_authentication_transport::github_cli;
		}

		void arm(const std::uint32_t aSeconds)
		{
			Scheduled = aSeconds;
			Timer.schedule(aSeconds);
		}

		void disarm()
		{
			Scheduled.reset();
			Timer.cancel();
		}

		/// One write at a time, each carrying the latest memory.
		void write()
		{
			if (Closing || !Writable) {
				return;
			}
			if (Writes != 0U) {
				WritePending = true;
				return;
			}
			post_write();
		}

		void post_write()
		{
			WritePending = false;
			++Writes;
			try {
				Effects.post([self = shared_from_this(), snapshot = Memory] {
					try {
						static_cast<void>(self->Store.store(snapshot));
					} catch (...) { }
					self->Decisions.post([self] {
						--self->Writes;
						if (self->WritePending && self->Writes == 0U && !self->Closing) {
							self->post_write();
						}
					});
				});
			} catch (...) {
				--Writes;
			}
		}

		void poll()
		{
			if (!may_poll() || InFlight) {
				return;
			}
			disarm();
			const auto identity = NextRequest++;
			InFlight            = identity;
			++Statistics.Polls;
			const auto& session = *Account;
			auto        request = pull_request_activity_request{
				.Session = session,
				.Context = core::github_session_request_context{
					.Request            = core::request_id{ identity },
					.ApplicationSession = session.Context.ApplicationSession,
					.Profile            = session.Context.Profile,
					.IdentityGeneration = session.Context.IdentityGeneration,
				},
				.IfModifiedSince = Memory.LastModified,
			};
			auto weak = weak_from_this();
			try {
				Provider.poll_activity(std::move(request), [weak, identity, decisions = &Decisions](pull_request_activity_answer aAnswer) {
					const auto self = weak.lock();
					if (!self) {
						return;
					}
					decisions->post([weak, identity, answer = std::move(aAnswer)]() mutable {
						if (const auto live = weak.lock()) {
							live->complete(identity, std::move(answer));
						}
					});
				});
			} catch (...) {
				InFlight.reset();
				complete_failure();
			}
		}

		void complete_failure()
		{
			++Statistics.Failed;
			const auto next = core::next_activity_poll(Schedule, { .Outcome = core::activity_poll_outcome::failed }, now());
			Schedule        = next.Schedule;
			if (next.Delay && may_poll()) {
				arm(*next.Delay);
			}
		}

		void complete(const std::uint64_t aIdentity, pull_request_activity_answer aAnswer)
		{
			if (InFlight != aIdentity) {
				++Statistics.Stale;
				return;
			}
			InFlight.reset();
			Statistics.Requests += aAnswer.Requests;
			if (Closing) {
				return;
			}
			if (!may_poll()) {
				// Signed out, another session or turned off while it ran.
				++Statistics.Stale;
				return;
			}
			const auto time = now();
			// [[BPR-1850]]. One line per poll: enums and counts, no repository.
			const auto requests = aAnswer.Requests;
			switch (aAnswer.Poll.Outcome) {
			case core::activity_poll_outcome::changed:      record_operation(Log.get(), { .Action = core::operation_action::activity_poll_changed, .Count = requests }); break;
			case core::activity_poll_outcome::unchanged:    record_operation(Log.get(), { .Action = core::operation_action::activity_poll_unchanged, .Count = requests }); break;
			case core::activity_poll_outcome::unsupported:  record_operation(Log.get(), { .Action = core::operation_action::activity_poll, .Failure = core::operation_failure::unsupported, .Count = requests }); break;
			case core::activity_poll_outcome::rate_limited: record_operation(Log.get(), { .Action = core::operation_action::activity_poll, .Failure = core::operation_failure::rate_limited, .Count = requests }); break;
			case core::activity_poll_outcome::failed:       record_operation(Log.get(), { .Action = core::operation_action::activity_poll, .Failure = core::operation_failure::unavailable, .Count = requests }); break;
			}
			switch (aAnswer.Poll.Outcome) {
			case core::activity_poll_outcome::changed:
			case core::activity_poll_outcome::unchanged: {
				aAnswer.Poll.Outcome == core::activity_poll_outcome::changed ? ++Statistics.Changed : ++Statistics.Unchanged;
				const auto changed = aAnswer.Poll.Outcome == core::activity_poll_outcome::changed;
				auto       context = core::activity_context{
					.Host    = Account->Queue.Host,
					.Login   = Account->Queue.Account,
					.Now     = time,
					.Focused = Focus ? Focus() : std::vector<core::pull_request_ref>{ },
				};
				auto decision = core::decide_activity(Memory, Session, changed ? aAnswer.Threads : std::vector<core::activity_thread>{ }, changed && aAnswer.Complete, context);
				if (changed && !aAnswer.LastModified.empty() && aAnswer.LastModified.size() <= core::activity_last_modified_limit) {
					decision.Memory.LastModified = aAnswer.LastModified;
				}
				Statistics.Suppressed += decision.Suppressed;
				auto active              = std::move(decision.Active);
				Session                  = std::move(decision.Session);
				const auto memoryChanged = decision.Memory != Memory;
				Memory                   = std::move(decision.Memory);
				auto handed              = std::uint32_t{ 0U };
				for (const auto& notification : decision.Notifications) {
					try {
						if (Sink.post(core::activity_notification_title(notification), core::activity_notification_body(notification))) {
							++Statistics.Posted;
							++handed;
						}
					} catch (...) { }
				}
				if (!decision.Notifications.empty()) {
					const auto decided = static_cast<std::uint32_t>(decision.Notifications.size());
					record_operation(Log.get(), { .Action = core::operation_action::activity_post, .Failure = handed == decided ? core::operation_failure::none : core::operation_failure::unavailable, .Count = decided });
				}
				if (decision.Suppressed != 0U) {
					record_operation(Log.get(), { .Action = core::operation_action::activity_suppress, .Count = decision.Suppressed });
				}
				if (memoryChanged) {
					write();
				}
				if (!active.empty() && Refresh) {
					// [[BPR-1867]]. Counts only: the log never names a repository.
					Statistics.Active += active.size();
					record_operation(Log.get(), { .Action = core::operation_action::activity_refresh, .Count = static_cast<std::uint32_t>(active.size()) });
					try {
						Refresh(active);
					} catch (...) { }
				}
				break;
			}
			case core::activity_poll_outcome::rate_limited:
				++Statistics.RateLimited;
				break;
			case core::activity_poll_outcome::unsupported:
				break;
			case core::activity_poll_outcome::failed:
				++Statistics.Failed;
				break;
			}
			const auto next = core::next_activity_poll(Schedule, aAnswer.Poll, time);
			Schedule        = next.Schedule;
			if (next.Delay) {
				arm(*next.Delay);
			} else {
				disarm();
			}
			if (Observer) {
				Observer();
			}
		}
	};

	pull_request_activity_host::pull_request_activity_host(pull_request_activity_port&          aProvider,
														   pull_request_activity_store_port&    aStore,
														   effect_executor&                     aEffects,
														   decision_executor&                   aDecisions,
														   pull_request_activity_timer_port&    aTimer,
														   pull_request_activity_sink_port&     aSink,
														   pull_request_activity_clock          aClock,
														   pull_request_activity_focus          aFocus,
														   core::activity_memory_read           aLoaded,
														   std::shared_ptr<diagnostic_log_port> aLog)
		: Impl{ std::make_shared<implementation>(aProvider, aStore, aEffects, aDecisions, aTimer, aSink, std::move(aClock), std::move(aFocus), aLoaded, std::move(aLog)) }
	{ }

	pull_request_activity_host::~pull_request_activity_host()
	{
		shutdown();
	}

	void pull_request_activity_host::adopt(std::optional<personal_discovery_session> aSession)
	{
		auto& impl = *Impl;
		if (impl.Closing || impl.Account == aSession) {
			return;
		}
		impl.Account = std::move(aSession);
		impl.InFlight.reset();
		impl.Session  = { };
		impl.Schedule = { };
		impl.disarm();
		if (!impl.Account) {
			return;
		}
		const auto memory = core::activity_memory_for(impl.Memory, account_key(*impl.Account));
		if (memory != impl.Memory) {
			impl.Memory = memory;
			impl.write();
		}
		impl.poll();
	}

	void pull_request_activity_host::set_enabled(const bool aEnabled)
	{
		auto& impl = *Impl;
		if (impl.Closing || impl.Memory.Enabled == aEnabled) {
			return;
		}
		impl.Memory = core::set_activity_enabled(impl.Memory, aEnabled);
		impl.write();
		impl.InFlight.reset();
		impl.Session  = { };
		impl.Schedule = { };
		impl.disarm();
		impl.poll();
	}

	auto pull_request_activity_host::enabled() const noexcept -> bool
	{
		return Impl->Memory.Enabled;
	}

	void pull_request_activity_host::tick()
	{
		Impl->Scheduled.reset();
		Impl->poll();
	}

	void pull_request_activity_host::set_observer(std::function<void()> aObserver)
	{
		Impl->Observer = std::move(aObserver);
	}

	void pull_request_activity_host::set_refresh(pull_request_activity_refresh aRefresh)
	{
		Impl->Refresh = std::move(aRefresh);
	}

	auto pull_request_activity_host::activate() -> core::activity_target
	{
		record_operation(Impl->Log.get(), { .Action = core::operation_action::activity_activate });
		auto activation = core::activate_activity(Impl->Session);
		Impl->Session   = std::move(activation.Session);
		return activation.Target;
	}

	void pull_request_activity_host::shutdown() noexcept
	{
		auto& impl = *Impl;
		if (impl.Closing) {
			return;
		}
		try {
			impl.disarm();
		} catch (...) { }
		impl.Closing = true;
		// The effect lane runs its writes in order before it closes.
		if (impl.WritePending && impl.Writable) {
			impl.post_write();
		}
	}

	auto pull_request_activity_host::state() const noexcept -> pull_request_activity_state
	{
		const auto& impl = *Impl;
		if (!impl.Memory.Enabled) {
			return pull_request_activity_state::off;
		}
		if (!impl.Account) {
			return pull_request_activity_state::signed_out;
		}
		if (impl.Account->ProfileAuthority.Authentication != github_authentication_transport::github_cli || impl.Schedule.Stopped) {
			return pull_request_activity_state::unsupported;
		}
		return impl.Schedule.Failures != 0U ? pull_request_activity_state::waiting : pull_request_activity_state::watching;
	}

	auto pull_request_activity_host::statistics() const noexcept -> pull_request_activity_statistics
	{
		return Impl->Statistics;
	}

	auto pull_request_activity_host::memory() const noexcept -> const core::activity_memory&
	{
		return Impl->Memory;
	}

	auto pull_request_activity_host::quiescent() const noexcept -> bool
	{
		return !Impl->InFlight && Impl->Writes == 0U && !Impl->WritePending;
	}

	auto pull_request_activity_host::scheduled() const noexcept -> std::optional<std::uint32_t>
	{
		return Impl->Scheduled;
	}
} // namespace bpr::application
