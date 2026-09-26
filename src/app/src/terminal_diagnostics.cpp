#include "terminal_diagnostics.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <semaphore>
#include <thread>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#else
	#include <pthread.h>
	#include <signal.h>
	#include <unistd.h>
#endif

namespace bpr::app::detail {
	namespace {
		constexpr std::size_t record_bytes = 2048U;
		struct terminal_queue
		{
			struct record
			{
				std::array<char, record_bytes> Bytes;
				std::size_t                    Size = 0U;
			};
			std::array<record, 64U>     Records{ };
			std::mutex                  Producers;
			std::counting_semaphore<65> Ready{ 0 };
			std::counting_semaphore<64> Slots{ 64 };
			std::size_t                 Read  = 0U;
			std::size_t                 Write = 0U;
			std::atomic<std::size_t>    Count{ 0U };
			std::atomic<bool>           Closing{ false };
			std::atomic<std::uint64_t>  Dropped{ 0U };
			std::atomic<bool>           Complete{ false };
		};
		// Borrowed only between explicit process startup and producer quiescence.
		// There is no lazy service creation or fallback when no owner is installed.
		std::atomic<terminal_queue*> active_queue{ nullptr };

		void dropped(terminal_queue& aQueue) noexcept
		{
			auto count = aQueue.Dropped.load(std::memory_order_relaxed);
			while (count != std::numeric_limits<std::uint64_t>::max()
				   && !aQueue.Dropped.compare_exchange_weak(count, count + 1U, std::memory_order_relaxed)) { }
		}

		void qt_diagnostic(QtMsgType aType, const QMessageLogContext&, const QString& aText)
		{
			if (aText == QStringLiteral("dif-catalog-unavailable")) {
				record_terminal_diagnostic("dif-catalog-unavailable\n");
				return;
			}
			const auto text = aType == QtDebugMsg ? "QT_DIAGNOSTIC:debug content=omitted\n"
				: aType == QtInfoMsg              ? "QT_DIAGNOSTIC:info content=omitted\n"
				: aType == QtWarningMsg           ? "QT_DIAGNOSTIC:warning content=omitted\n"
				: aType == QtCriticalMsg          ? "QT_DIAGNOSTIC:critical content=omitted\n"
												  : "QT_DIAGNOSTIC:fatal content=omitted\n";
			record_terminal_diagnostic(text);
		}

		// Producers serialize only against one another; the consumer owns Read.
		// Slot/ready tokens publish and reclaim each bounded record exactly once.
		// Cancellation is enabled only at write, with no held mutex or C++
		// resource requiring stack unwinding. The owner joins before destruction.
		auto drain_terminal(void* aContext) -> void*
		{
			auto& queue = *static_cast<terminal_queue*>(aContext);
#if !defined(_WIN32)
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
#endif
			terminal_queue::record record;
			for (;;) {
				queue.Ready.acquire();
				if (queue.Count.load(std::memory_order_acquire) == 0U) {
					break;
				}
				record     = queue.Records[queue.Read];
				queue.Read = (queue.Read + 1U) % queue.Records.size();
				queue.Count.fetch_sub(1U, std::memory_order_relaxed);
				queue.Slots.release();
#if defined(_WIN32)
				DWORD      written   = 0U;
				const auto delivered = WriteFile(GetStdHandle(STD_ERROR_HANDLE), record.Bytes.data(), static_cast<DWORD>(record.Size), &written, nullptr)
					&& written == record.Size;
#else
				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
				const auto written = ::write(STDERR_FILENO, record.Bytes.data(), record.Size);
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
				const auto delivered = written >= 0 && static_cast<std::size_t>(written) == record.Size;
#endif
				if (!delivered) {
					dropped(queue);
				}
			}
			queue.Complete.store(true, std::memory_order_release);
			return nullptr;
		}
#if defined(_WIN32)
		auto WINAPI drain_windows_terminal(void* aContext) -> DWORD
		{
			drain_terminal(aContext);
			return 0U;
		}
#endif
	} // namespace

	struct terminal_diagnostics::implementation
	{
		terminal_queue Queue;
#if defined(_WIN32)
		HANDLE Writer = nullptr;
#else
		pthread_t        Writer{ };
		struct sigaction PreviousSignal{ };
		bool             Started         = false;
		bool             SignalInstalled = false;
#endif
	};

