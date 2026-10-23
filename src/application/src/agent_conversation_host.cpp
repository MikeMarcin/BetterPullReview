#include "bpr/application/agent_conversation_host.hpp"

#include <atomic>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>

namespace bpr::application {
	namespace {
		/// Cuts to the longest prefix within the text bound that ends on a UTF-8 character
		/// boundary.
		void cut(std::string& aText)
		{
			if (aText.size() <= maximum_conversation_text_bytes) {
				return;
			}
			auto size = maximum_conversation_text_bytes;
			while (size > 0U && (static_cast<unsigned char>(aText[size]) & 0xC0U) == 0x80U) {
				--size;
			}
			aText.resize(size);
		}
	} // namespace

	auto bound_conversation(std::vector<agent_conversation_turn> aTurns) -> std::vector<agent_conversation_turn>
	{
		if (aTurns.size() > maximum_conversation_turns) {
			aTurns.erase(aTurns.begin(), aTurns.end() - static_cast<std::ptrdiff_t>(maximum_conversation_turns));
		}
		for (auto& turn : aTurns) {
			cut(turn.Asked);
			cut(turn.Answered);
			cut(turn.Ending);
		}
		return aTurns;
	}

	struct agent_conversation_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(agent_conversation_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{ }

		template <typename Task, typename Settle>
		void off_lane(Task aTask, Settle aSettle)
		{
			if (Closing || Dependencies.Effects == nullptr || Dependencies.Decision == nullptr) {
				return;
			}
			++Outstanding;
			try {
				Dependencies.Effects->post([self = shared_from_this(), task = std::move(aTask), settle = std::move(aSettle)]() mutable {
					if (self->Closing) {
						--self->Outstanding;
						return;
					}
					auto result = std::optional<decltype(task())>{ };
					try {
						result.emplace(task());
					} catch (...) { }
					try {
						self->Dependencies.Decision->post([owner = self, reply = std::move(settle), outcome = std::move(result)]() mutable {
							--owner->Outstanding;
							if (!owner->Closing) {
								reply(std::move(outcome));
							}
						});
					} catch (...) {
						--self->Outstanding;
					}
				});
			} catch (...) {
				--Outstanding;
			}
		}

		void publish()
		{
			++View.Generation;
			if (Dependencies.Publish) {
				try {
					Dependencies.Publish(View);
				} catch (...) { }
			}
		}

		/// Writes one thread, or waits behind the write in flight; only the
		/// newest thread waiting is written after it.
		void write(agent_conversation_key aKey, std::vector<agent_conversation_turn> aTurns)
		{
			if (Writing) {
				Waiting.emplace(std::move(aKey), std::move(aTurns));
				return;
			}
			Writing = true;
			off_lane([store = Dependencies.Store, key = std::move(aKey), turns = std::move(aTurns)] { return store != nullptr && store->store(key, turns); },
					 [self = shared_from_this()](std::optional<bool> /*aStored*/) {
						 self->Writing = false;
						 if (self->Waiting) {
							 auto next = std::exchange(self->Waiting, std::nullopt);
							 self->write(std::move(next->first), std::move(next->second));
						 }
					 });
		}

		agent_conversation_dependencies                                                        Dependencies;
		agent_conversation_projection                                                          View;
		bool                                                                                   Writing = false;
		std::optional<std::pair<agent_conversation_key, std::vector<agent_conversation_turn>>> Waiting;
		std::atomic_bool                                                                       Closing{ false };
		std::atomic_size_t                                                                     Outstanding{ 0U };
	};

	agent_conversation_host::agent_conversation_host(agent_conversation_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	agent_conversation_host::~agent_conversation_host() { shutdown(); }

	void agent_conversation_host::open(const agent_conversation_key& aKey)
	{
		auto& host = *Implementation;
		if (!valid(aKey)) {
			return;
		}
		const auto samePull = host.View.Key && host.View.Key->Host == aKey.Host && host.View.Key->Owner == aKey.Owner && host.View.Key->Name == aKey.Name && host.View.Key->Number == aKey.Number;
		if (samePull) {
			return;
		}
		host.View.Key    = aKey;
		host.View.Turns  = { };
		host.View.Loaded = false;
		host.publish();
		struct read
		{
			std::uint32_t                                       Agent = 0U;
			std::optional<std::vector<agent_conversation_turn>> Turns;
		};
		host.off_lane(
			[store = host.Dependencies.Store, key = aKey]() mutable {
				auto result = read{ .Agent = key.Agent };
				if (store != nullptr) {
					result.Agent = store->load_agent(key).value_or(key.Agent);
					key.Agent    = result.Agent;
					result.Turns = store->load(key);
				}
				return result;
			},
			[self = Implementation, key = aKey](std::optional<read> aRead) {
				// Another pull request, or another agent, was opened meanwhile.
				if (self->View.Key != std::optional{ key } || self->View.Loaded) {
					return;
				}
				if (aRead) {
					self->View.Key->Agent = aRead->Agent;
					if (aRead->Turns) {
						self->View.Turns = bound_conversation(std::move(*aRead->Turns));
					}
				}
				self->View.Loaded = true;
				self->publish();
			});
	}

	void agent_conversation_host::switch_agent(const std::uint32_t aAgent)
	{
		auto& host = *Implementation;
		if (!host.View.Key || host.View.Key->Agent == aAgent || aAgent > 64U) {
			return;
		}
		host.View.Key->Agent = aAgent;
		host.View.Turns      = { };
		host.View.Loaded     = false;
		host.publish();
		host.off_lane(
			[store = host.Dependencies.Store, key = *host.View.Key] {
				if (store == nullptr) {
					return std::optional<std::vector<agent_conversation_turn>>{ };
				}
				static_cast<void>(store->store_agent(key));
				return store->load(key);
			},
			[self = Implementation, key = *host.View.Key](std::optional<std::optional<std::vector<agent_conversation_turn>>> aLoaded) {
				if (self->View.Key != std::optional{ key } || self->View.Loaded) {
					return;
				}
				if (aLoaded && *aLoaded) {
					self->View.Turns = bound_conversation(std::move(**aLoaded));
				}
				self->View.Loaded = true;
				self->publish();
			});
	}

	void agent_conversation_host::close()
	{
		auto& host = *Implementation;
		if (!host.View.Key) {
			return;
		}
		host.View = agent_conversation_projection{ .Generation = host.View.Generation };
		host.publish();
	}

	void agent_conversation_host::record(std::vector<agent_conversation_turn> aTurns)
	{
		auto& host = *Implementation;
		if (!host.View.Key) {
			return;
		}
		host.View.Turns  = bound_conversation(std::move(aTurns));
		host.View.Loaded = true;
		host.write(*host.View.Key, host.View.Turns);
	}

	void agent_conversation_host::clear()
	{
		auto& host = *Implementation;
		if (!host.View.Key) {
			return;
		}
		host.View.Turns.clear();
		host.View.Loaded = true;
		host.write(*host.View.Key, { });
	}

	auto agent_conversation_host::projection() const -> agent_conversation_projection { return Implementation->View; }

	void agent_conversation_host::shutdown() noexcept { Implementation->Closing = true; }

	auto agent_conversation_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }
} // namespace bpr::application
