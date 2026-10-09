#include "bpr/application/profile_epoch_actor_registry.hpp"
#include "bpr/core/anonymous_read_policy.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bpr::application {
	struct profile_epoch_actor::implementation
	{
		implementation(effect_executor& aEffects, std::shared_ptr<std::atomic_bool> aReleased)
			: Effects{ aEffects }
			, Released{ std::move(aReleased) }
		{ }
		effect_executor&                   Effects;
		std::mutex                         Mutex;
		std::deque<std::function<void()>>  Queue;
		std::deque<std::function<void()>>  Preparation;
		bool                               Scheduled = false;
		std::unique_ptr<git_cache_session> Cache;
		std::shared_ptr<std::atomic_bool>  Released;
	};

	profile_epoch_actor::profile_epoch_actor(effect_executor& aEffects, std::shared_ptr<std::atomic_bool> aReleased)
		: Implementation{ std::make_unique<implementation>(aEffects, std::move(aReleased)) }
	{ }
	profile_epoch_actor::~profile_epoch_actor()
	{
		Implementation->Cache.reset();
		Implementation->Released->store(true, std::memory_order_release);
	}

	auto profile_epoch_actor::cache() noexcept -> std::unique_ptr<git_cache_session>&
	{
		return Implementation->Cache;
	}

	void profile_epoch_actor::enqueue(std::function<void()> aWork, const profile_epoch_actor_priority aPriority)
	{
		const auto       self = shared_from_this();
		std::scoped_lock lock{ Implementation->Mutex };
		auto&            queue = aPriority == profile_epoch_actor_priority::preparation ? Implementation->Preparation : Implementation->Queue;
		queue.push_back(std::move(aWork));
		if (Implementation->Scheduled) {
			return;
		}
		Implementation->Scheduled = true;
		try {
			Implementation->Effects.post([self] {
				while (true) {
					std::function<void()> work;
					{
						std::scoped_lock queueLock{ self->Implementation->Mutex };
						// [[BPR-1344]]. The reviewer's work first; preparation only
						// when none of theirs is waiting.
						auto& next = !self->Implementation->Queue.empty() ? self->Implementation->Queue : self->Implementation->Preparation;
						if (next.empty()) {
							self->Implementation->Scheduled = false;
							return;
						}
						work = std::move(next.front());
						next.pop_front();
					}
					try {
						work();
					} catch (...) {
						// Each worker owns its failure completion; one foreign callback
						// must not strand another window's already admitted work.
					}
				}
			});
		} catch (...) {
			Implementation->Scheduled = false;
			queue.pop_back();
			throw;
		}
	}

	struct profile_epoch_registry_state
	{
		profile_epoch_registry_state(core::bpr_profile_id aProfile, epoch_window_context_port* aWindows, git_cache_port& aCaches, effect_executor& aEffects)
			: Profile{ aProfile }
			, Windows{ aWindows }
			, Caches{ aCaches }
			, Effects{ aEffects }
		{ }
		core::bpr_profile_id       Profile;
		epoch_window_context_port* Windows;
		git_cache_port&            Caches;
		effect_executor&           Effects;
		std::atomic_bool           Closing = false;
		std::mutex                 Mutex;
		struct actor_entry
		{
			profile_epoch_actor_key            Key;
			std::weak_ptr<profile_epoch_actor> Actor;
			std::shared_ptr<std::atomic_bool>  Released;
		};
		std::vector<actor_entry>                                                                      Actors;
		std::vector<std::pair<core::bpr_window_context, std::weak_ptr<profile_epoch_actor_consumer>>> Consumers;
	};

	struct profile_epoch_actor_consumer::implementation
	{
		std::shared_ptr<profile_epoch_registry_state> State;
		core::bpr_window_context                      Context;
		std::atomic_bool                              Retired = false;
	};

	profile_epoch_actor_consumer::profile_epoch_actor_consumer(
		std::shared_ptr<profile_epoch_registry_state> aState, const core::bpr_window_context aContext)
		: Implementation{ std::make_unique<implementation>(std::move(aState), aContext) }
	{ }
	profile_epoch_actor_consumer::~profile_epoch_actor_consumer() = default;

	auto profile_epoch_actor_consumer::active() const noexcept -> bool
	{
		return !Implementation->Retired.load(std::memory_order_acquire)
			&& !Implementation->State->Closing.load(std::memory_order_acquire);
	}
	auto profile_epoch_actor_consumer::current() const noexcept -> bool
	{
		return active() && (!Implementation->State->Windows || Implementation->State->Windows->current(Implementation->Context));
	}
	void profile_epoch_actor_consumer::retire() noexcept
	{
		Implementation->Retired.store(true, std::memory_order_release);
	}

	auto profile_epoch_actor_consumer::acquire(const profile_epoch_actor_key& aKey)
		-> std::expected<std::shared_ptr<profile_epoch_actor>, profile_epoch_actor_failure>
	{
		if (!current()) {
			return std::unexpected{ profile_epoch_actor_failure::stale_window };
		}
		if (aKey.Profile.value() == 0U || aKey.ProfileRevision.value() == 0U
			|| aKey.Host.value() == 0U || !aKey.HostAuthority.valid()
			|| aKey.Repository.value() == 0U
			|| (aKey.ObjectFormat != core::git_hash_algorithm::sha1 && aKey.ObjectFormat != core::git_hash_algorithm::sha256)
			|| (aKey.Mode == core::github_access_mode::authenticated && aKey.Account.value() == 0U)
			|| (aKey.Mode == core::github_access_mode::public_preview && aKey.Account.value() != 0U)
			|| (aKey.Mode != core::github_access_mode::public_preview && aKey.Mode != core::github_access_mode::authenticated)) {
			return std::unexpected{ profile_epoch_actor_failure::invalid_authority };
		}
		auto&            state = *Implementation->State;
		std::scoped_lock lock{ state.Mutex };
		if (!active()) {
			return std::unexpected{ profile_epoch_actor_failure::closing };
		}
		std::erase_if(state.Actors, [](const auto& aEntry) { return aEntry.Released->load(std::memory_order_acquire); });
		for (const auto& [key, weak, released] : state.Actors) {
			if (key == aKey) {
				if (auto actor = weak.lock()) {
					return actor;
				}
				if (!released->load(std::memory_order_acquire)) {
					return std::unexpected{ profile_epoch_actor_failure::actor_draining };
				}
			}
		}
		if (state.Actors.size() >= 1'000U) {
			return std::unexpected{ profile_epoch_actor_failure::actor_limit };
		}
		auto released = std::make_shared<std::atomic_bool>(false);
		auto actor    = std::shared_ptr<profile_epoch_actor>{ new profile_epoch_actor{ state.Effects, released } };
		state.Actors.push_back({ aKey, actor, std::move(released) });
		return actor;
	}

	profile_epoch_actor_registry::profile_epoch_actor_registry(const core::bpr_profile_id aProfile,
															   epoch_window_context_port& aWindows, git_cache_port& aCaches, effect_executor& aEffects)
		: State{ std::make_shared<profile_epoch_registry_state>(aProfile, &aWindows, aCaches, aEffects) }
	{
		if (aProfile.value() == 0U) {
			throw std::invalid_argument{ "Invalid epoch registry profile" };
		}
	}
	profile_epoch_actor_registry::profile_epoch_actor_registry(git_cache_port& aCaches, effect_executor& aEffects)
		: State{ std::make_shared<profile_epoch_registry_state>(core::bpr_profile_id{ }, nullptr, aCaches, aEffects) }
	{ }
	auto profile_epoch_actor_registry::caches() noexcept -> git_cache_port& { return State->Caches; }

	auto profile_epoch_actor_registry::attach_legacy() -> std::shared_ptr<profile_epoch_actor_consumer>
	{
		return std::shared_ptr<profile_epoch_actor_consumer>{ new profile_epoch_actor_consumer{ State, { } } };
	}
	profile_epoch_actor_registry::~profile_epoch_actor_registry() { shutdown(); }

	auto profile_epoch_actor_registry::attach(const core::bpr_window_context aContext)
		-> std::expected<std::shared_ptr<profile_epoch_actor_consumer>, profile_epoch_actor_failure>
	{
		if (aContext.Profile != State->Profile || aContext.Window.value() == 0U
			|| aContext.Generation.value() == 0U || !State->Windows->current(aContext)) {
			return std::unexpected{ profile_epoch_actor_failure::stale_window };
		}
		std::scoped_lock lock{ State->Mutex };
		if (State->Closing.load(std::memory_order_acquire)) {
			return std::unexpected{ profile_epoch_actor_failure::closing };
		}
		std::erase_if(State->Consumers, [](const auto& aEntry) {
			const auto consumer = aEntry.second.lock();
			return !consumer || !consumer->active();
		});
		for (const auto& [context, weak] : State->Consumers) {
			static_cast<void>(weak);
			if (context.Window == aContext.Window) {
				return std::unexpected{ profile_epoch_actor_failure::already_attached };
			}
		}
		if (State->Consumers.size() >= core::maximum_bpr_windows_per_profile) {
			return std::unexpected{ profile_epoch_actor_failure::consumer_limit };
		}
		auto consumer = std::shared_ptr<profile_epoch_actor_consumer>{ new profile_epoch_actor_consumer{ State, aContext } };
		State->Consumers.emplace_back(aContext, consumer);
		return consumer;
	}
	void profile_epoch_actor_registry::shutdown() noexcept
	{
		State->Closing.store(true, std::memory_order_release);
	}
	auto profile_epoch_actor_registry::is_quiescent() const noexcept -> bool
	{
		if (!State->Closing.load(std::memory_order_acquire)) {
			return false;
		}
		std::scoped_lock lock{ State->Mutex };
		return std::ranges::all_of(State->Actors, [](const auto& aEntry) { return aEntry.Released->load(std::memory_order_acquire); });
	}
}
