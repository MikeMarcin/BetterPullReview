#pragma once

#include "bpr/application/git_cache_port.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

namespace bpr::adapters::git
{
struct installed_git_cache_configuration
{
    std::filesystem::path GitExecutable;
    std::filesystem::path StorageRoot;
    std::optional<std::filesystem::path> WindowsSystemRoot;
};

[[nodiscard]] auto make_installed_git_cache(
    const installed_git_cache_configuration& configuration)
    -> std::expected<
        std::unique_ptr<application::git_cache_port>,
        application::git_cache_failure>;
}
