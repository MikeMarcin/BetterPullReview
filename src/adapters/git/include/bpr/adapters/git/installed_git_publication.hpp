#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PUBLICATION_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PUBLICATION_HPP

#include "bpr/application/git_process_port.hpp"
#include "bpr/application/publication_candidate_port.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>

/// \file
/// \brief Installed Git as the publication candidate catalog (RFC-BPR-0059).
/// \details The same isolated runner as the epoch cache — an owner-private
/// isolation root with its own home, configuration, empty templates and
/// hooks, no system configuration, no credential helper, no protocol and no
/// lazy fetch — driving a separate catalog of plumbing commands against a
/// BPR-owned store. It shares no repository or operation with the cache.
namespace bpr::adapters::git {
	struct installed_git_publication_configuration
	{
		std::filesystem::path GitExecutable;
		/// Owner-private; the store lives in its `publication/v1`.
		std::filesystem::path                IsolationRoot;
		std::optional<std::filesystem::path> WindowsSystemRoot;
	};

	[[nodiscard]] auto make_installed_git_publication(
		const installed_git_publication_configuration& aConfiguration,
		std::stop_token                                aStop = { })
		-> std::expected<
			std::unique_ptr<application::publication_candidate_port>,
			application::git_process_error>;
}

#endif
