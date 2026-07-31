#pragma once

#include "bpr/application/git_process_port.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

namespace bpr::adapters::git
{
struct installed_git_process_configuration
{
    std::filesystem::path GitExecutable;
    std::filesystem::path IsolationRoot;
    std::optional<std::filesystem::path> WindowsSystemRoot;
};

[[nodiscard]] auto make_installed_git_process(
    const installed_git_process_configuration& configuration)
    -> std::expected<
        std::unique_ptr<application::git_process_port>,
        application::git_process_failure>;
}
