#ifndef BPR_QT_EVENT_LOOP_METRICS_HPP
#define BPR_QT_EVENT_LOOP_METRICS_HPP

#include "bpr/core/insights.hpp"
#include <QElapsedTimer>
#include <QObject>
#include <functional>
#include <map>

namespace bpr::qt {
	/// \brief Measures busy event-loop turns, excluding dispatcher sleep, without a polling timer.
	/// \details Lives on the GUI thread. Batches millisecond latency bins for30seconds,
	/// flushing before a snapshot and shutdown. The callback only queues observations.
	class event_loop_metrics final : public QObject
	{
	public:
		explicit event_loop_metrics(std::function<void(core::insights_work_sample)> aRecord);
		~event_loop_metrics() override;
		void flush();

	private:
		bool                                                            eventFilter(QObject* aObject, QEvent* aEvent) override;
		void                                                            finish_turn();
		std::function<void(core::insights_work_sample)>                 Record;
		QElapsedTimer                                                   Busy, Batch;
		std::map<std::pair<std::int64_t, std::uint64_t>, std::uint64_t> Latencies;
	};
}
#endif
