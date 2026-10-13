#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_HPP

#include "cli_sensitive_bytes.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace bpr::adapters::github::detail {
	inline constexpr std::size_t github_cli_process_max_path_bytes              = 16U * 1024U;
	inline constexpr std::size_t github_cli_process_max_argument_count          = 64U;
	inline constexpr std::size_t github_cli_process_max_argument_bytes          = 8U * 1024U;
	inline constexpr std::size_t github_cli_process_max_arguments_bytes         = 24U * 1024U;
	inline constexpr std::size_t github_cli_process_max_environment_count       = 32U;
	inline constexpr std::size_t github_cli_process_max_environment_name_bytes  = 128U;
	inline constexpr std::size_t github_cli_process_max_environment_value_bytes = 8U * 1024U;
	inline constexpr std::size_t github_cli_process_max_environment_bytes       = 24U * 1024U;
	inline constexpr std::size_t github_cli_process_max_standard_input_bytes    = 4U * 1024U * 1024U;
	inline constexpr std::size_t github_cli_process_max_standard_output_bytes   = 32U * 1024U * 1024U;
	inline constexpr std::size_t github_cli_process_max_standard_error_bytes    = 64U * 1024U;
	inline constexpr auto        github_cli_process_max_timeout                 = std::chrono::minutes{ 5 };
	inline constexpr auto        github_cli_process_max_reap_timeout            = std::chrono::seconds{ 5 };

	struct github_cli_process_environment_entry
	{
		std::string Name;
		std::string Value;

		auto operator==(
			const github_cli_process_environment_entry&) const -> bool = default;
	};

	struct github_cli_process_request
	{
		std::string                                       Executable;
		std::vector<std::string>                          Arguments;
		std::vector<github_cli_process_environment_entry> Environment;
		std::string                                       WorkingDirectory;
		std::string                                       StandardInput;
		std::size_t                                       StandardInputLimit  = 0;
		std::size_t                                       StandardOutputLimit = 0;
		std::size_t                                       StandardErrorLimit  = 0;
		std::chrono::milliseconds                         Timeout{ };
		std::chrono::milliseconds                         ReapTimeout{ };

		// Only gh API response parsing and the bounded local Git clone-help probe
		// opt in to completed nonzero output. Failed/partial output remains unavailable.
		bool PreserveCompletedOutput = false;

		auto operator==(const github_cli_process_request&) const -> bool = default;
	};

	struct github_cli_process_success
	{
		std::string StandardOutput;
		int         ExitCode = 0;

		auto operator==(const github_cli_process_success&) const -> bool = default;
	};

	enum class github_cli_process_failure : std::uint8_t {
		invalid_request,
		start_failure,
		write_failure,
		timeout,
		cancelled,
		standard_output_limit,
		standard_error_limit,
		unexpected_standard_error,
		process_crash,
		nonzero_exit,
		reap_failure,
	};

	using github_cli_process_result = std::expected<
		github_cli_process_success,
		github_cli_process_failure>;

	enum class cli_credential_environment : std::uint8_t { github,
														   enterprise };
	using cli_transient_credential = std::shared_ptr<const cli_sensitive_bytes>;
	using cli_sensitive_result     = std::expected<cli_transient_credential, github_cli_process_failure>;

	class github_cli_process_port
	{
	public:
		github_cli_process_port()                               = default;
		github_cli_process_port(const github_cli_process_port&) = delete;
		auto operator=(const github_cli_process_port&)
			-> github_cli_process_port&                    = delete;
		github_cli_process_port(github_cli_process_port&&) = delete;
		auto operator=(github_cli_process_port&&)
			-> github_cli_process_port&    = delete;
		virtual ~github_cli_process_port() = default;

		[[nodiscard]] virtual auto run(
			const github_cli_process_request& aRequest,
			std::stop_token                   aStop) -> github_cli_process_result = 0;

		/// \brief Closed credential acquisition; output never enters generic result storage.
		[[nodiscard]] virtual auto acquire_sensitive(const github_cli_process_request&, std::stop_token, std::shared_ptr<const void> = { }) -> cli_sensitive_result
		{
			return std::unexpected{ github_cli_process_failure::invalid_request };
		}
		/// \brief Supplies one operation-owned credential outside copyable generic requests.
		[[nodiscard]] virtual auto run_credentialed(const github_cli_process_request&, cli_transient_credential,
													cli_credential_environment, std::stop_token) -> github_cli_process_result
		{
			return std::unexpected{ github_cli_process_failure::invalid_request };
		}
	};
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_HPP
