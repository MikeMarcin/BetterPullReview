#include "bpr/qt/event_loop_executor.hpp"

#include <QMetaObject>

#include <utility>

namespace bpr::qt
{
void event_loop_executor::post(std::function<void()> task)
{
    QMetaObject::invokeMethod(
        &Context,
        [task = std::move(task)]() mutable { task(); },
        Qt::QueuedConnection);
}
}
