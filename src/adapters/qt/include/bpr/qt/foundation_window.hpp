#pragma once

#include "bpr/application/port.hpp"

#include <QMainWindow>

#include <cstddef>
#include <functional>
#include <string>

class QAction;
class QLabel;

namespace bpr::qt
{
struct foundation_view_config
{
    std::string Version;
    std::string Platform;
    std::string Configuration;
};

struct projection_stats
{
    std::size_t CommandUpdates = 0;
    std::size_t StatusUpdates = 0;
    std::size_t DiagnosticUpdates = 0;
    std::size_t FullResets = 0;
};

using semantic_dispatch = std::function<void(core::application_event)>;

class foundation_window final
    : public QMainWindow
    , public application::projection_port
{
public:
    foundation_window(
        foundation_view_config config,
        semantic_dispatch dispatch,
        QWidget* parent = nullptr);

    void publish(const core::transition_output& output) override;

    [[nodiscard]] auto refresh_action() const noexcept -> QAction*;
    [[nodiscard]] auto status_text() const -> std::string;
    [[nodiscard]] auto diagnostic_text() const -> std::string;
    [[nodiscard]] auto statistics() const noexcept -> projection_stats;

private:
    void apply_command(const core::command_projection& command);
    void apply_diagnostic(const core::diagnostic& diagnostic);

    semantic_dispatch Dispatch;
    QAction* RefreshAction = nullptr;
    QLabel* StatusLabel = nullptr;
    QLabel* DiagnosticLabel = nullptr;
    projection_stats Statistics;
};
}
