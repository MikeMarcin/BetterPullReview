#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_MANAGEMENT_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_MANAGEMENT_HOST_HPP

#include "bpr/application/profile_management_projection.hpp"
#include "bpr/application/profile_registry.hpp"

namespace bpr::application {
	/// \brief Decision-lane projection and composition commands, with no Qt or storage effects.
	class profile_management_projection_port
	{
	public:
		virtual ~profile_management_projection_port()                          = default;
		virtual void publish(const profile_management_projection& aProjection) = 0;
		/// \brief Accepts a command exactly once; exceptions must occur before retaining it.
		virtual void perform(const profile_runtime_command& aCommand) = 0;
	};

	/// \brief Durable management with one in-process runtime reservation per profile.
	/// \details All methods and callbacks run on the decision lane. Runtime commands are
	/// performed by composition, which must report exact completions. Closing reservations
	/// remain occupied until complete_close. Shutdown cancels registry work and marks every
	/// reservation closing; quiescence requires registry terminals and runtime closes drained.
	class profile_management_host final
	{
	public:
		profile_management_host(profile_registry_port& aRegistry, effect_executor& aEffects,
								decision_executor& aDecision, profile_management_projection_port& aProjection,
								core::application_session_id aSession);
		~profile_management_host();
		profile_management_host(const profile_management_host&)                    = delete;
		auto operator=(const profile_management_host&) -> profile_management_host& = delete;
		void apply(profile_management_intent aIntent);
		void reload();
		/// \brief Accepts one startup completion; false means composition must dispose any late runtime.
		[[nodiscard]] auto complete_open(core::bpr_window_context                        aContext,
										 std::expected<void, profile_management_failure> aResult = { }) -> bool;
		[[nodiscard]] auto begin_close(core::bpr_window_context aContext) -> bool;
		[[nodiscard]] auto complete_close(core::bpr_window_context aContext) -> bool;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
