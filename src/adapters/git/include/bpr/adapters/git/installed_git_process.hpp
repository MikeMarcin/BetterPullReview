#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PROCESS_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PROCESS_HPP

#include "bpr/application/git_process_port.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>

namespace bpr::adapters::git {
	/// \brief Describes the installed git process configuration record.
	struct installed_git_process_configuration
	{
		std::filesystem::path                GitExecutable;
		std::filesystem::path                IsolationRoot;
		std::optional<std::filesystem::path> WindowsSystemRoot;
	};

	/// \brief Creates the installed git process.
	[[nodiscard]] auto make_installed_git_process(
		const installed_git_process_configuration& aConfiguration,
		std::stop_token                            aStop = { })
		-> std::expected<
			std::unique_ptr<application::git_process_port>,
			application::git_process_failure>;
}

#endif // BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PROCESS_HPP
