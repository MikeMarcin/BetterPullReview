#ifndef BPR_APPLICATION_PULL_REQUEST_ACTIVITY_HOST_HPP
#define BPR_APPLICATION_PULL_REQUEST_ACTIVITY_HOST_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/pull_request_activity.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1850]]. Polls the signed-in account's GitHub notification
/// threads and posts a desktop notification for activity on my pull requests.
/// \details One host per profile, whatever the number of windows, with one
/// timer. Each poll is one conditional `GET /notifications`; GitHub answers
/// 304 while nothing changed, which costs nothing against the rate limit. The
/// core decides what is new and what is said; the host only performs the poll,
/// the store's writes and the posting, and never marks anything read on GitHub.
namespace bpr::application {
	/// \brief One poll of the notification list.
	struct pull_request_activity_request
	{
		/// The accepted account session the poll runs under.
		personal_discovery_session Session;
		/// This poll's identity; its other fields are the session's.
		core::github_session_request_context Context;
		/// The previous answer's `Last-Modified`; empty for none.
		std::string IfModifiedSince;

		auto operator<=>(const pull_request_activity_request&) const = default;
	};

	struct pull_request_activity_answer
	{
		core::activity_poll_answer         Poll;
		std::string                        LastModified;
		std::vector<core::activity_thread> Threads;
		bool                               Complete = false;
		/// Wire requests the poll sent: zero when the sign-in cannot poll.
		std::uint32_t Requests = 0U;

		auto operator<=>(const pull_request_activity_answer&) const = default;
	};

	using pull_request_activity_completion = std::function<void(pull_request_activity_answer)>;

	/// \brief The provider's notification read.
	/// \details A submission that returns normally invokes its completion
	/// exactly once, possibly before returning, on any thread.
	class pull_request_activity_port
	{
	public:
		virtual ~pull_request_activity_port()                                                                            = default;
		virtual void poll_activity(pull_request_activity_request aRequest, pull_request_activity_completion aCompletion) = 0;
	};

	/// \brief The memory's store.
	class pull_request_activity_store_port
	{
	public:
		virtual ~pull_request_activity_store_port() = default;
		/// \brief Writes the whole memory. False when it could not.
		[[nodiscard]] virtual auto store(const core::activity_memory& aMemory) -> bool = 0;
	};

	/// \brief The one timer: a single shot, rearmed after each poll.
	class pull_request_activity_timer_port
	{
	public:
		virtual ~pull_request_activity_timer_port()   = default;
		virtual void schedule(std::uint32_t aSeconds) = 0;
		virtual void cancel()                         = 0;
	};

	/// \brief Where notifications go: the desktop's own surface.
	class pull_request_activity_sink_port
	{
	public:
		virtual ~pull_request_activity_sink_port() = default;
		/// \brief Posts one notification. False when the platform has no surface.
		[[nodiscard]] virtual auto post(const std::string& aTitle, const std::string& aBody) -> bool = 0;
	};

	/// \brief What the Settings line says.
	enum class pull_request_activity_state : std::uint8_t {
		/// The setting is off.
		off,
		/// Waiting for an account session.
		signed_out,
		/// GitHub refuses this sign-in its notification list; nothing is polled.
		unsupported,
		/// Polling.
		watching,
		/// Waiting out a rate limit or failures.
		waiting,
	};

	struct pull_request_activity_statistics
	{
		std::uint64_t Polls       = 0U;
		std::uint64_t Requests    = 0U;
		std::uint64_t Unchanged   = 0U;
		std::uint64_t Changed     = 0U;
		std::uint64_t Failed      = 0U;
		std::uint64_t RateLimited = 0U;
		std::uint64_t Posted      = 0U;
		std::uint64_t Suppressed  = 0U;
		std::uint64_t Stale       = 0U;

		auto operator<=>(const pull_request_activity_statistics&) const = default;
	};

	/// \brief The host's clock, whole seconds since the epoch.
	using pull_request_activity_clock = std::function<std::int64_t()>;
	/// \brief The pull requests open and focused in a window now.
	using pull_request_activity_focus = std::function<std::vector<core::pull_request_ref>()>;

	class pull_request_activity_host final
	{
	public:
		pull_request_activity_host(pull_request_activity_port&          aProvider,
								   pull_request_activity_store_port&    aStore,
								   effect_executor&                     aEffects,
								   decision_executor&                   aDecisions,
								   pull_request_activity_timer_port&    aTimer,
								   pull_request_activity_sink_port&     aSink,
								   pull_request_activity_clock          aClock,
								   pull_request_activity_focus          aFocus,
								   core::activity_memory_read           aLoaded,
								   std::shared_ptr<diagnostic_log_port> aLog = { });
		~pull_request_activity_host();

		pull_request_activity_host(const pull_request_activity_host&)                    = delete;
		auto operator=(const pull_request_activity_host&) -> pull_request_activity_host& = delete;
		pull_request_activity_host(pull_request_activity_host&&)                         = delete;
		auto operator=(pull_request_activity_host&&) -> pull_request_activity_host&      = delete;

		/// \brief Adopts the profile's account session, or none (signed out).
		/// \details A new session polls at once; the same again changes nothing;
		/// none cancels the timer and stops polling.
		void adopt(std::optional<personal_discovery_session> aSession);
		/// \brief The setting. Off cancels the timer; on polls at once, and the
		/// first answer only takes a baseline.
		void               set_enabled(bool aEnabled);
		[[nodiscard]] auto enabled() const noexcept -> bool;
		/// \brief The timer fired.
		void tick();
		/// \brief Called on the decision lane after each answered poll, so the
		/// Settings line can follow `state()`.
		void set_observer(std::function<void()> aObserver);
		/// \brief A notification was clicked: where it goes.
		[[nodiscard]] auto activate() -> core::activity_target;

		void               shutdown() noexcept;
		[[nodiscard]] auto state() const noexcept -> pull_request_activity_state;
		[[nodiscard]] auto statistics() const noexcept -> pull_request_activity_statistics;
		[[nodiscard]] auto memory() const noexcept -> const core::activity_memory&;
		/// \brief No poll and no write running or waiting.
		[[nodiscard]] auto quiescent() const noexcept -> bool;
		/// \brief Seconds the timer was last set to; none when not set.
		[[nodiscard]] auto scheduled() const noexcept -> std::optional<std::uint32_t>;

	private:
		struct implementation;
		std::shared_ptr<implementation> Impl;
	};
} // namespace bpr::application

#endif
