#ifndef BPR_APP_GITHUB_APPLICATION_THEME_DOCUMENT_FILE_HPP
#define BPR_APP_GITHUB_APPLICATION_THEME_DOCUMENT_FILE_HPP
#include "bpr/application/appearance_runtime_ports.hpp"

#include <cstddef>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::app {
	/// \brief [[BPR-2283]]. Reads at most `aMaximumBytes` of a regular file,
	/// as Import Profile Settings… and a dropped file read it.
	///
	/// \details A path names no authority to stream from: the file is opened
	/// without blocking and identified from the opened descriptor, so a FIFO,
	/// a device or a folder is refused rather than waited on. None when the
	/// file cannot be read.
	[[nodiscard]] auto read_bounded_regular_file(std::string_view aPath, std::size_t aMaximumBytes) -> std::optional<std::string>;

	/// \brief Writes the file Appearance's Export… saves.
	///
	/// \details Qt supplies a path. [[BPR-2283]]: the file is a Profile
	/// Settings file holding only the Theme, replaced whole; a theme is read
	/// through Import Profile Settings, so nothing here reads.
	class bounded_theme_document_reader final
		: public application::theme_document_port
	{
	public:
		[[nodiscard]] auto write(const std::string_view aPath,
								 const std::string_view aDocument, const std::stop_token aStop)
			-> application::theme_document_result<void> override;
	};
}
#endif