	terminal_diagnostics::terminal_diagnostics() noexcept
	{
		PreviousHandler = qInstallMessageHandler(qt_diagnostic);
		try {
			Implementation = std::make_unique<implementation>();
		} catch (...) {
			return;
		}
		auto& state = *Implementation;
#if !defined(_WIN32)
		struct sigaction ignored{ };
		ignored.sa_handler = SIG_IGN;
		if (sigemptyset(&ignored.sa_mask) == 0 && ::sigaction(SIGPIPE, &ignored, &state.PreviousSignal) == 0) {
			state.SignalInstalled = true;
			state.Started         = pthread_create(&state.Writer, nullptr, drain_terminal, &state.Queue) == 0;
		}
		if (state.Started) {
			active_queue.store(&state.Queue, std::memory_order_release);
		}
#else
		state.Writer = CreateThread(nullptr, 0U, drain_windows_terminal, &state.Queue, 0U, nullptr);
		if (state.Writer != nullptr) {
			active_queue.store(&state.Queue, std::memory_order_release);
		}
#endif
	}

	terminal_diagnostics::~terminal_diagnostics() noexcept
	{
		qInstallMessageHandler(PreviousHandler);
		if (!Implementation) {
			return;
		}
		auto& state = *Implementation;
		active_queue.store(nullptr, std::memory_order_release);
#if defined(_WIN32)
		const auto started = state.Writer != nullptr;
#else
		const auto started = state.Started;
#endif
		if (started) {
			state.Queue.Closing.store(true, std::memory_order_release);
			state.Queue.Ready.release();
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 20 };
			while (!state.Queue.Complete.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
#if defined(_WIN32)
			// Repeat cancellation through the race before a synchronous write
			// begins. The thread owns no CRT stream lock or application effect.
			while (WaitForSingleObject(state.Writer, 1U) == WAIT_TIMEOUT) {
				CancelSynchronousIo(state.Writer);
			}
			CloseHandle(state.Writer);
#else
			if (!state.Queue.Complete.load(std::memory_order_acquire)) {
				pthread_cancel(state.Writer);
			}
			pthread_join(state.Writer, nullptr);
#endif
		}
#if !defined(_WIN32)
		if (state.SignalInstalled) {
			::sigaction(SIGPIPE, &state.PreviousSignal, nullptr);
		}
#endif
	}

	void record_terminal_diagnostic(const std::string_view aText) noexcept
	{
		auto* const queue = active_queue.load(std::memory_order_acquire);
		if (!queue) {
			return;
		}
		if (aText.empty() || aText.size() > record_bytes - 80U) {
			dropped(*queue);
			return;
		}
		std::unique_lock lock{ queue->Producers, std::try_to_lock };
		if (!lock.owns_lock() || queue->Closing.load(std::memory_order_acquire) || !queue->Slots.try_acquire()) {
			dropped(*queue);
			return;
		}
		auto&      record = queue->Records[queue->Write];
		const auto loss   = queue->Dropped.exchange(0U, std::memory_order_relaxed);
		const auto prefix = loss == 0U ? 0 : std::snprintf(record.Bytes.data(), 80U, "TERMINAL_DIAGNOSTICS:dropped=%llu\n", static_cast<unsigned long long>(loss));
		record.Size       = static_cast<std::size_t>(prefix);
		std::memcpy(record.Bytes.data() + record.Size, aText.data(), aText.size());
		record.Size += aText.size();
		queue->Write = (queue->Write + 1U) % queue->Records.size();
		queue->Count.fetch_add(1U, std::memory_order_release);
		queue->Ready.release();
	}

	auto terminal_text_stream::bounded_buffer::writeData(const char* aData, const qint64 aSize) -> qint64
	{
		if (aSize < 0 || static_cast<std::size_t>(aSize) > Bytes.size() - Size) {
			Overflow = true;
			return aSize;
		}
		std::memcpy(Bytes.data() + Size, aData, static_cast<std::size_t>(aSize));
		Size += static_cast<std::size_t>(aSize);
		return aSize;
	}
} // namespace bpr::app::detail
