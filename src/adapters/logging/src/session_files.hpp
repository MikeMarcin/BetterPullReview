#ifndef BPR_LOGGING_SESSION_FILES_HPP
#define BPR_LOGGING_SESSION_FILES_HPP
#include "bpr/logging/quill_log.hpp"
#include <string>
#include <string_view>
namespace bpr::adapters::logging {
	/// \brief Backend-only native session storage; no file work occurs on record ingress.
	class session_files final
	{
	public:
		session_files(const std::filesystem::path& aRoot, std::string aSession, diagnostic_file_limits aLimits);
		~session_files();
		[[nodiscard]] auto write(std::string_view aBytes) noexcept -> application::diagnostic_log_status;
		[[nodiscard]] auto clear() noexcept -> application::diagnostic_clear_result;
		[[nodiscard]] auto failure() const noexcept -> diagnostic_file_failure;

	private:
		class implementation;
		std::unique_ptr<implementation> Implementation;
		diagnostic_file_failure         OpenFailure = diagnostic_file_failure::none;
	};
}
#endif
