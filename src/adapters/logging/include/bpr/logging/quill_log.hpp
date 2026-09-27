#ifndef BPR_LOGGING_QUILL_LOG_HPP
#define BPR_LOGGING_QUILL_LOG_HPP
#include "bpr/application/diagnostic_log_port.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
namespace bpr::adapters::logging {
	enum class diagnostic_file_failure : std::uint8_t { none,
														invalid_root,
														invalid_permissions,
														busy,
														io_failure };
	/// \brief Storage budgets shared by all cooperating sessions in one profile directory.
	struct diagnostic_file_limits
	{
		std::uint64_t        FileBytes  = 5U * 1024U * 1024U;
		std::uint64_t        TotalBytes = 50U * 1024U * 1024U;
		std::size_t          Files      = 64U;
		std::chrono::seconds MaximumAge{ std::chrono::hours{ 24 * 30 } };
	};
	/// \brief Explicit owner of the bounded Quill backend and rotating private session files.
	class quill_log final : public application::diagnostic_log_port
	{
	public:
		explicit quill_log(const std::filesystem::path& aRoot, diagnostic_file_limits aLimits = { }) noexcept;
		~quill_log() override;
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override;
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override;
		/// \brief Fixed local admission failure; contains no path, provider data or exception text.
		[[nodiscard]] auto open_failure() const noexcept -> diagnostic_file_failure;
		/// \brief Called only after producers quiesce; flushes accepted records on ordinary shutdown.
		void close() noexcept;

	private:
		class implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
