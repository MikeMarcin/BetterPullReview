#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_STARTUP_ADMISSION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_STARTUP_ADMISSION_HPP

#include "bpr/application/profile_storage_authority.hpp"

#include <filesystem>
#include <memory>
#include <stop_token>

namespace bpr::application {
	enum class profile_startup_failure : std::uint8_t {
		unknown_profile,
		invalid_registry,
		registry_unavailable,
		root_unavailable,
		owner_busy,
		cancelled,
		busy,
		closing,
		identity_exhausted
	};
	template <typename TValue>
	using profile_startup_result = std::expected<TValue, profile_startup_failure>;

	/// \brief Exact startup identity carried through both effect phases and the terminal.
	struct profile_startup_context
	{
		core::application_session_id Session;
		core::request_id             Request;
		core::bpr_profile_id         Profile;
		auto                         operator==(const profile_startup_context&) const -> bool = default;
	};

	/// \brief A durable profile and resolved private root, ready for runtime construction.
	struct profile_startup_admission
	{
		profile_storage_authority Authority;
		std::string               Label;
		std::filesystem::path     StorageRoot;
	};

	/// \brief Performs concrete private-root preparation on the effect executor.
	class profile_startup_root_port
	{
	public:
		virtual ~profile_startup_root_port() = default;
		[[nodiscard]] virtual auto resolve(const profile_storage_authority& aAuthority, std::stop_token aStop)
			-> profile_startup_result<std::filesystem::path> = 0;
	};

	struct profile_startup_terminal
	{
		profile_startup_context                           Context;
		profile_startup_result<profile_startup_admission> Result;
	};

	class profile_startup_projection_port
	{
	public:
		virtual ~profile_startup_projection_port()                      = default;
		virtual void publish(const profile_startup_terminal& aTerminal) = 0;
	};

	/// \brief Admits one exact durable profile before any private runtime is constructed.
	/// \details Public methods run on the decision lane. Registry load and root resolution
	/// run as separate effects, with selection between them on the decision lane. One
	/// pending startup is allowed; rejected starts enqueue no work. Explicit cancellation
	/// or shutdown still drains one cancelled terminal. Ports/executors must outlive
	/// quiescence; destruction detaches projection but does not destroy queued effects.
	class profile_startup_host final
	{
	public:
		profile_startup_host(profile_registry_port& aRegistry, profile_startup_root_port& aRoots,
							 effect_executor& aEffects, decision_executor& aDecision,
							 profile_startup_projection_port& aProjection, core::application_session_id aSession);
		~profile_startup_host();
		profile_startup_host(const profile_startup_host&)                                  = delete;
		auto               operator=(const profile_startup_host&) -> profile_startup_host& = delete;
		[[nodiscard]] auto start(core::bpr_profile_id aProfile) -> profile_startup_result<core::request_id>;
		void               cancel();
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
