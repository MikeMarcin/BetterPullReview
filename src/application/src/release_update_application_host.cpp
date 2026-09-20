#include "bpr/application/release_update_application_host.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <utility>

namespace bpr::application {
	using update_reason    = core::release_update_reason;
	using update_operation = core::release_update_operation;

	struct release_update_application_host::implementation : std::enable_shared_from_this<implementation>
	{
		struct job
		{
			core::release_update_context Context;
			std::stop_source             Stop;
			std::atomic<std::uint64_t>   ProgressBytes{ 0U };
			std::atomic<bool>            ProgressPosted{ false };
		};
		struct delivery
		{
			core::release_update_event                            Event;
			std::shared_ptr<const release_update_archive>         Archive;
			std::shared_ptr<const release_update_verified_bundle> Bundle;
		};

		core::release_update_state                            State;
		release_update_clock_port&                            Clock;
		decision_executor&                                    Decision;
		effect_executor&                                      Effects;
		std::shared_ptr<release_update_verification_port>     Verifier;
		release_update_projection_port*                       Projection;
		std::shared_ptr<release_update_access_port>           Access;
		std::shared_ptr<release_update_access_port>           PendingAccess;
		std::shared_ptr<const release_update_archive>         Archive;
		std::shared_ptr<const release_update_verified_bundle> Bundle;
		std::vector<std::shared_ptr<job>>                     Jobs;
		std::deque<delivery>                                  Ingress;
		std::atomic<std::size_t>                              Deliveries{ 0U };
		std::atomic<std::size_t>                              RunningEffects{ 0U };
		bool                                                  Driving             = false;
		bool                                                  AccessPending       = false;
		bool                                                  ShutdownRequested   = false;
		std::size_t                                           ReentrantAdmissions = 0U;
		static constexpr std::size_t                          maximum_jobs        = 4U;
		static constexpr std::size_t                          maximum_ingress     = 64U;

		implementation(core::release_update_state aState, release_update_clock_port& aClock,
					   decision_executor& aDecision, effect_executor& aEffects,
					   std::shared_ptr<release_update_verification_port> aVerifier, release_update_projection_port& aProjection)
			: State{ std::move(aState) }
			, Clock{ aClock }
			, Decision{ aDecision }
			, Effects{ aEffects }
			, Verifier{ std::move(aVerifier) }
			, Projection{ &aProjection }
		{ }

		[[nodiscard]] auto projection() const -> release_update_projection
		{
			return { State.Installed, State.Phase, State.Reason, State.Candidate, State.Preferences,
					 core::release_update_commands(State), State.Bytes, State.OfferVisible };
		}

		void cancel_all()
		{
			// Copy controls before request_stop: stop callbacks may reenter.
			const auto jobs = Jobs;
			for (const auto& item : jobs) {
				item->Stop.request_stop();
			}
		}

		void apply(delivery aDelivery)
		{
			const auto output = core::transition(State, aDelivery.Event, Clock.now());
			if (!State.Candidate) {
				Archive.reset();
				Bundle.reset();
			}
			if (output.Accepted && aDelivery.Archive && State.Candidate) {
				Archive = std::move(aDelivery.Archive);
			}
			if (output.Accepted && aDelivery.Bundle && State.Candidate) {
				Bundle = std::move(aDelivery.Bundle);
			}
			for (const auto& request : output.Requests) {
				execute(request);
			}
			if (Projection && (output.Changed || output.PreferencesChanged)) {
				try {
					Projection->publish(projection(), output.PreferencesChanged);
				} catch (...) { }
			}
		}

		void drain()
		{
			if (Driving) {
				return;
			}
			Driving             = true;
			ReentrantAdmissions = 0U;
			while (true) {
				if (ShutdownRequested && !State.Stopped) {
					Ingress.clear();
					Access.reset();
					PendingAccess.reset();
					AccessPending = false;
					apply({ core::release_update_shutdown{ }, { }, { } });
				} else if (AccessPending && !State.Stopped) {
					AccessPending      = false;
					Access             = std::move(PendingAccess);
					const auto binding = Access ? Access->binding() : core::release_update_access_id{ };
					const auto kind    = Access ? Access->kind() : core::release_update_access_kind::unavailable;
					apply({ core::release_update_access_changed{ binding, kind }, { }, { } });
				} else if (!Ingress.empty()) {
					auto next = std::move(Ingress.front());
					Ingress.pop_front();
					apply(std::move(next));
				} else {
					break;
				}
			}
			Driving = false;
		}

