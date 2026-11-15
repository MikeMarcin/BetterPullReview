#include "bpr/application/prompt_turn_name_host.hpp"

#include <atomic>
#include <deque>
#include <utility>

namespace bpr::application {
	struct prompt_turn_name_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(prompt_turn_name_dependencies aDependencies)
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

		/// Writes the oldest name waiting, one at a time and in order.
		void write_next()
		{
			if (Writing || Waiting.empty()) {
				return;
			}
			Writing   = true;
			auto next = std::move(Waiting.front());
			Waiting.pop_front();
			off_lane([store = Dependencies.Store, name = std::move(next)] { return store != nullptr && store->store(name); },
					 [self = shared_from_this()](std::optional<bool> /*aStored*/) {
						 self->Writing = false;
						 self->write_next();
					 });
		}

		prompt_turn_name_dependencies      Dependencies;
		core::prompt_turn_names            Names;
		std::deque<core::prompt_turn_name> Waiting;
		bool                               Writing = false;
		bool                               Asked   = false;
		bool                               Loaded  = false;
		std::atomic_bool                   Closing{ false };
		std::atomic_size_t                 Outstanding{ 0U };
	};

	prompt_turn_name_host::prompt_turn_name_host(prompt_turn_name_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	prompt_turn_name_host::~prompt_turn_name_host() { shutdown(); }

	void prompt_turn_name_host::load()
	{
		auto& host = *Implementation;
		if (host.Asked) {
			return;
		}
		host.Asked = true;
		host.off_lane([store = host.Dependencies.Store] { return store != nullptr ? store->load() : std::nullopt; },
					  [self = Implementation](std::optional<std::optional<std::vector<core::prompt_turn_name>>> aRead) {
						  if (aRead && *aRead) {
							  self->Names.adopt(**aRead);
						  }
						  self->Loaded = true;
						  if (self->Dependencies.Loaded) {
							  try {
								  self->Dependencies.Loaded();
							  } catch (...) { }
						  }
					  });
	}

	void prompt_turn_name_host::remember(core::prompt_turn_name aName)
	{
		auto& host = *Implementation;
		if (host.Closing || !host.Names.remember(aName)) {
			return;
		}
		host.Waiting.push_back(std::move(aName));
		host.write_next();
	}

	auto prompt_turn_name_host::names() const noexcept -> const core::prompt_turn_names& { return Implementation->Names; }

	auto prompt_turn_name_host::is_loaded() const noexcept -> bool { return Implementation->Loaded; }

	void prompt_turn_name_host::shutdown() noexcept { Implementation->Closing = true; }

	auto prompt_turn_name_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }
} // namespace bpr::application
