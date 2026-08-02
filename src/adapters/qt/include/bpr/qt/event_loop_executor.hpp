#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EVENT_LOOP_EXECUTOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EVENT_LOOP_EXECUTOR_HPP

#include "bpr/application/port.hpp"

#include <QObject>

namespace bpr::qt {
	/// \brief Provides the event loop executor contract.
	class event_loop_executor final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override;

	private:
		QObject Context;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EVENT_LOOP_EXECUTOR_HPP
