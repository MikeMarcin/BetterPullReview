#ifndef BPR_APPLICATION_RELEASE_MAINTENANCE_APPLICATION_HOST_HPP
#define BPR_APPLICATION_RELEASE_MAINTENANCE_APPLICATION_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/release_maintenance.hpp"

#include <expected>
#include <memory>
#include <stop_token>
#include <string>

namespace bpr::application {
	using release_maintenance_result = std::expected<void, core::release_maintenance_reason>;

	/// \brief Immutable preparation minted by the effect owner, never constructed from Qt data.
	class release_maintenance_preparation
	{
	public:
		virtual ~release_maintenance_preparation()                                          = default;
		[[nodiscard]] virtual auto selection() const -> core::release_maintenance_selection = 0;
		/// \brief The exact metadata-only bytes used by preview and export; empty for deletion.
		[[nodiscard]] virtual auto preview() const noexcept -> std::string_view = 0;
	};

	/// \brief One independently admitted local save destination; implementations never upload.
	class release_diagnostic_output
	{
	public:
		virtual ~release_diagnostic_output() = default;
		/// \brief Writes the immutable snapshot atomically, refusing changed destination authority.
		virtual auto write(std::string_view aBytes, std::stop_token aStop,
						   const std::function<bool()>& aBeginCommit) -> release_maintenance_result = 0;
	};

	/// \brief Effect-lane private-root owner, retained through all completions and recovery.
	class release_maintenance_storage_port
	{
	public:
		virtual ~release_maintenance_storage_port()                                                        = default;
		[[nodiscard]] virtual auto authority() const noexcept -> core::release_maintenance_authority       = 0;
		[[nodiscard]] virtual auto capabilities() const noexcept -> core::release_maintenance_capabilities = 0;
		virtual auto               prepare(const core::release_maintenance_context& aContext, std::stop_token aStop)
			-> std::expected<std::shared_ptr<const release_maintenance_preparation>, core::release_maintenance_reason> = 0;
		/// \brief Revalidates retained native authority before using a frozen preparation.
		virtual auto revalidate(const core::release_maintenance_context& aContext,
								const release_maintenance_preparation&   aPreparation) -> release_maintenance_result = 0;
		/// \brief Revalidates the exact retained inventory, journals before deletion, and preserves recovery on failure.
		virtual auto erase(const core::release_maintenance_context& aContext,
						   const release_maintenance_preparation& aPreparation, std::stop_token aStop,
						   const std::function<bool()>& aBeginCommit) -> release_maintenance_result = 0;
	};

	/// \brief Decision-lane barrier before a destructive inventory is captured.
	/// \details Close ingress and invalidate/drain affected producers/windows, then close stores/logger.
	/// Completion returns once on any lane, including cancellation. The retained root lease survives.
	/// A cancelled preparation may require reopening the profile; it never silently restarts producers.
	class release_maintenance_lifecycle_port
	{
	public:
		virtual ~release_maintenance_lifecycle_port()                                     = default;
		virtual void quiesce(const core::release_maintenance_context& aContext, std::stop_token aStop,
							 std::function<void(release_maintenance_result)> aCompletion) = 0;
	};

	/// \brief Bounded provider-free state for maintenance controls, independent of sign-in.
	struct release_maintenance_projection
	{
		core::release_maintenance_state                             State;
		std::array<core::release_maintenance_command_projection, 7> Commands;
		std::shared_ptr<const std::string>                          Preview;
		core::release_maintenance_output                            Delta;
	};
	class release_maintenance_projection_port
	{
	public:
		virtual ~release_maintenance_projection_port()                          = default;
		virtual void publish(const release_maintenance_projection& aProjection) = 0;
	};

	/// \brief Serializes maintenance decisions and retains at most one physical job and snapshot.
	/// \details All public calls run on the decision lane. Effects never mutate core state.
	/// Keep executors/ports alive through shutdown and is_quiescent(); projection exceptions and
	/// reentrant intents cannot abandon an owed completion or overtake a physical operation.
	class release_maintenance_application_host final
	{
	public:
		release_maintenance_application_host(release_maintenance_storage_port&   aStorage,
											 release_maintenance_lifecycle_port& aLifecycle, effect_executor& aEffects,
											 decision_executor& aDecisions, release_maintenance_projection_port& aProjection);
		~release_maintenance_application_host();
		release_maintenance_application_host(const release_maintenance_application_host&)                                  = delete;
		auto               operator=(const release_maintenance_application_host&) -> release_maintenance_application_host& = delete;
		void               dispatch(core::release_maintenance_intent aIntent, std::shared_ptr<release_diagnostic_output> aOutput = { });
		void               invalidate();
		void               shutdown();
		[[nodiscard]] auto state() const noexcept -> const core::release_maintenance_state&;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
