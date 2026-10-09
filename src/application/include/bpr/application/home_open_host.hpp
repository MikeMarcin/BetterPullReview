#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOME_OPEN_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOME_OPEN_HOST_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/home_open.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace bpr::application {
	/// \brief [[BPR-1357]]. The application's clock for one bounded wait: call
	/// `aElapsed` once, on the decision lane, after `aMilliseconds`.
	using home_open_wait = std::function<void(std::uint32_t aMilliseconds, std::function<void()> aElapsed)>;

	/// \brief [[BPR-1357]]. Follows the pull request one window's Home asked to
	/// open, and owns the clock that decides when it has taken long enough to say
	/// which step it is on.
	///
	/// \details Every transition of the window's read lane is observed in the
	/// turn it was published, so a gesture's opening is published before control
	/// returns to the event loop. The core decides each change; this host only
	/// arms the wait for a new read, delivers the core its elapse, and publishes
	/// when what Home shows has changed. A wait that elapses for a read the core
	/// no longer shows changes nothing, and none outlives the host.
	class home_open_host
	{
	public:
		using publication = std::function<void(const home_open_projection&)>;

		home_open_host(home_open_wait aWait, publication aPublication);
		home_open_host(const home_open_host&)                    = delete;
		auto operator=(const home_open_host&) -> home_open_host& = delete;
		home_open_host(home_open_host&&)                         = delete;
		auto operator=(home_open_host&&) -> home_open_host&      = delete;
		~home_open_host();

		/// \brief One transition of the read lane, then the lane as it now stands.
		void observe(const core::github_transition_output& aTransition, const core::github_state& aLane);
		/// \brief The lane as it now stands, after a change with no transition.
		void reconcile(const core::github_state& aLane);
		/// \brief Whether a Cancel naming this read is for what Home shows and
		/// the lane still holds it.
		[[nodiscard]] auto admit_cancel(core::request_id aRequest, const core::github_state& aLane) const -> bool;
		/// \brief The pull request a Retry naming this read asks for again, when
		/// Home shows that read's failure and the core offers Retry.
		[[nodiscard]] auto admit_retry(core::request_id aRequest) const -> std::optional<core::open_pull_request_intent>;

		[[nodiscard]] auto state() const noexcept -> const std::optional<core::home_open_state>&;
		[[nodiscard]] auto projection() const -> home_open_projection;
		/// \brief Waits armed, for evidence; one per read that started.
		[[nodiscard]] auto waits_armed() const noexcept -> std::uint64_t;

	private:
		void adopt(std::optional<core::home_open_state> aNext);
		void elapsed(core::request_id aRequest);

		home_open_wait                       Wait;
		publication                          Publication;
		std::optional<core::home_open_state> State;
		std::shared_ptr<int>                 Lifetime;
		std::uint64_t                        WaitsArmed = 0U;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOME_OPEN_HOST_HPP
