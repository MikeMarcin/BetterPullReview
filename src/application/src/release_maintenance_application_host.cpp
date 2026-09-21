#include "bpr/application/release_maintenance_application_host.hpp"
#include "bpr/core/sha256.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <utility>

namespace bpr::application {
	namespace {
		using command            = core::release_maintenance_command;
		using reason             = core::release_maintenance_reason;
		using preparation_result = std::expected<std::shared_ptr<const release_maintenance_preparation>, reason>;

		auto preparing(command aCommand) noexcept -> bool
		{
			return aCommand == command::preview_diagnostics || aCommand == command::prepare_clear || aCommand == command::prepare_purge;
		}
		auto effect_failure(reason aReason) noexcept -> reason
		{
			// Native admission failures are observations, not malformed core intents.
			switch (aReason) {
			case reason::invalid_authority:
			case reason::invalid_selection:    return reason::selection_changed;
			case reason::unavailable:
			case reason::io_failure:
			case reason::selection_changed:
			case reason::privacy_refusal:
			case reason::limit_exceeded:
			case reason::incomplete_inventory:
			case reason::recovery_required:    return aReason;
			default:                           return reason::io_failure;
			}
		}
		auto valid_snapshot(const release_maintenance_preparation& aPreparation) -> bool
		{
			const auto  selection = aPreparation.selection();
			const auto* snapshot  = std::get_if<core::release_diagnostic_snapshot>(&selection);
			const auto  bytes     = aPreparation.preview();
			return snapshot && bytes.size() == snapshot->Bytes && bytes.size() <= core::release_diagnostic_byte_limit
				&& core::sha256({ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() }) == snapshot->Digest;
		}
	}

	struct release_maintenance_application_host::implementation : std::enable_shared_from_this<implementation>
	{
		struct job
		{
			core::release_maintenance_context                      Context;
			std::shared_ptr<const release_maintenance_preparation> Preparation;
			std::shared_ptr<release_diagnostic_output>             Output;
			std::stop_source                                       Stop;
			std::mutex                                             Mutex;
			bool                                                   Revoked          = false;
			bool                                                   Committed        = false;
			bool                                                   BarrierDelivered = false;
		};
		release_maintenance_storage_port&                      Storage;
		release_maintenance_lifecycle_port&                    Lifecycle;
		effect_executor&                                       Effects;
		decision_executor&                                     Decisions;
		release_maintenance_projection_port&                   Projection;
		core::release_maintenance_state                        State;
		std::shared_ptr<job>                                   Live;
		std::shared_ptr<const release_maintenance_preparation> Prepared;
		std::shared_ptr<const std::string>                     Preview;
		std::deque<std::function<void()>>                      Ingress;
		bool                                                   Turning      = false;
		bool                                                   Closing      = false;
		bool                                                   Invalidating = false;
		bool                                                   Overflow     = false;
		std::atomic<std::size_t>                               PendingDecisions{ 0U };

		implementation(release_maintenance_storage_port& aStorage, release_maintenance_lifecycle_port& aLifecycle,
					   effect_executor& aEffects, decision_executor& aDecisions, release_maintenance_projection_port& aProjection)
			: Storage{ aStorage }
			, Lifecycle{ aLifecycle }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Projection{ aProjection }
			, State{ core::make_release_maintenance_state(aStorage.authority(), aStorage.capabilities()) }
		{ }

