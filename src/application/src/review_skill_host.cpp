#include "bpr/application/review_skill_host.hpp"

#include "bpr/core/sha256.hpp"
#include "review_skill_data.hpp"

#include <atomic>
#include <cstddef>
#include <span>
#include <utility>

namespace bpr::application {
	auto built_in_review_skill() noexcept -> std::string_view
	{
		return detail::BuiltInReviewSkill;
	}

	namespace {
		[[nodiscard]] auto digest_of(const std::string_view aText) noexcept -> std::array<std::uint8_t, 32>
		{
			return core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() });
		}
	} // namespace

	struct review_skill_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(review_skill_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{
			const auto facts = core::inspect_review_skill(Dependencies.BuiltIn);
			BuiltInVersion   = facts && facts->Version ? *facts->Version : 0U;
			View.Text        = Dependencies.BuiltIn;
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
							if (!owner->Closing && outcome) {
								reply(std::move(*outcome));
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

		/// Recomputes the projection from the built-in text and the copy.
		void settle_view()
		{
			View.BuiltInVersion = BuiltInVersion;
			View.StoredProblem.reset();
			View.CopyBaseVersion.reset();
			View.Offer.reset();
			if (!Copy) {
				View.Source = core::review_skill_source::built_in;
				View.Text   = Dependencies.BuiltIn;
				return;
			}
			View.Source          = core::review_skill_source::profile_copy;
			View.Text            = Copy->Text;
			View.CopyBaseVersion = Copy->Record.BaseVersion;
			View.Offer           = core::newer_review_skill(BuiltInVersion, Copy->Record);
			if (const auto facts = core::inspect_review_skill(Copy->Text); !facts) {
				View.StoredProblem = facts.error();
			}
		}

		void publish()
		{
			settle_view();
			++View.Generation;
			if (Dependencies.Publish) {
				try {
					Dependencies.Publish(View);
				} catch (...) { }
			}
		}

		static void reply(const answer& aAnswered, const review_skill_answer aAnswer)
		{
			if (aAnswered) {
				try {
					aAnswered(aAnswer);
				} catch (...) { }
			}
		}

		review_skill_dependencies          Dependencies;
		review_skill_projection            View;
		std::optional<stored_review_skill> Copy;
		std::uint32_t                      BuiltInVersion = 0U;
		std::atomic_bool                   Closing{ false };
		std::atomic_size_t                 Outstanding{ 0U };
	};

	review_skill_host::review_skill_host(review_skill_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	review_skill_host::~review_skill_host() { shutdown(); }

	void review_skill_host::load()
	{
		auto& host = *Implementation;
		host.off_lane([store = host.Dependencies.Store] { return store != nullptr ? store->load_review_skill() : std::optional<std::optional<stored_review_skill>>{ }; },
					  [self = Implementation](std::optional<std::optional<stored_review_skill>> aLoaded) {
						  if (aLoaded) {
							  self->Copy = std::move(*aLoaded);
						  }
						  self->View.Loaded = true;
						  self->publish();
					  });
	}

	void review_skill_host::save(std::string aText, answer aAnswered)
	{
		auto& host = *Implementation;
		if (const auto facts = core::inspect_review_skill(aText); !facts) {
			host.View.Refusal = facts.error();
			host.publish();
			implementation::reply(aAnswered, review_skill_answer::refused);
			return;
		}
		host.View.Refusal.reset();
		if (!host.Copy && aText == host.Dependencies.BuiltIn) {
			host.publish();
			implementation::reply(aAnswered, review_skill_answer::done);
			return;
		}
		auto next    = host.Copy.value_or(stored_review_skill{ .Record = { .BaseVersion = host.BuiltInVersion, .BaseDigest = digest_of(host.Dependencies.BuiltIn) } });
		next.Text    = std::move(aText);
		auto written = next;
		host.off_lane([store = host.Dependencies.Store, skill = std::move(written)] { return store != nullptr && store->store_review_skill(skill); },
					  [self = Implementation, skill = std::move(next), reply = std::move(aAnswered)](const bool aStored) mutable {
						  if (aStored) {
							  self->Copy = std::move(skill);
						  }
						  self->publish();
						  implementation::reply(reply, aStored ? review_skill_answer::done : review_skill_answer::not_saved);
					  });
	}

	void review_skill_host::use_built_in(const bool aSaveCopy, answer aAnswered)
	{
		auto& host = *Implementation;
		if (!host.Copy) {
			implementation::reply(aAnswered, review_skill_answer::done);
			return;
		}
		struct outcome
		{
			review_skill_answer        Answer = review_skill_answer::not_saved;
			std::optional<std::string> SavedCopy;
		};
		host.off_lane(
			[store = host.Dependencies.Store, save = host.Dependencies.SaveCopy, text = host.Copy->Text, aSaveCopy] {
				auto result = outcome{ };
				if (aSaveCopy) {
					result.SavedCopy = save ? save(text) : std::nullopt;
					if (!result.SavedCopy) {
						result.Answer = review_skill_answer::copy_failed;
						return result;
					}
				}
				result.Answer = store != nullptr && store->forget_review_skill() ? review_skill_answer::done : review_skill_answer::not_saved;
				return result;
			},
			[self = Implementation, reply = std::move(aAnswered)](outcome aOutcome) {
				if (aOutcome.Answer == review_skill_answer::done) {
					self->Copy.reset();
				}
				self->View.SavedCopy = std::move(aOutcome.SavedCopy);
				self->publish();
				implementation::reply(reply, aOutcome.Answer);
			});
	}

	void review_skill_host::keep_mine(answer aAnswered)
	{
		auto& host = *Implementation;
		if (!host.Copy || !host.View.Offer) {
			implementation::reply(aAnswered, review_skill_answer::done);
			return;
		}
		auto next               = *host.Copy;
		next.Record.KeptVersion = host.View.Offer;
		auto written            = next;
		host.off_lane([store = host.Dependencies.Store, skill = std::move(written)] { return store != nullptr && store->store_review_skill(skill); },
					  [self = Implementation, skill = std::move(next), reply = std::move(aAnswered)](const bool aStored) mutable {
						  if (aStored) {
							  self->Copy = std::move(skill);
						  }
						  self->publish();
						  implementation::reply(reply, aStored ? review_skill_answer::done : review_skill_answer::not_saved);
					  });
	}

	auto review_skill_host::brief_skill() const -> std::optional<core::brief_review_skill>
	{
		const auto& host = *Implementation;
		if (host.Copy) {
			if (const auto facts = core::inspect_review_skill(host.Copy->Text)) {
				return core::brief_review_skill{ .Source = core::review_skill_source::profile_copy, .Version = facts->Version, .Text = host.Copy->Text };
			}
		}
		if (!core::inspect_review_skill(host.Dependencies.BuiltIn)) {
			return std::nullopt;
		}
		return core::brief_review_skill{ .Source = core::review_skill_source::built_in, .Version = host.BuiltInVersion, .Text = host.Dependencies.BuiltIn };
	}

	auto review_skill_host::projection() const -> review_skill_projection
	{
		return Implementation->View;
	}

	void review_skill_host::shutdown() noexcept
	{
		Implementation->Closing = true;
	}

	auto review_skill_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Outstanding.load() == 0U;
	}
} // namespace bpr::application