		[[nodiscard]] auto enqueue(core::release_update_event aEvent) -> bool
		{
			if (ShutdownRequested || Ingress.size() >= maximum_ingress
				|| (Driving && ReentrantAdmissions++ >= maximum_ingress)) {
				return false;
			}
			Ingress.push_back({ std::move(aEvent), { }, { } });
			drain();
			return true;
		}

		void progress(const std::shared_ptr<job>& aJob, const std::uint64_t aBytes)
		{
			aJob->ProgressBytes.store(aBytes);
			if (aJob->ProgressPosted.exchange(true)) {
				return;
			}
			++Deliveries;
			Decision.post([self = shared_from_this(), aJob] {
				const auto bytes = aJob->ProgressBytes.load();
				aJob->ProgressPosted.store(false);
				self->Ingress.push_back({ core::release_update_progress{ aJob->Context, bytes }, { }, { } });
				self->drain();
				--self->Deliveries;
			});
		}

		[[nodiscard]] auto perform(const std::shared_ptr<job>&                          aJob,
								   const std::shared_ptr<release_update_access_port>&   aAccess,
								   const std::shared_ptr<const release_update_archive>& aArchive) -> delivery
		{
			const auto& context = aJob->Context;
			const auto  stop    = aJob->Stop.get_token();
			if (stop.stop_requested()) {
				return { core::release_update_failed{ context, update_reason::cancelled }, { }, { } };
			}
			if (!lease_matches(context, aAccess)) {
				return { core::release_update_failed{ context, update_reason::access_required }, { }, { } };
			}
			switch (context.Operation) {
			case update_operation::check: {
				if (!aAccess) {
					break;
				}
				auto result = aAccess->latest(stop);
				if (result.Failure != update_reason::none) {
					return { core::release_update_failed{ context, result.Failure, result.RetryAfterSeconds }, { }, { } };
				}
				return { core::release_update_checked{ context, std::move(result.Release) }, { }, { } };
			}
			case update_operation::download: {
				if (!aAccess || !context.Candidate) {
					break;
				}
				auto result = aAccess->download(*context.Candidate, stop,
												[self = shared_from_this(), aJob](const auto aBytes) { self->progress(aJob, aBytes); });
				if (result.Failure != update_reason::none) {
					return { core::release_update_failed{ context, result.Failure, result.RetryAfterSeconds }, { }, { } };
				}
				if (!result.Archive || result.Archive->candidate() != *context.Candidate) {
					return { core::release_update_failed{ context, update_reason::changed_candidate }, { }, { } };
				}
				return { core::release_update_downloaded{ context, result.Bytes }, std::move(result.Archive), { } };
			}
			case update_operation::verify: {
				if (!Verifier || !aArchive || !context.Candidate) {
					break;
				}
				auto result = Verifier->verify(aArchive, stop);
				if (result.Failure != update_reason::none) {
					return { core::release_update_failed{ context, result.Failure }, { }, { } };
				}
				if (!result.Bundle || result.Bundle->candidate() != *context.Candidate) {
					return { core::release_update_failed{ context, update_reason::changed_candidate }, { }, { } };
				}
				return { core::release_update_verified{ context }, { }, std::move(result.Bundle) };
			}
			}
			return { core::release_update_failed{ context, update_reason::unavailable }, { }, { } };
		}

		[[nodiscard]] static auto lease_matches(const core::release_update_context&                aContext,
												const std::shared_ptr<release_update_access_port>& aAccess) noexcept -> bool
		{
			return aAccess && aAccess->kind() == core::release_update_access_kind::native_github_com
				&& aAccess->binding() == aContext.Access && aContext.Access.value() != 0U;
		}

