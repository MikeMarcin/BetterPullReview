#ifndef BPR_APP_GITHUB_APPLICATION_THEME_DOCUMENT_FILE_HPP
#define BPR_APP_GITHUB_APPLICATION_THEME_DOCUMENT_FILE_HPP
#include "bpr/application/appearance_runtime_ports.hpp"

#include <cstddef>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::app {
	/// \brief Performs the only local filesystem read in theme import.
	///
	/// \details Qt supplies a path and the application host supplies the byte
	/// bound. Reading one byte beyond the core limit is intentional: the core
	/// parser then returns its typed `document_too_large` refusal instead of
	/// receiving a silently truncated document.
	class bounded_theme_document_reader final
		: public application::theme_document_port
	{
	public:
		[[nodiscard]] auto read(
			const std::string_view aPath,
			const std::size_t      aMaximumBytes,
			const std::stop_token  aStop)
			-> application::theme_document_result<std::string> override;

		[[nodiscard]] auto write(const std::string_view aPath,
								 const std::string_view aDocument, const std::stop_token aStop)
			-> application::theme_document_result<void> override;
	};
}
#endif
