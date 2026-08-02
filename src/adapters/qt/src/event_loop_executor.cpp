#include "bpr/qt/event_loop_executor.hpp"

#include <QMetaObject>

#include <utility>

namespace bpr::qt {
	void event_loop_executor::post(std::function<void()> aTask)
	{
		QMetaObject::invokeMethod(
			&Context,
			[task = std::move(aTask)]() mutable { task(); },
			Qt::QueuedConnection);
	}
}