		void execute(const core::release_update_request& aRequest)
		{
			if (aRequest.Cancel) {
				const auto found = std::ranges::find_if(Jobs, [&](const auto& aJob) { return aJob->Context == aRequest.Context; });
				if (found != Jobs.end()) {
					(*found)->Stop.request_stop();
				}
				return;
			}
			if (Jobs.size() >= maximum_jobs) {
				Ingress.push_back({ core::release_update_failed{ aRequest.Context, update_reason::busy }, { }, { } });
				return;
			}
			auto current     = std::make_shared<job>();
			current->Context = aRequest.Context;
			Jobs.push_back(current);
			++RunningEffects;
			try {
				Effects.post([self = shared_from_this(), current, access = Access, archive = Archive] {
					delivery result{ core::release_update_failed{ current->Context, update_reason::unavailable }, { }, { } };
					try {
						result = self->perform(current, access, archive);
					} catch (...) { }
					if (!lease_matches(current->Context, access)) {
						result = { core::release_update_failed{ current->Context, update_reason::access_required }, { }, { } };
					}
					++self->Deliveries;
					self->Decision.post([self, current, lease = access, completed = std::move(result)]() mutable {
						std::erase(self->Jobs, current);
						if (!lease_matches(current->Context, lease)) {
							completed = { core::release_update_failed{ current->Context, update_reason::access_required }, { }, { } };
						}
						self->Ingress.push_back(std::move(completed));
						self->drain();
						--self->Deliveries;
					});
					--self->RunningEffects;
				});
			} catch (...) {
				--RunningEffects;
				std::erase(Jobs, current);
				Ingress.push_back({ core::release_update_failed{ aRequest.Context, update_reason::unavailable }, { }, { } });
			}
		}
	};

	release_update_application_host::release_update_application_host(const core::application_session_id aSession,
																	 const std::string_view aInstalled, const core::release_update_capability aCapability,
																	 core::release_update_preferences aPreferences, release_update_clock_port& aClock,
																	 decision_executor& aDecision, effect_executor& aEffects,
																	 std::shared_ptr<release_update_verification_port> aVerifier, release_update_projection_port& aProjection)
		: Impl{ std::make_shared<implementation>(core::make_release_update_state(aSession, aInstalled,
																				 aCapability, std::move(aPreferences), aClock.now()),
												 aClock, aDecision, aEffects,
												 std::move(aVerifier), aProjection) }
	{ }

	release_update_application_host::~release_update_application_host()
	{
		Impl->Projection = nullptr;
		shutdown();
	}

	void release_update_application_host::select_access(std::shared_ptr<release_update_access_port> aAccess)
	{
		if (Impl->ShutdownRequested) {
			return;
		}
		// A feedback cycle may revoke authority but cannot retain an unbounded
		// chain of replacement generations in one decision turn.
		if (Impl->Driving && Impl->ReentrantAdmissions++ >= implementation::maximum_ingress) {
			aAccess.reset();
		}
		if (aAccess == (Impl->AccessPending ? Impl->PendingAccess : Impl->Access)) {
			return;
		}
		Impl->PendingAccess = std::move(aAccess);
		Impl->AccessPending = true;
		Impl->cancel_all();
		Impl->drain();
	}
	auto release_update_application_host::dispatch(core::release_update_intent aIntent) -> bool { return Impl->enqueue(std::move(aIntent)); }
	auto release_update_application_host::tick() -> bool { return Impl->enqueue(core::release_update_tick{ }); }
	auto release_update_application_host::set_automatic_checks(const bool aEnabled) -> bool { return Impl->enqueue(core::release_update_set_automatic{ aEnabled }); }
	void release_update_application_host::shutdown()
	{
		Impl->ShutdownRequested = true;
		Impl->cancel_all();
		Impl->drain();
	}
	auto release_update_application_host::quiescent() const noexcept -> bool
	{
		return Impl->Jobs.empty() && Impl->Ingress.empty() && !Impl->Driving && !Impl->AccessPending
			&& Impl->Deliveries.load() == 0U && Impl->RunningEffects.load() == 0U;
	}
	auto release_update_application_host::projection() const -> release_update_projection { return Impl->projection(); }
}
