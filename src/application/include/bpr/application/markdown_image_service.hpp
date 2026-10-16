#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_IMAGE_SERVICE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_IMAGE_SERVICE_HPP

#include "bpr/application/markdown_image_ports.hpp"
#include "bpr/application/port.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace bpr::application {
	/// \brief RFC-BPR-0084. A profile's image lane: one bounded fetch queue, decoding
	/// off the decision lane, and the decoded in-memory cache shared by its windows.
	///
	/// \details Every member function runs on the decision lane. Fetches run at
	/// most `core::markdown_image_fetch_concurrency` at a time; requests for the
	/// same key share one fetch. Fetch and decode completions return to the
	/// decision lane as posted tasks, and a load whose subscribers have all
	/// cancelled stops its fetch and delivers nothing. The cache is keyed by
	/// `core::markdown_image_cache_key`, holds at most its byte bound of decoded
	/// pixels, and evicts the least recently used image first.
	class markdown_image_service
	{
	public:
		/// \brief Receives the terminal state of one load, once, on the decision lane.
		using subscriber = std::function<void(core::markdown_image_state, std::shared_ptr<const markdown_decoded_image>)>;

		struct load_request
		{
			std::string                  Key;
			markdown_image_fetch_request Fetch;
		};

		markdown_image_service(decision_executor& aDecision, effect_executor& aDecode, markdown_image_fetch_port* aFetch,
							   const markdown_image_decoder_port* aDecoder,
							   std::size_t                        aCacheBytes = core::markdown_image_memory_cache_bytes);
		~markdown_image_service();
		markdown_image_service(const markdown_image_service&)                    = delete;
		auto operator=(const markdown_image_service&) -> markdown_image_service& = delete;

		/// \brief Whether images can be fetched and decoded at all.
		[[nodiscard]] auto available() const noexcept -> bool;
		/// \brief The cached image for `aKey`, marked most recently used; null on a miss.
		[[nodiscard]] auto cached(const std::string& aKey) -> std::shared_ptr<const markdown_decoded_image>;
		/// \brief Starts or joins the load of `aRequest.Key`; returns a ticket for `cancel`.
		/// \details A cache hit delivers through a posted task, never inline.
		[[nodiscard]] auto load(load_request aRequest, subscriber aSubscriber) -> std::uint64_t;
		/// \brief Drops one subscriber; the last one stops its fetch.
		void cancel(std::uint64_t aTicket);
		/// \brief Stops all loads; no subscriber is called afterwards.
		void shutdown();
		/// \brief No fetch or decode is still running.
		[[nodiscard]] auto quiescent() const noexcept -> bool;
		[[nodiscard]] auto cached_bytes() const noexcept -> std::size_t;
		[[nodiscard]] auto active_fetches() const noexcept -> std::size_t;

	private:
		struct state;
		std::shared_ptr<state> State;
	};
} // namespace bpr::application

#endif