		void publish(const core::release_maintenance_output& aDelta) noexcept
		{
			try {
				Projection.publish({ State, core::release_maintenance_commands(State), Preview, aDelta });
			} catch (...) { }
		}
		void post_decision(std::function<void()> aAction)
		{
			++PendingDecisions;
			Decisions.post([self = shared_from_this(), action = std::move(aAction)] {
				self->turn(action);
				--self->PendingDecisions;
			});
		}
		void turn(std::function<void()> aAction)
		{
			if (Turning) {
				if (Ingress.size() < 32U) {
					Ingress.push_back(std::move(aAction));
				} else {
					Overflow = true;
				}
				return;
			}
			Turning = true;
			aAction();
			drain_ingress();
			Turning = false;
		}
		void drain_ingress()
		{
			while (Closing || Invalidating || !Ingress.empty() || Overflow) {
				if (Closing && !State.Stopped) {
					Ingress.clear();
					Invalidating = false;
					accept(core::release_maintenance_shutdown{ });
				} else if (Invalidating) {
					Invalidating = false;
					accept(core::release_maintenance_invalidated{ State.Authority });
				} else if (!Ingress.empty()) {
					auto action = std::move(Ingress.front());
					Ingress.pop_front();
					action();
				} else if (Overflow) {
					Overflow = false;
					publish({ .Diagnostic = reason::limit_exceeded });
				} else {
					break;
				}
			}
		}
		void revoke()
		{
			auto live = Live;
			if (!live) {
				return;
			}
			bool committed = false;
			{
				const std::lock_guard lock{ live->Mutex };
				live->Revoked = true;
				committed     = live->Committed;
			}
			// The effect's boundary may have crossed before its posted observation ran.
			if (committed) {
				static_cast<void>(core::transition(State, core::release_maintenance_commit_started{ live->Context }));
			}
			live->Stop.request_stop();
		}
		void accept(core::release_maintenance_event aEvent)
		{
			auto delta = core::transition(State, aEvent);
			if (delta.SelectionChanged || delta.AuthorityChanged) {
				Prepared.reset();
				Preview.reset();
			}
			if (delta.Request && delta.Request->Cancel) {
				revoke();
			}
			publish(delta);
		}
		void dispatch(core::release_maintenance_intent aIntent, std::shared_ptr<release_diagnostic_output> aOutput)
		{
			if (Live && aIntent.Command != command::cancel) {
				publish({ .Diagnostic = reason::busy });
				return;
			}
			auto preparation = Prepared;
			auto delta       = core::transition(State, aIntent);
			if (delta.SelectionChanged) {
				Prepared.reset();
				Preview.reset();
			}
			if (!delta.Request) {
				publish(delta);
				return;
			}
			if (delta.Request->Cancel) {
				revoke();
				publish(delta);
				return;
			}
			std::shared_ptr<job> live;
			try {
				live = std::make_shared<job>();
			} catch (...) {
				accept(core::release_maintenance_failed{ delta.Request->Context, reason::unavailable });
				return;
			}
			live->Context     = delta.Request->Context;
			live->Preparation = std::move(preparation);
			live->Output      = std::move(aOutput);
			Live              = live;
			publish(delta);
			if (live->Context.Operation == command::prepare_clear || live->Context.Operation == command::prepare_purge) {
				barrier(live);
			} else {
				submit(live);
			}
		}
		void barrier(const std::shared_ptr<job>& aJob)
		{
			auto callback = [self = shared_from_this(), aJob](release_maintenance_result aResult) {
				{
					const std::lock_guard lock{ aJob->Mutex };
					if (aJob->BarrierDelivered) {
						return;
					}
					aJob->BarrierDelivered = true;
				}
				self->post_decision([self, aJob, result = std::move(aResult)] {
					if (!result) {
						self->finish(aJob, std::unexpected(result.error()));
					} else {
						self->submit(aJob);
					}
				});
			};
			try {
				Lifecycle.quiesce(aJob->Context, aJob->Stop.get_token(), callback);
			} catch (...) {
				callback(std::unexpected(reason::unavailable));
			}
		}
		auto begin_commit(const std::shared_ptr<job>& aJob) -> bool
		{
			const std::lock_guard lock{ aJob->Mutex };
			if (aJob->Revoked || aJob->Stop.stop_requested() || aJob->Committed) {
				return false;
			}
			aJob->Committed = true;
			post_decision([self = shared_from_this(), aJob] {
				self->accept(core::release_maintenance_commit_started{ aJob->Context });
			});
			return true;
		}
		auto execute(const std::shared_ptr<job>& aJob) -> release_maintenance_result
		{
			if (!aJob->Preparation || aJob->Preparation->selection() != aJob->Context.Selection) {
				return std::unexpected(reason::selection_changed);
			}
			auto admitted = Storage.revalidate(aJob->Context, *aJob->Preparation);
			if (!admitted) {
				return admitted;
			}
			const auto commit = [self = shared_from_this(), aJob] { return self->begin_commit(aJob); };
			if (aJob->Context.Operation == command::export_diagnostics) {
				if (!aJob->Output) {
					return std::unexpected(reason::unavailable);
				}
				if (!valid_snapshot(*aJob->Preparation)) {
					return std::unexpected(reason::privacy_refusal);
				}
				return aJob->Output->write(aJob->Preparation->preview(), aJob->Stop.get_token(), commit);
			}
			return Storage.erase(aJob->Context, *aJob->Preparation, aJob->Stop.get_token(), commit);
		}
		void submit(const std::shared_ptr<job>& aJob)
		{
			try {
				Effects.post([self = shared_from_this(), aJob] {
					preparation_result result = std::unexpected(reason::unavailable);
					try {
						if (aJob->Stop.stop_requested()) {
							result = std::unexpected(reason::cancelled);
						} else if (preparing(aJob->Context.Operation)) {
							result = self->Storage.prepare(aJob->Context, aJob->Stop.get_token());
						} else {
							auto execution = self->execute(aJob);
							result         = execution ? preparation_result{ nullptr } : std::unexpected(execution.error());
						}
					} catch (...) {
						result = std::unexpected(reason::io_failure);
					}
					self->post_decision([self, aJob, outcome = std::move(result)] {
						self->finish(aJob, outcome);
					});
				});
			} catch (...) {
				finish(aJob, std::unexpected(reason::unavailable));
			}
		}
		void finish(const std::shared_ptr<job>& aJob, preparation_result aResult)
		{
			if (Live != aJob) {
				return;
			}
			if (aJob->Committed) {
				// Decision executors may reorder independent posted observations.
				static_cast<void>(core::transition(State, core::release_maintenance_commit_started{ aJob->Context }));
			}
			// All effect work has finished. Retire before observational/reentrant delivery.
			Live.reset();
			if (!aResult) {
				if (aResult.error() == reason::cancelled && !aJob->Committed) {
					accept(core::release_maintenance_cancelled{ aJob->Context });
				} else {
					accept(core::release_maintenance_failed{ aJob->Context, effect_failure(aResult.error()) });
				}
				return;
			}
			if (!preparing(aJob->Context.Operation)) {
				accept(core::release_maintenance_completed{ aJob->Context });
				return;
			}
			if (!*aResult || (aJob->Context.Operation == command::preview_diagnostics && !valid_snapshot(**aResult))) {
				accept(core::release_maintenance_failed{ aJob->Context, reason::privacy_refusal });
				return;
			}
			std::shared_ptr<const std::string> preview;
			try {
				if (aJob->Context.Operation == command::preview_diagnostics) {
					preview = std::make_shared<const std::string>((*aResult)->preview());
				}
			} catch (...) {
				accept(core::release_maintenance_failed{ aJob->Context, reason::unavailable });
				return;
			}
			auto delta = core::transition(State, core::release_maintenance_prepared{ aJob->Context, (*aResult)->selection() });
			if (delta.Accepted && State.Prepared) {
				Prepared = std::move(*aResult);
				Preview  = std::move(preview);
			}
			publish(delta);
		}
	};

