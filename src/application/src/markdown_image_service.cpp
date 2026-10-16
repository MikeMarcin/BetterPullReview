#include "bpr/application/markdown_image_service.hpp"

#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <stop_token>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		/// The content types a fetched image may declare; the decoder still
		/// decides by signature. SVG is refused here without decoding.
		auto image_content_type(const std::string_view aType) -> bool
		{
			auto type = std::string{ aType.substr(0, aType.find(';')) };
			std::ranges::transform(type, type.begin(), [](const char aChar) { return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar; });
			while (!type.empty() && type.back() == ' ') {
				type.pop_back();
			}
			return type.starts_with("image/") && type != "image/svg+xml";
		}
	} // namespace

	struct markdown_image_service::state : std::enable_shared_from_this<state>
	{
		struct load
		{
			load_request                        Request;
			std::map<std::uint64_t, subscriber> Subscribers;
			std::stop_source                    Stop;
			bool                                Started = false;
		};

		struct cache_entry
		{
			std::string                                   Key;
			std::shared_ptr<const markdown_decoded_image> Image;
		};

		state(decision_executor& aDecision, effect_executor& aDecode, markdown_image_fetch_port* aFetch,
			  const markdown_image_decoder_port* aDecoder, const std::size_t aCacheBytes)
			: Decision{ aDecision }
			, Decode{ aDecode }
			, Fetch{ aFetch }
			, Decoder{ aDecoder }
			, CacheLimit{ aCacheBytes }
		{
		}

		decision_executor&                                                Decision;
		effect_executor&                                                  Decode;
		markdown_image_fetch_port*                                        Fetch;
		const markdown_image_decoder_port*                                Decoder;
		std::size_t                                                       CacheLimit;
		std::list<cache_entry>                                            Recent;
		std::unordered_map<std::string, std::list<cache_entry>::iterator> Cache;
		std::size_t                                                       CacheBytes = 0U;
		std::unordered_map<std::string, load>                             Loads;
		std::unordered_map<std::uint64_t, std::string>                    Tickets;
		std::deque<std::string>                                           Queue;
		std::size_t                                                       Active     = 0U;
		std::size_t                                                       Decoding   = 0U;
		std::uint64_t                                                     NextTicket = 1U;
		bool                                                              Closed     = false;

		auto cached(const std::string& aKey) -> std::shared_ptr<const markdown_decoded_image>
		{
			const auto found = Cache.find(aKey);
			if (found == Cache.end()) {
				return nullptr;
			}
			Recent.splice(Recent.begin(), Recent, found->second);
			return found->second->Image;
		}

		void remember(const std::string& aKey, std::shared_ptr<const markdown_decoded_image> aImage)
		{
			if (Cache.contains(aKey) || aImage == nullptr || aImage->bytes() > CacheLimit) {
				return;
			}
			CacheBytes += aImage->bytes();
			Recent.push_front({ aKey, std::move(aImage) });
			Cache.emplace(aKey, Recent.begin());
			while (CacheBytes > CacheLimit && !Recent.empty()) {
				CacheBytes -= Recent.back().Image->bytes();
				Cache.erase(Recent.back().Key);
				Recent.pop_back();
			}
		}

		/// Delivers one terminal to every subscriber of `aKey` and forgets the load.
		void finish(const std::string& aKey, const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage)
		{
			const auto found = Loads.find(aKey);
			if (found == Loads.end()) {
				return;
			}
			auto subscribers = std::move(found->second.Subscribers);
			Loads.erase(found);
			for (const auto& [ticket, subscriber] : subscribers) {
				Tickets.erase(ticket);
			}
			for (auto& [ticket, subscriber] : subscribers) {
				if (subscriber) {
					subscriber(aState, aImage);
				}
			}
		}

		void pump()
		{
			while (!Closed && Active < core::markdown_image_fetch_concurrency && !Queue.empty()) {
				const auto key = std::move(Queue.front());
				Queue.pop_front();
				const auto found = Loads.find(key);
				if (found == Loads.end() || found->second.Started) {
					continue;
				}
				start(key, found->second);
			}
		}

		void start(const std::string& aKey, load& aLoad)
		{
			aLoad.Started = true;
			++Active;
			auto       weak  = weak_from_this();
			auto*      lane  = &Decision;
			const auto token = aLoad.Stop.get_token();
			try {
				Fetch->fetch(aLoad.Request.Fetch, token, [weak, lane, key = aKey, token](markdown_image_fetch_result aResult) mutable {
					lane->post([weak, fetchedKey = std::move(key), token, fetchResult = std::move(aResult)]() mutable {
						if (const auto self = weak.lock()) {
							self->fetched(fetchedKey, token, std::move(fetchResult));
						}
					});
				});
			} catch (...) {
				--Active;
				finish(aKey, core::markdown_image_state::failed, nullptr);
			}
		}

		void fetched(const std::string& aKey, const std::stop_token& aToken, markdown_image_fetch_result aResult)
		{
			--Active;
			const auto found = Loads.find(aKey);
			// A load whose last subscriber left was erased and its fetch stopped;
			// a newer load of the same key owns a different stop token.
			if (Closed || aToken.stop_requested() || found == Loads.end() || found->second.Stop.get_token() != aToken) {
				pump();
				return;
			}
			if (!aResult) {
				finish(aKey, aResult.error(), nullptr);
			} else if (!image_content_type(aResult->ContentType)) {
				finish(aKey, core::markdown_image_state::unsupported, nullptr);
			} else if (aResult->Bytes.size() > found->second.Request.Fetch.MaximumBytes) {
				finish(aKey, core::markdown_image_state::too_large, nullptr);
			} else {
				decode(aKey, aToken, std::move(aResult->Bytes));
			}
			pump();
		}

		void decode(const std::string& aKey, const std::stop_token& aToken, std::string aBytes)
		{
			++Decoding;
			auto        weak    = weak_from_this();
			auto*       lane    = &Decision;
			const auto* decoder = Decoder;
			try {
				Decode.post([weak, lane, decoder, key = aKey, token = aToken, bytes = std::move(aBytes)]() mutable {
					auto result = std::expected<markdown_decoded_image, core::markdown_image_state>{ std::unexpected{ core::markdown_image_state::failed } };
					if (!token.stop_requested()) {
						try {
							result = decoder->decode(bytes);
						} catch (...) {
							result = std::unexpected{ core::markdown_image_state::failed };
						}
					}
					lane->post([weak, decodedKey = std::move(key), token, decodeResult = std::move(result)]() mutable {
						if (const auto self = weak.lock()) {
							self->decoded(decodedKey, token, std::move(decodeResult));
						}
					});
				});
			} catch (...) {
				--Decoding;
				finish(aKey, core::markdown_image_state::failed, nullptr);
			}
		}

		void decoded(const std::string& aKey, const std::stop_token& aToken, std::expected<markdown_decoded_image, core::markdown_image_state> aResult)
		{
			--Decoding;
			const auto found = Loads.find(aKey);
			if (Closed || aToken.stop_requested() || found == Loads.end() || found->second.Stop.get_token() != aToken) {
				return;
			}
			if (!aResult) {
				finish(aKey, aResult.error(), nullptr);
				return;
			}
			auto image = std::make_shared<const markdown_decoded_image>(std::move(*aResult));
			remember(aKey, image);
			finish(aKey, core::markdown_image_state::loaded, std::move(image));
		}
	};

	markdown_image_service::markdown_image_service(decision_executor& aDecision, effect_executor& aDecode, markdown_image_fetch_port* aFetch,
												   const markdown_image_decoder_port* aDecoder, const std::size_t aCacheBytes)
		: State{ std::make_shared<state>(aDecision, aDecode, aFetch, aDecoder, aCacheBytes) }
	{
	}

	markdown_image_service::~markdown_image_service()
	{
		shutdown();
	}

	auto markdown_image_service::available() const noexcept -> bool
	{
		return !State->Closed && State->Fetch != nullptr && State->Decoder != nullptr;
	}

	auto markdown_image_service::cached(const std::string& aKey) -> std::shared_ptr<const markdown_decoded_image>
	{
		return State->cached(aKey);
	}

	auto markdown_image_service::load(load_request aRequest, subscriber aSubscriber) -> std::uint64_t
	{
		auto&      lane   = *State;
		const auto ticket = lane.NextTicket++;
		if (!available()) {
			lane.Decision.post([subscriber = std::move(aSubscriber)] {
				if (subscriber) {
					subscriber(core::markdown_image_state::failed, nullptr);
				}
			});
			return ticket;
		}
		if (auto hit = lane.cached(aRequest.Key)) {
			auto weak = std::weak_ptr<state>{ State };
			lane.Tickets.emplace(ticket, std::string{ });
			lane.Decision.post([weak, ticket, subscriber = std::move(aSubscriber), image = std::move(hit)] {
				const auto self = weak.lock();
				// A ticket cancelled before this ran has left Tickets.
				if (self && !self->Closed && self->Tickets.erase(ticket) != 0U && subscriber) {
					subscriber(core::markdown_image_state::loaded, image);
				}
			});
			return ticket;
		}
		const auto key      = aRequest.Key;
		auto [found, added] = lane.Loads.try_emplace(key);
		if (added) {
			found->second.Request = std::move(aRequest);
			lane.Queue.push_back(key);
		}
		found->second.Subscribers.emplace(ticket, std::move(aSubscriber));
		lane.Tickets.emplace(ticket, key);
		lane.pump();
		return ticket;
	}

	void markdown_image_service::cancel(const std::uint64_t aTicket)
	{
		auto&      state = *State;
		const auto entry = state.Tickets.find(aTicket);
		if (entry == state.Tickets.end()) {
			return;
		}
		const auto key = entry->second;
		state.Tickets.erase(entry);
		const auto found = state.Loads.find(key);
		if (found == state.Loads.end()) {
			return;
		}
		found->second.Subscribers.erase(aTicket);
		if (found->second.Subscribers.empty()) {
			found->second.Stop.request_stop();
			state.Loads.erase(found);
			std::erase(state.Queue, key);
		}
	}

	void markdown_image_service::shutdown()
	{
		auto& state = *State;
		if (state.Closed) {
			return;
		}
		state.Closed = true;
		for (auto& [key, load] : state.Loads) {
			load.Stop.request_stop();
		}
		state.Loads.clear();
		state.Tickets.clear();
		state.Queue.clear();
	}

	auto markdown_image_service::quiescent() const noexcept -> bool
	{
		return State->Active == 0U && State->Decoding == 0U;
	}

	auto markdown_image_service::cached_bytes() const noexcept -> std::size_t
	{
		return State->CacheBytes;
	}

	auto markdown_image_service::active_fetches() const noexcept -> std::size_t
	{
		return State->Active;
	}
} // namespace bpr::application
