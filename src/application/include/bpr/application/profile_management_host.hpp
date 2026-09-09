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

	/// \brief Durable profile management with bounded live window reservations per profile.
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
		/// \brief Reports refused pre-runtime admission without reserving or changing windows.
		void report_runtime_admission_failure(core::bpr_profile_id aProfile, profile_management_failure aFailure);
		/// \brief Accepts one startup completion; false means composition must dispose any late runtime.
		[[nodiscard]] auto complete_open(core::bpr_window_context                        aContext,
										 std::expected<void, profile_management_failure> aResult = { }) -> bool;
		/// \brief Queues durable close admission; only the eventual close command authorizes runtime teardown.
		[[nodiscard]] auto begin_close(core::bpr_window_context aContext) -> bool;
		[[nodiscard]] auto complete_close(core::bpr_window_context aContext) -> bool;
		/// \brief Admits only exact opening/open contexts; closing and retired windows are stale.
		[[nodiscard]] auto current(core::bpr_window_context aContext) const noexcept -> bool;
		/// \brief Complete accepted live set, including startup contexts before commands are delivered.
		[[nodiscard]] auto live_windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context>;
		/// \brief Records native focus for an exact open window, preserving core MRU order.
		[[nodiscard]] auto focus(core::bpr_window_context aContext) -> bool;

		/// \brief Saves the retained set before emitting an orderly quit command. Busy/failure leaves live windows usable.
		[[nodiscard]] auto request_quit() -> bool;
		/// \brief Forced lifetime cleanup; never substitutes for successful orderly Quit persistence.
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
