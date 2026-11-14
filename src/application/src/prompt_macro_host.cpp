#include "bpr/application/prompt_macro_host.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <utility>

namespace bpr::application {
	struct prompt_macro_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(prompt_macro_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{
			Projection.Set = core::merge_prompt_macros(core::shipped_prompt_macros(), { });
		}

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

		void tell()
		{
			if (Dependencies.Changed) {
				try {
					Dependencies.Changed(Projection);
				} catch (...) { }
			}
		}

		void adopt(const std::optional<prompt_macro_stored>& aStored)
		{
			Projection.Loaded = true;
			if (!aStored) {
				// The shipped chips still work; Settings says edits are
				// unavailable.
				Projection.Readable = false;
				Projection.Set      = core::merge_prompt_macros(core::shipped_prompt_macros(), { });
				Differences         = { };
				Projection.Newer.clear();
				return;
			}
			Projection.Readable = true;
			Projection.Revision = aStored->Revision;
			Projection.Newer    = aStored->Newer;
			Differences         = aStored->Differences;
			Projection.Set      = core::merge_prompt_macros(core::shipped_prompt_macros(), Differences);
		}

		/// One store operation at a time, in order.
		void next()
		{
			if (Busy || Waiting.empty()) {
				return;
			}
			Busy      = true;
			auto work = std::move(Waiting.front());
			Waiting.pop_front();
			work();
		}

		void finish()
		{
			Busy = false;
			next();
		}

		prompt_macro_dependencies         Dependencies;
		prompt_macro_projection           Projection;
		core::prompt_macro_differences    Differences;
		std::deque<std::function<void()>> Waiting;
		bool                              Busy = false;
		std::atomic_bool                  Closing{ false };
		std::atomic_size_t                Outstanding{ 0U };
	};

	prompt_macro_host::prompt_macro_host(prompt_macro_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	prompt_macro_host::~prompt_macro_host() { shutdown(); }

	void prompt_macro_host::load()
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return;
		}
		host.Waiting.push_back([weak = std::weak_ptr{ Implementation }] {
			const auto self = weak.lock();
			if (!self) {
				return;
			}
			self->off_lane([store = self->Dependencies.Store] { return store != nullptr ? store->load() : std::nullopt; },
						   [self](std::optional<std::optional<prompt_macro_stored>> aRead) {
							   const auto before = self->Projection;
							   self->adopt(aRead ? *aRead : std::nullopt);
							   if (self->Projection != before) {
								   self->tell();
							   }
							   self->finish();
						   });
		});
		host.next();
	}

	void prompt_macro_host::save(core::prompt_macro_differences aDifferences)
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return;
		}
		host.Waiting.push_back([weak = std::weak_ptr{ Implementation }, differences = std::move(aDifferences)]() mutable {
			const auto self = weak.lock();
			if (!self) {
				return;
			}
			if (!self->Projection.Readable) {
				self->Projection.LastWrite = prompt_macro_write::failed;
				self->tell();
				self->finish();
				return;
			}
			const auto from = self->Projection.Revision;
			self->off_lane([store = self->Dependencies.Store, differences, from] { return store != nullptr ? store->store(differences, from) : prompt_macro_write::failed; },
						   [self](std::optional<prompt_macro_write> aWrite) {
							   self->Projection.LastWrite = aWrite.value_or(prompt_macro_write::failed);
							   // Read back what the store holds now: the write, or
							   // another window's.
							   self->off_lane([store = self->Dependencies.Store] { return store != nullptr ? store->load() : std::nullopt; },
											  [self](std::optional<std::optional<prompt_macro_stored>> aRead) {
												  self->adopt(aRead ? *aRead : std::nullopt);
												  self->tell();
												  self->finish();
											  });
						   });
		});
		host.next();
	}

	auto prompt_macro_host::projection() const noexcept -> const prompt_macro_projection& { return Implementation->Projection; }

	auto prompt_macro_host::differences() const noexcept -> const core::prompt_macro_differences& { return Implementation->Differences; }

	void prompt_macro_host::shutdown() noexcept { Implementation->Closing = true; }

	auto prompt_macro_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }

	auto prompt_macro_differences_of(const std::vector<core::prompt_builtin>& aShipped, const std::vector<core::prompt_macro>& aEdited) -> core::prompt_macro_differences
	{
		auto differences = core::prompt_macro_differences{ };
		for (const auto& macro : aEdited) {
			if (!macro.BuiltIn) {
				differences.Own.push_back(macro);
				continue;
			}
			const auto shipped = std::ranges::find(aShipped, macro.Id, [](const core::prompt_builtin& aBuiltin) -> const std::string& { return aBuiltin.Macro.Id; });
			if (shipped == aShipped.end()) {
				continue;
			}
			auto change = core::prompt_builtin_change{ .Id = macro.Id };
			if (macro.Body != shipped->Macro.Body) {
				change.Body           = macro.Body;
				change.WrittenAgainst = shipped->Revision;
			}
			if (macro.On != shipped->Macro.On) {
				change.On = macro.On;
			}
			if (change.Body || change.On) {
				differences.Changes.push_back(std::move(change));
			}
		}
		// A place's order is kept only where it is not what the merge makes
		// without one.
		const auto unordered = core::merge_prompt_macros(aShipped, { .Changes = differences.Changes, .Own = differences.Own }).macros();
		for (auto place = std::size_t{ 0 }; place < core::prompt_place_count; ++place) {
			const auto here = static_cast<core::prompt_place>(place);
			const auto ids  = [here](const std::vector<core::prompt_macro>& aMacros) {
				auto result = std::vector<std::string>{ };
				for (const auto& macro : aMacros) {
					if (macro.Place == here) {
						result.push_back(macro.Id);
					}
				}
				return result;
			};
			if (auto edited = ids(aEdited); edited != ids(unordered)) {
				differences.Orders.push_back({ .Place = here, .Ids = std::move(edited) });
			}
		}
		return differences;
	}
} // namespace bpr::application
