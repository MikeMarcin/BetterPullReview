#pragma once

#include "bpr/core/model.hpp"
#include "bpr/qt/foundation_window.hpp"

#include <memory>

namespace bpr::app
{
class foundation_application final
{
public:
    explicit foundation_application(qt::foundation_view_config config);
    ~foundation_application();

    foundation_application(const foundation_application&) = delete;
    auto operator=(const foundation_application&)
        -> foundation_application& = delete;
    foundation_application(foundation_application&&) = delete;
    auto operator=(foundation_application&&) -> foundation_application& =
        delete;

    void dispatch(core::application_event event);
    void show();

    [[nodiscard]] auto window() noexcept -> qt::foundation_window&;

private:
    class implementation;
    std::unique_ptr<implementation> Implementation;
};
}
