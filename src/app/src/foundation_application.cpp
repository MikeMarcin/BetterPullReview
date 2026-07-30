#include "bpr/app/foundation_application.hpp"

#include "bpr/application/host.hpp"
#include "bpr/qt/event_loop_executor.hpp"

#include <utility>

namespace bpr::app
{
namespace
{
class local_observations final
    : public application::foundation_observation_port
{
public:
    [[nodiscard]] auto observe() const
        -> core::foundation_observation override
    {
        return core::foundation_observation{
            .GitHubRevision = 0,
            .LocalPrivateRevision = 0,
        };
    }
};

class local_foundation_effect final
    : public application::foundation_effect_port
{
public:
    void submit(
        const core::refresh_foundation_request& request,
        application::completion_callback completion) override
    {
        completion(core::foundation_refreshed_event{
            .Context = request.Context,
            .GitHubRevision = 0,
        });
    }

    void cancel(core::request_id request) override
    {
        static_cast<void>(request);
    }
};
}

class foundation_application::implementation final
{
public:
    explicit implementation(qt::foundation_view_config config)
        : Window{
              std::move(config),
              [this](core::application_event event) {
                  Host.dispatch(event);
              },
          }
        , Host{
              core::application_state{
                  .Session = core::application_session_id{1},
                  .NextRequest = core::request_id{1},
                  .RefreshGeneration = {},
                  .PendingRefresh = {},
                  .GitHubRevision = 0U,
              },
              Observations,
              Foundation,
              DecisionLane,
              Window,
          }
    {
    }

    local_observations Observations;
    local_foundation_effect Foundation;
    qt::event_loop_executor DecisionLane;
    qt::foundation_window Window;
    application::application_host Host;
};

foundation_application::foundation_application(
    qt::foundation_view_config config)
    : Implementation{
          std::make_unique<implementation>(std::move(config))}
{
}

foundation_application::~foundation_application() = default;

void foundation_application::show()
{
    Implementation->Window.show();
}

auto foundation_application::window() noexcept -> qt::foundation_window&
{
    return Implementation->Window;
}
}
