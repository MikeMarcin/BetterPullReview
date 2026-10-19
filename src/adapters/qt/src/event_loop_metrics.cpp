#include "bpr/qt/event_loop_metrics.hpp"
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>

namespace bpr::qt {
	event_loop_metrics::event_loop_metrics(std::function<void(core::insights_work_sample)> aRecord)
		: Record{ std::move(aRecord) }
	{
		Batch.start();
		auto* dispatcher = QAbstractEventDispatcher::instance();
		QCoreApplication::instance()->installEventFilter(this);
		connect(dispatcher, &QAbstractEventDispatcher::awake, this, [this] { finish_turn(); });
		connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, [this] { finish_turn(); });
	}
	event_loop_metrics::~event_loop_metrics()
	{
		QCoreApplication::instance()->removeEventFilter(this);
		flush();
	}
	bool event_loop_metrics::eventFilter(QObject* aObject, QEvent* aEvent)
	{
		// Dispatchers differ: Unix emits awake after processing, CF can emit
		// it before. Start on the first delivered event and finish on either
		// boundary. Sleep and nested modal waits never enter the duration.
		if (QThread::currentThread()->loopLevel() == 0) {
			Busy.invalidate();
			return QObject::eventFilter(aObject, aEvent);
		}
		if (!Busy.isValid()) {
			Busy.start();
		}
		return QObject::eventFilter(aObject, aEvent);
	}
	void event_loop_metrics::finish_turn()
	{
		if (Busy.isValid()) {
			const auto duration = static_cast<std::uint64_t>(Busy.nsecsElapsed() / 1000);
			try {
				++Latencies[{ QDateTime::currentSecsSinceEpoch() / 300 * 300, duration > 100000 ? duration : duration / 1000 * 1000 }];
			} catch (...) { }
			Busy.invalidate();
		}
		if (Batch.elapsed() >= 30000) {
			flush();
		}
	}

	void event_loop_metrics::flush()
	{
		auto latencies = std::move(Latencies);
		Latencies.clear();
		Batch.restart();
		for (const auto& [bin, count] : latencies) {
			core::insights_work_sample sample;
			sample.At                   = bin.first;
			sample.Kind                 = core::insights_work_kind::latency;
			sample.Count                = count;
			sample.DurationMicroseconds = bin.second;
			try {
				Record(std::move(sample));
			} catch (...) { }
		}
	}
}
