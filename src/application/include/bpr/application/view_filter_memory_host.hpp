#ifndef BPR_APPLICATION_VIEW_FILTER_MEMORY_HOST_HPP
#define BPR_APPLICATION_VIEW_FILTER_MEMORY_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/view_filter_memory.hpp"

#include <memory>

/// \file
/// \brief [[BPR-1844]]. The profile's remembered Discuss, Threads and
/// Review Comments filters: read once when the profile starts, written off
/// the decision lane after each reviewer's choice.
namespace bpr::application {
	/// \brief The store the memory is read from and written to.
	class view_filter_memory_port
	{
	public:
		virtual ~view_filter_memory_port() = default;

		/// \brief Writes the whole memory. False when it could not.
		[[nodiscard]] virtual auto store(const core::view_filter_defaults& aDefaults) -> bool = 0;
	};

	/// \brief Holds one profile's memory for every window.
	/// \details Each window asks `defaults()` when it opens a pull request and
	/// hands its reviewer's choices to `remember`. The memory changes at once,
	/// so the next window opens with it; nothing is pushed to windows already
	/// open. Writes are serialized on the effect lane, one at a time, each
	/// carrying the latest memory, so a burst of choices costs at most one
	/// write behind the one running. A store that cannot be written, or that a
	/// newer build wrote, keeps the memory for this session only.
	class view_filter_memory_host final
	{
	public:
		view_filter_memory_host(view_filter_memory_port& aStore, effect_executor& aEffects, decision_executor& aDecisions, core::view_filter_memory_read aLoaded);
		~view_filter_memory_host();

		view_filter_memory_host(const view_filter_memory_host&)                    = delete;
		auto operator=(const view_filter_memory_host&) -> view_filter_memory_host& = delete;
		view_filter_memory_host(view_filter_memory_host&&)                         = delete;
		auto operator=(view_filter_memory_host&&) -> view_filter_memory_host&      = delete;

		[[nodiscard]] auto defaults() const noexcept -> const core::view_filter_defaults&;
		void               remember(const core::view_filter_remember& aChoice);
		/// \brief Whether the stored row was damaged when it was read.
		[[nodiscard]] auto recovered() const noexcept -> bool;
		/// \brief Whether the last write that finished succeeded, or none failed.
		[[nodiscard]] auto durable() const noexcept -> bool;
		/// \brief No write running or waiting.
		[[nodiscard]] auto quiescent() const noexcept -> bool;
		/// \brief Takes no more choices. A choice waiting behind a running
		/// write is posted at once, so draining the effect lane writes it.
		void shutdown() noexcept;

	private:
		struct implementation;
		std::shared_ptr<implementation> Impl;
	};
}

#endif
