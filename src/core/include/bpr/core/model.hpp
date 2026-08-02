#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_MODEL_HPP

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bpr::core {
	/// \brief Provides the strong id contract.
	template <typename Tag>
	class strong_id
	{
	public:
		constexpr strong_id() = default;
		explicit constexpr strong_id(const std::uint64_t aValue) noexcept
			: Value{ aValue }
		{
		}

		[[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
		{
			return Value;
		}

		auto operator<=>(const strong_id&) const = default;

	private:
		std::uint64_t Value = 0;
	};

	struct request_id_tag;
	struct application_session_id_tag;
	struct request_generation_tag;

	using request_id             = strong_id<request_id_tag>;
	using application_session_id = strong_id<application_session_id_tag>;
	using request_generation     = strong_id<request_generation_tag>;

	enum class authority_domain : std::uint8_t {
		github_shared,
		local_private,
		derived_projection,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(authority_domain aAuthority) noexcept -> std::string_view;

	/// \brief Describes the request context record.
	struct request_context
	{
		request_id             Request;
		application_session_id Session;
		request_generation     Generation;

		auto operator<=>(const request_context&) const = default;
	};

	/// \brief Describes the refresh requested intent record.
	struct refresh_requested_intent
	{
		auto operator<=>(const refresh_requested_intent&) const = default;
	};

	/// \brief Describes the foundation refreshed event record.
	struct foundation_refreshed_event
	{
		request_context Context;
		std::uint64_t   GitHubRevision = 0;

		auto operator<=>(const foundation_refreshed_event&) const = default;
	};

	enum class foundation_failure : std::uint8_t {
		unavailable,
	};

	/// \brief Describes the foundation refresh failed event record.
	struct foundation_refresh_failed_event
	{
		request_context    Context;
		foundation_failure Failure = foundation_failure::unavailable;

		auto operator<=>(const foundation_refresh_failed_event&) const = default;
	};

	using application_event = std::variant<
		refresh_requested_intent,
		foundation_refreshed_event,
		foundation_refresh_failed_event>;

	/// \brief Describes the foundation observation record.
	struct foundation_observation
	{
		std::uint64_t GitHubRevision       = 0;
		std::uint64_t LocalPrivateRevision = 0;

		auto operator<=>(const foundation_observation&) const = default;
	};

	/// \brief Describes the refresh foundation request record.
	struct refresh_foundation_request
	{
		request_context Context;

		auto operator<=>(const refresh_foundation_request&) const = default;
	};

	using host_request = std::variant<refresh_foundation_request>;

	enum class command_id : std::uint8_t {
		refresh,
	};

	enum class reason_code : std::uint8_t {
		none,
		refresh_in_progress,
		stale_completion,
		foundation_unavailable,
	};

	/// \brief Describes the command projection record.
	struct command_projection
	{
		command_id  Id             = command_id::refresh;
		bool        Enabled        = true;
		bool        Checked        = false;
		reason_code DisabledReason = reason_code::none;

		auto operator<=>(const command_projection&) const = default;
	};

	/// \brief Describes the status delta record.
	struct status_delta
	{
		std::string Message;

		auto operator<=>(const status_delta&) const = default;
	};

	enum class changed_domain : std::uint8_t {
		none      = 0,
		commands  = 1U << 0U,
		status    = 1U << 1U,
		authority = 1U << 2U,
	};

	[[nodiscard]] constexpr auto operator|(
		const changed_domain aLeft,
		const changed_domain aRight) noexcept -> changed_domain
	{
		return static_cast<changed_domain>(
			static_cast<std::uint8_t>(aLeft) | static_cast<std::uint8_t>(aRight));
	}

	/// \brief Describes the diagnostic record.
	struct diagnostic
	{
		reason_code Code = reason_code::none;
		std::string Detail;

		auto operator<=>(const diagnostic&) const = default;
	};

	/// \brief Describes the transition output record.
	struct transition_output
	{
		changed_domain                  Changed = changed_domain::none;
		std::vector<host_request>       Requests;
		std::vector<command_projection> Commands;
		std::optional<status_delta>     Status;
		std::vector<diagnostic>         Diagnostics;

		auto operator<=>(const transition_output&) const = default;
	};

	/// \brief Describes the application state record.
	struct application_state
	{
		application_session_id    Session;
		request_id                NextRequest{ 1 };
		request_generation        RefreshGeneration;
		std::optional<request_id> PendingRefresh;
		std::uint64_t             GitHubRevision = 0;

		auto operator<=>(const application_state&) const = default;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_MODEL_HPP
