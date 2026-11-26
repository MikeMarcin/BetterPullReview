#include "bpr/application/prompt_macro_host.hpp"

#include <atomic>
#include <deque>
#include <random>
#include <string_view>
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
		host.Waiting.emplace_back([weak = std::weak_ptr{ Implementation }] {
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

	void prompt_macro_host::save(core::prompt_macro_differences aDifferences, const std::uint64_t aFrom, std::function<void(prompt_macro_write)> aSettled)
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return;
		}
		host.Waiting.emplace_back([weak = std::weak_ptr{ Implementation }, differences = std::move(aDifferences), aFrom, settled = std::move(aSettled)]() mutable {
			const auto self = weak.lock();
			if (!self) {
				return;
			}
			const auto settle = [self](const std::function<void(prompt_macro_write)>& aSettle, const prompt_macro_write aWrite) {
				self->Projection.LastWrite = aWrite;
				if (aSettle) {
					try {
						aSettle(aWrite);
					} catch (...) { }
				}
			};
			if (!self->Projection.Readable) {
				settle(settled, prompt_macro_write::failed);
				self->tell();
				self->finish();
				return;
			}
			// Review N1: the revision the edits came from, not the one the
			// projection holds now; the store refuses a write over another
			// window's as stale.
			self->off_lane([store = self->Dependencies.Store, differences, aFrom] { return store != nullptr ? store->store(differences, aFrom) : prompt_macro_write::failed; },
						   [self, settle, told = std::move(settled)](std::optional<prompt_macro_write> aWrite) {
							   settle(told, aWrite.value_or(prompt_macro_write::failed));
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

	auto mint_prompt_uuid(const std::function<std::array<std::uint8_t, 16>()>& aRandom) -> std::string
	{
		auto bytes = std::array<std::uint8_t, 16>{ };
		if (aRandom) {
			bytes = aRandom();
		} else {
			auto random = std::random_device{ };
			for (auto& byte : bytes) {
				byte = static_cast<std::uint8_t>(random() & 0xFFU);
			}
		}
		// Version 4 and the RFC 4122 variant.
		bytes[6]              = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U);
		bytes[8]              = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U);
		constexpr auto digits = std::string_view{ "0123456789abcdef" };
		auto           text   = std::string{ };
		for (std::size_t index = 0U; index < bytes.size(); ++index) {
			if (index == 4U || index == 6U || index == 8U || index == 10U) {
				text += '-';
			}
			text += digits[bytes[index] >> 4U];
			text += digits[bytes[index] & 0x0FU];
		}
		return text;
	}

	auto prompt_macro_host::mint_identity() const -> std::string
	{
		return Implementation->Dependencies.MintIdentity ? Implementation->Dependencies.MintIdentity() : mint_prompt_uuid();
	}

	auto prompt_macro_host::projection() const noexcept -> const prompt_macro_projection& { return Implementation->Projection; }

	auto prompt_macro_host::differences() const noexcept -> const core::prompt_macro_differences& { return Implementation->Differences; }

	void prompt_macro_host::shutdown() noexcept { Implementation->Closing = true; }

	auto prompt_macro_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }

} // namespace bpr::application
