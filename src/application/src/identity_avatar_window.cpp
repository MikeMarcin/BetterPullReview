#include "bpr/application/identity_avatar_window.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace bpr::application {
	struct identity_avatar_window::state : std::enable_shared_from_this<state>
	{
		struct entry
		{
			std::string                  Login;
			std::uint32_t                Pixels = 0U;
			markdown_image_fetch_request Fetch;
			std::optional<std::uint64_t> Ticket;
		};

		state(decision_executor& aDecision, markdown_image_service* aService, facts_source aFacts, publication aPublish)
			: Decision{ aDecision }
			, Service{ aService }
			, Facts{ std::move(aFacts) }
			, Publish{ std::move(aPublish) }
		{
		}

		decision_executor&           Decision;
		markdown_image_service*      Service;
		facts_source                 Facts;
		publication                  Publish;
		std::map<std::string, entry> Entries;
		std::string                  Scope;
		bool                         Closed = false;

		auto current_facts() const -> markdown_image_window_facts
		{
			try {
				return Facts ? Facts() : markdown_image_window_facts{ };
			} catch (...) {
				return { };
			}
		}

		void publish(std::vector<identity_avatar_projection> aAvatars) const
		{
			if (aAvatars.empty() || !Publish || Closed) {
				return;
			}
			try {
				Publish(std::move(aAvatars));
			} catch (...) {
				// Presentation failures belong to the projection owner; a picture
				// is never retried through this lane.
			}
		}

		void reset()
		{
			for (const auto& [key, item] : Entries) {
				if (item.Ticket && Service != nullptr) {
					Service->cancel(*item.Ticket);
				}
			}
			Entries.clear();
		}

		void request(const std::span<const core::identity_avatar_intent> aIntents)
		{
			if (Closed) {
				return;
			}
			const auto facts = current_facts();
			if (facts.Scope != Scope) {
				reset();
				Scope = facts.Scope;
			}
			const auto                              capable = facts.Available && Service != nullptr && Service->available();
			std::vector<identity_avatar_projection> immediate;
			for (const auto& intent : aIntents) {
				const auto login  = core::identity_avatar_folded_login(intent.Login);
				const auto pixels = core::identity_avatar_picture_pixels(intent.DevicePixels);
				if (!capable) {
					immediate.push_back({ .Login = login, .Pixels = pixels, .State = core::markdown_image_state::failed, .Image = nullptr });
					continue;
				}
				auto admitted = core::admit_identity_avatar(intent.Login, intent.DevicePixels, facts.Policy);
				if (!admitted) {
					immediate.push_back({ .Login = login, .Pixels = pixels, .State = admitted.error(), .Image = nullptr });
					continue;
				}
				auto key = core::identity_avatar_cache_key(facts.Policy.Hostname, facts.Account, admitted->Login, admitted->Pixels);
				if (Entries.contains(key)) {
					continue;
				}
				if (auto image = Service->cached(key)) {
					immediate.push_back({ .Login = admitted->Login, .Pixels = admitted->Pixels, .State = core::markdown_image_state::loaded, .Image = std::move(image) });
					continue;
				}
				if (Entries.size() >= core::identity_avatar_window_queue) {
					immediate.push_back({ .Login = admitted->Login, .Pixels = admitted->Pixels, .State = core::markdown_image_state::over_limit, .Image = nullptr });
					continue;
				}
				auto fetch         = facts.Fetch;
				fetch.First        = std::move(admitted->First);
				fetch.Policy       = std::move(admitted->Policy);
				fetch.MaximumBytes = core::identity_avatar_encoded_bytes;
				fetch.DeadlineMs   = core::identity_avatar_fetch_deadline_ms;
				auto& item         = Entries.emplace(key, entry{ .Login = admitted->Login, .Pixels = admitted->Pixels, .Fetch = std::move(fetch), .Ticket = std::nullopt })
										 .first->second;
				// The profile's disk cache answers first: a restarted application
				// shows the pictures it already holds without asking the host.
				auto weak = weak_from_this();
				if (Service->persistent()) {
					item.Ticket = Service->probe(key, [weak, key](std::shared_ptr<const markdown_decoded_image> aImage) {
						if (const auto self = weak.lock()) {
							self->probed(key, std::move(aImage));
						}
					});
					continue;
				}
				load(key, item);
			}
			publish(std::move(immediate));
		}

		void load(const std::string& aKey, entry& aItem)
		{
			auto weak    = weak_from_this();
			aItem.Ticket = Service->load({ .Key = aKey, .Fetch = aItem.Fetch },
										 [weak, aKey](const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage) {
											 if (const auto self = weak.lock()) {
												 self->loaded(aKey, aState, std::move(aImage));
											 }
										 });
		}

		void probed(const std::string& aKey, std::shared_ptr<const markdown_decoded_image> aImage)
		{
			const auto found = Entries.find(aKey);
			if (found == Entries.end() || Closed || current_facts().Scope != Scope) {
				return;
			}
			if (aImage == nullptr) {
				found->second.Ticket.reset();
				load(aKey, found->second);
				return;
			}
			loaded(aKey, core::markdown_image_state::loaded, std::move(aImage));
		}

		void loaded(const std::string& aKey, const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage)
		{
			const auto found = Entries.find(aKey);
			if (found == Entries.end() || Closed) {
				return;
			}
			auto item = std::move(found->second);
			Entries.erase(found);
			if (current_facts().Scope != Scope) {
				return;
			}
			const auto shown = aState == core::markdown_image_state::loaded && aImage != nullptr && !aImage->Frames.empty();
			publish({ identity_avatar_projection{ .Login  = std::move(item.Login),
												  .Pixels = item.Pixels,
												  .State  = shown ? core::markdown_image_state::loaded
																  : (aState == core::markdown_image_state::loaded ? core::markdown_image_state::failed : aState),
												  .Image  = shown ? std::move(aImage) : nullptr } });
		}
	};

	identity_avatar_window::identity_avatar_window(decision_executor& aDecision, markdown_image_service* aService, facts_source aFacts, publication aPublish)
		: State{ std::make_shared<state>(aDecision, aService, std::move(aFacts), std::move(aPublish)) }
	{
	}

	identity_avatar_window::~identity_avatar_window()
	{
		shutdown();
	}

	void identity_avatar_window::request(const std::span<const core::identity_avatar_intent> aIntents)
	{
		State->request(aIntents);
	}

	void identity_avatar_window::reset()
	{
		State->reset();
	}

	void identity_avatar_window::shutdown()
	{
		State->reset();
		State->Closed = true;
	}

	auto identity_avatar_window::quiescent() const noexcept -> bool
	{
		// Loads and probes belong to the profile's service, which reports its
		// own quiescence; this window holds no work of its own between turns.
		return true;
	}

	auto identity_avatar_window::pending() const noexcept -> std::size_t
	{
		return State->Entries.size();
	}
} // namespace bpr::application
