#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP

#include "bpr/core/workbench_interaction.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t   maximum_workbench_actionable_files     = 262'144U;
	inline constexpr std::size_t   maximum_workbench_outstanding_requests = 64U;
	inline constexpr std::uint32_t minimum_workbench_drawer_width         = 64U;
	inline constexpr std::uint32_t maximum_workbench_drawer_width         = 4'096U;

	/// \brief Describes the workbench host limits record.
	///
	/// \details `ActionableFiles` bounds the ordered actionable set the host will
	/// retain for one workbench, `OutstandingRequests` bounds concurrently
	/// submitted effect work, and `DrawerWidth` is the largest drawer width the
	/// host will retain or persist.
	struct workbench_host_limits
	{
		std::size_t   ActionableFiles     = 4'096U;
		std::size_t   OutstandingRequests = 8U;
		std::uint32_t DrawerWidth         = 1'024U;

		auto operator<=>(const workbench_host_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const workbench_host_limits& aLimits) noexcept -> bool
	{
		return aLimits.ActionableFiles > 0U
			&& aLimits.ActionableFiles <= maximum_workbench_actionable_files
			&& aLimits.OutstandingRequests > 0U
			&& aLimits.OutstandingRequests <= maximum_workbench_outstanding_requests
			&& aLimits.DrawerWidth >= minimum_workbench_drawer_width
			&& aLimits.DrawerWidth <= maximum_workbench_drawer_width;
	}

	/// \brief Describes the workbench request context record.
	///
	/// \details `Request` is unique for the host lifetime and is the exactly-once
	/// terminal key: a second delivery of the same request finds no outstanding
	/// record. `Session` names the host that issued the request, so a terminal
	/// from a foreign host cannot be mistaken for a local one. `Generation` names
	/// the retained workbench incarnation the request was issued under; it
	/// advances whenever the host adopts a new observation or abandons its
	/// outstanding work. A locally decided change carries a zero `Request`,
	/// because no effect produced it.
	struct workbench_request_context
	{
		core::request_id             Request;
		core::application_session_id Session;
		core::request_generation     Generation;

		auto operator<=>(const workbench_request_context&) const = default;
	};

	/// \brief Describes the workbench observation request record.
	///
	/// \details `MaximumFiles` is the bound the host will accept. A port that
	/// cannot answer within it must report `limit_exceeded` rather than truncate,
	/// because a truncated order would give the core false neighbour facts.
	struct workbench_observation_request
	{
		workbench_request_context    Context;
		core::pull_request_authority Authority;
		std::size_t                  MaximumFiles = 0U;

		auto operator<=>(const workbench_observation_request&) const = default;
	};

	enum class workbench_observation_status : std::uint8_t {
		available,
		cancelled,
		unavailable,
		limit_exceeded,
	};

	/// \brief Describes the workbench file observation record.
	///
	/// \details `Actionable` is the filtered actionable order in presentation
	/// order. The host owns it; the core never sees it and receives only the
	/// bounded cursor facts derived from it. `Context` and `Authority` must repeat
	/// the request exactly, so a misrouted answer cannot be adopted. The port owns
	/// uniqueness within `Actionable`; the host resolves the first matching entry.
	struct workbench_file_observation
	{
		workbench_request_context            Context;
		core::pull_request_authority         Authority;
		workbench_observation_status         Status = workbench_observation_status::unavailable;
		std::vector<core::workbench_file_id> Actionable;

		auto operator<=>(const workbench_file_observation&) const = default;
	};

	/// \brief Provides the workbench observation port contract.
	///
	/// \details The host calls this on the effect executor, never on the decision
	/// lane. An implementation must honour the supplied stop token and must repeat
	/// the request context and authority in its answer.
	class workbench_observation_port
	{
	public:
		virtual ~workbench_observation_port() = default;

		[[nodiscard]] virtual auto assemble(
			const workbench_observation_request& aRequest,
			std::stop_token                      aStop)
			-> workbench_file_observation = 0;
	};

	/// \brief Describes the workbench layout settings record.
	///
	/// \details This is the whole durable workbench layout: the semantic drawer
	/// states the core owns while a workbench is active, and the drawer widths the
	/// host owns outright because they carry no semantic meaning.
	struct workbench_layout_settings
	{
		core::workbench_drawer_state Files              = core::workbench_drawer_state::expanded;
		core::workbench_drawer_state Conversations      = core::workbench_drawer_state::expanded;
		std::uint32_t                FilesWidth         = 320U;
		std::uint32_t                ConversationsWidth = 320U;

		auto operator<=>(const workbench_layout_settings&) const = default;
	};

	enum class workbench_settings_failure : std::uint8_t {
		unavailable,
		cancelled,
	};

	/// \brief Describes the workbench settings error record.
	struct workbench_settings_error
	{
		workbench_settings_failure Failure = workbench_settings_failure::unavailable;

		auto operator<=>(const workbench_settings_error&) const = default;
	};

	template <typename Value>
	using workbench_settings_result = std::expected<Value, workbench_settings_error>;

	/// \brief Provides the workbench settings port contract.
	///
	/// \details The host calls this on the effect executor, never on the decision
	/// lane, and marshals the answer back as a typed terminal. An absent durable
	/// record is not a failure: an implementation with nothing stored returns the
	/// default settings.
	class workbench_settings_port
	{
	public:
		virtual ~workbench_settings_port() = default;

		[[nodiscard]] virtual auto load(std::stop_token aStop)
			-> workbench_settings_result<workbench_layout_settings> = 0;
		[[nodiscard]] virtual auto store(
			const workbench_layout_settings& aSettings,
			std::stop_token                  aStop)
			-> workbench_settings_result<void> = 0;
	};

	/// \brief Describes the workbench layout delta record.
	///
	/// \details Drawer widths only. Collapsed and expanded are semantic and travel
	/// in the core's drawer delta instead, so one meaning never has two carriers.
	struct workbench_layout_delta
	{
		std::uint32_t FilesWidth         = 0U;
		std::uint32_t ConversationsWidth = 0U;

		auto operator<=>(const workbench_layout_delta&) const = default;
	};

	/// \brief Describes the workbench projection record.
	///
	/// \details Every domain is engaged only when its published meaning changed,
	/// so a bounded change never forces presentation to rebuild the whole
	/// workbench. The host publishes nothing when no domain changed.
	struct workbench_projection
	{
		workbench_request_context                      Context;
		std::optional<core::workbench_selection_delta> Selection;
		std::optional<core::workbench_drawer_delta>    Drawers;
		std::optional<core::workbench_command_delta>   Commands;
		std::optional<workbench_layout_delta>          Layout;

		auto operator<=>(const workbench_projection&) const = default;
	};

	/// \brief Checks whether the supplied projection carries no changed domain.
	[[nodiscard]] constexpr auto empty(
		const workbench_projection& aProjection) noexcept -> bool
	{
		return !aProjection.Selection.has_value()
			&& !aProjection.Drawers.has_value()
			&& !aProjection.Commands.has_value()
			&& !aProjection.Layout.has_value();
	}

	enum class workbench_host_failure : std::uint8_t {
		observation_unavailable,
		observation_limit,
		settings_unavailable,
		host_failure,
	};

	/// \brief Describes the workbench failure publication record.
	struct workbench_failure_publication
	{
		workbench_request_context Context;
		workbench_host_failure    Failure = workbench_host_failure::host_failure;

		auto operator<=>(const workbench_failure_publication&) const = default;
	};

	/// \brief Provides the workbench projection port contract.
	///
	/// \details Both operations run on the decision lane inside the host's
	/// exception boundary. A stale, superseded, cancelled, duplicate, or foreign
	/// terminal reaches neither operation.
	class workbench_projection_port
	{
	public:
		virtual ~workbench_projection_port() = default;

		virtual void publish(const workbench_projection& aProjection)    = 0;
		virtual void fail(const workbench_failure_publication& aFailure) = 0;
	};

	enum class workbench_terminal_rejection : std::uint8_t {
		none,
		unknown_request,
		inexact_terminal,
		superseded,
		cancelled,
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP
