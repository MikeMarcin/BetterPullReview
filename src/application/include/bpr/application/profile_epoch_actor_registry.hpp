#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_EPOCH_ACTOR_REGISTRY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_EPOCH_ACTOR_REGISTRY_HPP

#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/profile_window.hpp"

#include <atomic>
#include <expected>
#include <functional>
#include <memory>

namespace bpr::application {
	/// \brief Decision-lane admission to exact live profile windows.
	/// \details The referenced port outlives the registry and all consumers.
	class epoch_window_context_port
	{
	public:
		virtual ~epoch_window_context_port()                                                         = default;
		[[nodiscard]] virtual auto current(core::bpr_window_context aContext) const noexcept -> bool = 0;
	};

	/// \brief Physical cache authority, independent of a window's application session.
	struct profile_epoch_actor_key
	{
		core::host_profile_id            Profile;
		core::github_profile_revision    ProfileRevision;
		core::git_host_id                Host;
		core::host_authority_fingerprint HostAuthority;
		core::github_account_id          Account;
		core::repository_id              Repository;
		core::git_hash_algorithm         ObjectFormat = core::git_hash_algorithm::invalid;
		core::github_access_mode         Mode{ };
		auto                             operator<=>(const profile_epoch_actor_key&) const = default;
	};

	enum class profile_epoch_actor_failure : std::uint8_t {
		invalid_authority,
		stale_window,
		closing,
		consumer_limit,
		actor_limit,
		actor_draining,
		already_attached
	};

	/// \brief [[BPR-1344]]. Whose work an actor runs: the reviewer's before any
	/// queued preparation, so preparing files the reviewer may open never
	/// delays one they did open.
	enum class profile_epoch_actor_priority : std::uint8_t {
		reviewer,
		preparation,
	};

	/// \brief Retained FIFOs and their single lifetime-exclusive cache session.
	/// \details One FIFO per priority; preparation's runs only while the
	/// reviewer's is empty ([[BPR-1344]]). Cache access is permitted only inside
	/// enqueue callbacks. Work owns its typed failure boundary. The executor
	/// must outlive registry quiescence.
	class profile_epoch_actor final : public std::enable_shared_from_this<profile_epoch_actor>
	{
	public:
		~profile_epoch_actor();
		void               enqueue(std::function<void()> aWork, profile_epoch_actor_priority aPriority = profile_epoch_actor_priority::reviewer);
		[[nodiscard]] auto cache() noexcept -> std::unique_ptr<git_cache_session>&;

	private:
		friend class profile_epoch_actor_consumer;
		profile_epoch_actor(effect_executor& aEffects, std::shared_ptr<std::atomic_bool> aReleased);
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};

	struct profile_epoch_registry_state;

	/// \brief Window-local admission gate; retiring it never cancels another window.
	class profile_epoch_actor_consumer final
	{
	public:
		~profile_epoch_actor_consumer();
		/// \brief Decision-lane check against the live catalog and retirement gate.
		[[nodiscard]] auto current() const noexcept -> bool;
		/// \brief Concurrent check; composition retires before closing its catalog context.
		[[nodiscard]] auto active() const noexcept -> bool;
		void               retire() noexcept;
		[[nodiscard]] auto acquire(const profile_epoch_actor_key& aKey)
			-> std::expected<std::shared_ptr<profile_epoch_actor>, profile_epoch_actor_failure>;

	private:
		friend class profile_epoch_actor_registry;
		profile_epoch_actor_consumer(std::shared_ptr<profile_epoch_registry_state> aState,
									 core::bpr_window_context                      aContext);
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};

	/// \brief Profile owner for bounded weak actor entries and exact window consumers.
	/// \details Attach/current are decision-lane operations. FIFO work and retirement
	/// gates are synchronized separately. Shutdown closes admission without waiting;
	/// hosts must also shut down and release their actor leases. Retired consumer
	/// handles do not prevent quiescence. No persisted identity is minted here.
	class profile_epoch_actor_registry final
	{
	public:
		profile_epoch_actor_registry(core::bpr_profile_id       aProfile,
									 epoch_window_context_port& aWindows, git_cache_port& aCaches, effect_executor& aEffects);
		~profile_epoch_actor_registry();
		profile_epoch_actor_registry(const profile_epoch_actor_registry&)                                  = delete;
		auto               operator=(const profile_epoch_actor_registry&) -> profile_epoch_actor_registry& = delete;
		[[nodiscard]] auto attach(core::bpr_window_context aContext)
			-> std::expected<std::shared_ptr<profile_epoch_actor_consumer>, profile_epoch_actor_failure>;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		friend class epoch_application_host;
		profile_epoch_actor_registry(git_cache_port& aCaches, effect_executor& aEffects);
		[[nodiscard]] auto                            caches() noexcept -> git_cache_port&;
		[[nodiscard]] auto                            attach_legacy() -> std::shared_ptr<profile_epoch_actor_consumer>;
		std::shared_ptr<profile_epoch_registry_state> State;
	};
}

#endif
