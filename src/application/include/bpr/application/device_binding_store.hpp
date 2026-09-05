#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_BINDING_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_BINDING_STORE_HPP

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <variant>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t maximum_device_bindings = 64U;

	/// \brief Wall-clock facts retained across lifetimes; active timers remain monotonic.
	struct device_binding_lifetime
	{
		std::int64_t IssuedAt                                          = 0;
		std::int64_t AccessExpiresAt                                   = 0;
		std::int64_t RefreshExpiresAt                                  = 0;
		auto         operator<=>(const device_binding_lifetime&) const = default;
	};

	/// \brief Exact device-grant provenance and lifetime, with no credential material.
	struct device_binding_record
	{
		github_credential_authority_key Authority;
		device_binding_lifetime         Lifetime;
		bool                            Tombstone                                       = false;
		auto                            operator<=>(const device_binding_record&) const = default;
	};

	/// \brief Checks bounded issuance and expiry facts without reading a clock.
	[[nodiscard]] auto valid(const device_binding_lifetime& aLifetime) noexcept -> bool;
	/// \brief Checks exact authority and the stored lifetime envelope.
	[[nodiscard]] auto valid(const device_binding_record& aRecord) noexcept -> bool;
	/// \brief Returns conservative remaining seconds; backward or expired clocks yield zero.
	[[nodiscard]] auto remaining_access_lifetime(const device_binding_lifetime& aLifetime, std::int64_t aNow) noexcept -> std::uint32_t;

	enum class device_binding_failure { unavailable,
										corrupt,
										capacity,
										cancelled };
	using device_binding_result      = std::expected<void, device_binding_failure>;
	using device_binding_load_result = std::expected<std::vector<device_binding_record>, device_binding_failure>;

	/// \brief Synchronous metadata effects, called only on the host's serialized effect lane.
	class device_binding_store_port
	{
	public:
		virtual ~device_binding_store_port()                                                                     = default;
		virtual auto load(std::stop_token aStop) -> device_binding_load_result                                   = 0;
		virtual auto store(const device_binding_record& aRecord, std::stop_token aStop) -> device_binding_result = 0;
		/// \brief Durably tombstones only the exact authority; absence succeeds.
		virtual auto erase(const github_credential_authority_key& aAuthority, std::stop_token aStop) -> device_binding_result = 0;
	};

	/// \brief Serializes bounded metadata effects and returns their terminals to the decision lane.
	/// \details Keep all ports alive until quiescent. Calls and callbacks are decision-lane confined.
	class device_binding_persistence final
	{
	public:
		device_binding_persistence(device_binding_store_port& aStore, effect_executor& aEffects, decision_executor& aDecisions);
		~device_binding_persistence();
		device_binding_persistence(const device_binding_persistence&)                    = delete;
		auto operator=(const device_binding_persistence&) -> device_binding_persistence& = delete;
		device_binding_persistence(device_binding_persistence&&)                         = delete;
		auto operator=(device_binding_persistence&&) -> device_binding_persistence&      = delete;
		void load(std::function<void(device_binding_load_result)> aCompletion);
		void store(device_binding_record aRecord, std::function<void(device_binding_result)> aCompletion);
		void erase(github_credential_authority_key aAuthority, std::function<void(device_binding_result)> aCompletion);
		/// \brief Cancels ordinary pending work; exact tombstones remain admitted during shutdown.
		void               cancel_pending();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
