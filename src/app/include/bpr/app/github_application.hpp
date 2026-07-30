#pragma once

#include "bpr/qt/github_window.hpp"

#include <memory>
#include <optional>
#include <string>

namespace bpr::app
{
namespace detail
{
class github_application_fixture_access;
}

struct github_application_config
{
    std::optional<std::string> EnterpriseHostname = std::nullopt;
};

struct github_application_status
{
    bool Closing = false;
    bool ShutdownComplete = false;
    bool ShutdownCleanupFailed = false;
    bool NativePlatformVaultSelected = false;
};

class github_application final
{
public:
    explicit github_application(github_application_config config = {});
    ~github_application();

    github_application(const github_application&) = delete;
    auto operator=(const github_application&) -> github_application& = delete;
    github_application(github_application&&) = delete;
    auto operator=(github_application&&) -> github_application& = delete;

    void show();
    void begin_shutdown();

    [[nodiscard]] auto window() noexcept -> qt::github_window&;
    [[nodiscard]] auto status() const noexcept -> github_application_status;

private:
    class implementation;
    explicit github_application(
        std::unique_ptr<implementation> implementation) noexcept;

    friend class detail::github_application_fixture_access;

    std::unique_ptr<implementation> Implementation;
};
}
