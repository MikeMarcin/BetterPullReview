#pragma once

#include "github_cli_process.hpp"

#include <memory>

namespace bpr::adapters::github::detail
{
class github_cli_process_runner_test_seam
{
public:
    github_cli_process_runner_test_seam() = default;
    github_cli_process_runner_test_seam(
        const github_cli_process_runner_test_seam&) = delete;
    auto operator=(const github_cli_process_runner_test_seam&)
        -> github_cli_process_runner_test_seam& = delete;
    virtual ~github_cli_process_runner_test_seam() = default;

    virtual auto before_execute() -> void = 0;
};

[[nodiscard]] auto make_github_cli_process_runner()
    -> std::unique_ptr<github_cli_process_port>;

[[nodiscard]] auto make_github_cli_process_runner_for_test(
    std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
    -> std::unique_ptr<github_cli_process_port>;
}
