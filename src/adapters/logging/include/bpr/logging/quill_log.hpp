#ifndef BPR_LOGGING_QUILL_LOG_HPP
#define BPR_LOGGING_QUILL_LOG_HPP
#include "bpr/application/diagnostic_log_port.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::logging {
	/// \brief Explicit owner of the bounded Quill backend and one append-only local file.
	class quill_log final : public application::diagnostic_log_port
	{
	public:
		explicit quill_log(const std::filesystem::path& aRoot, std::uint64_t aMaximumBytes = 16U * 1024U * 1024U) noexcept;
		~quill_log() override;
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override;
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override;
		/// \brief Called only after producers quiesce; flushes accepted records on ordinary shutdown.
		void close() noexcept;

	private:
		class implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