	release_maintenance_application_host::release_maintenance_application_host(release_maintenance_storage_port&   aStorage,
																			   release_maintenance_lifecycle_port& aLifecycle, effect_executor& aEffects, decision_executor& aDecisions,
																			   release_maintenance_projection_port& aProjection)
		: Implementation{ std::make_shared<implementation>(aStorage, aLifecycle, aEffects, aDecisions, aProjection) }
	{ }
	release_maintenance_application_host::~release_maintenance_application_host() { shutdown(); }
	void release_maintenance_application_host::dispatch(core::release_maintenance_intent aIntent, std::shared_ptr<release_diagnostic_output> aOutput)
	{
		auto self = Implementation;
		self->turn([self, aIntent, output = std::move(aOutput)] { self->dispatch(aIntent, output); });
	}
	void release_maintenance_application_host::invalidate()
	{
		auto self          = Implementation;
		self->Invalidating = true;
		self->revoke();
		self->turn([] { });
	}
	void release_maintenance_application_host::shutdown()
	{
		auto self     = Implementation;
		self->Closing = true;
		self->revoke();
		self->turn([] { });
	}
	auto release_maintenance_application_host::state() const noexcept -> const core::release_maintenance_state& { return Implementation->State; }
	auto release_maintenance_application_host::is_quiescent() const noexcept -> bool
	{
		const auto& self = *Implementation;
		return !self.Live && !self.Turning && self.Ingress.empty() && self.PendingDecisions.load() == 0U;
	}
}
