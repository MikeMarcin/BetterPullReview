#ifndef BPR_APPLICATION_DIAGNOSTIC_LOG_PORT_HPP
#define BPR_APPLICATION_DIAGNOSTIC_LOG_PORT_HPP
#include <array>
#include <cstdint>
#include <string_view>
namespace bpr::application {
	enum class diagnostic_operation : std::uint8_t { application,
													 viewer,
													 capability };
	enum class diagnostic_outcome : std::uint8_t { started,
												   succeeded,
												   failed,
												   cancelled,
												   response_received,
												   shutdown };
	enum class diagnostic_summary_state : std::uint8_t { absent,
														 sanitized,
														 omitted_invalid,
														 omitted_limit,
														 omitted_failure };
	/// \brief Fixed-capacity prose sanitized by the credential-owning adapter before logging.
	struct diagnostic_summary
	{
		std::array<char, 2048>   Text{ };
		std::uint16_t            Size  = 0;
		diagnostic_summary_state State = diagnostic_summary_state::absent;
		[[nodiscard]] auto       text() const noexcept -> std::string_view { return { Text.data(), Size <= Text.size() ? Size : 0U }; }
		auto                     operator==(const diagnostic_summary&) const -> bool = default;
	};
	struct diagnostic_event
	{
		diagnostic_operation Operation  = diagnostic_operation::application;
		diagnostic_outcome   Outcome    = diagnostic_outcome::started;
		std::uint64_t        Request    = 0;
		std::uint16_t        HttpStatus = 0;
		diagnostic_summary   Summary{ };
	};
	enum class diagnostic_log_status : std::uint8_t { available,
													  unavailable,
													  full,
													  dropped,
													  closed };
	/// \brief Nonblocking bounded diagnostic submission; failure never changes domain behavior.
	class diagnostic_log_port
	{
	public:
		virtual ~diagnostic_log_port()                                                                      = default;
		[[nodiscard]] virtual auto record(const diagnostic_event& aEvent) noexcept -> diagnostic_log_status = 0;
		[[nodiscard]] virtual auto status() const noexcept -> diagnostic_log_status                         = 0;
	};
}
#endif
