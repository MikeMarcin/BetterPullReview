/// \file
/// \brief Bounded, content-free startup diagnostics for the standalone helper.

#ifndef BPR_ADAPTERS_MCP_TERMINAL_DIAGNOSTIC_HPP
#define BPR_ADAPTERS_MCP_TERMINAL_DIAGNOSTIC_HPP

#include "bpr/adapters/mcp/mcp_context_helper.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>

#if !defined(_WIN32)
	#include <pthread.h>
	#include <signal.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::mcp {
	/// \brief Maximum complete startup record, including its fixed framing.
	inline constexpr auto maximum_terminal_diagnostic_bytes = std::size_t{ 512U };

	/// \brief Reports all environment observations without arbitrary names.
	inline auto terminal_environment_summary(const mcp_environment_report& aReport) -> std::string
	{
		auto result = std::string{ "mcp-diagnostics\tbest_effort\tlimit=512\n" };
		result += "mcp-environment-counts\t" + std::to_string(aReport.ReceivedCount)
			+ "\t" + std::to_string(aReport.RetainedCount)
			+ "\t" + std::to_string(aReport.UnexpectedRetainedCount) + "\n";
		for (auto index = std::size_t{ 0U }; index < mcp_environment_allowlist.size(); ++index) {
			if (aReport.ReceivedAllowlisted[index]) {
				result += "mcp-environment-received\t";
				result += mcp_environment_allowlist[index];
				result += '\n';
			}
			if (aReport.RetainedAllowlisted[index]) {
				result += "mcp-environment-retained\t";
				result += mcp_environment_allowlist[index];
				result += '\n';
			}
		}
		return result;
	}

#if !defined(_WIN32)
	namespace detail {
		struct terminal_write
		{
			std::string_view  Text;
			std::atomic<bool> Complete{ false };
		};

		// POSIX write is a deferred cancellation point. Keep this routine free
		// of locks and owned C++ data, so cancellation can always join safely.
		inline auto write_terminal_record(void* aContext) -> void*
		{
			auto& write = *static_cast<terminal_write*>(aContext);
			static_cast<void>(::write(STDERR_FILENO, write.Text.data(), write.Text.size()));
			write.Complete.store(true, std::memory_order_release);
			return nullptr;
		}
	} // namespace detail
#endif

	/// \brief Attempts one bounded record; diagnostic refusal never changes MCP.
	///
	/// \details POSIX owns exactly one writer, allows 20 ms for ordinary delivery,
	/// then cancels its blocking write and joins before releasing its buffer.
	/// No retry, queue, detached thread or inherited descriptor flag change.
	/// The record may be lost/partial on failure; unavailable stderr cannot
	/// report its own loss. This does not bound uninterruptible device/kernel
	/// stalls. Windows retains synchronous delivery pending its native course.
	inline void write_terminal_diagnostic(const std::string_view aText)
	{
		if (aText.size() > maximum_terminal_diagnostic_bytes) {
			return;
		}
#if defined(_WIN32)
		static_cast<void>(std::fwrite(aText.data(), 1U, aText.size(), stderr));
		static_cast<void>(std::fflush(stderr));
#else
		// Called once before stdin is read and before stdout is written. Some
		// POSIX systems direct a pipe's SIGPIPE to the process, so a writer-only
		// signal mask is insufficient. Restore the disposition after joining;
		// protocol stdout keeps its original behavior.
		struct sigaction ignored{ };
		struct sigaction previous{ };
		ignored.sa_handler = SIG_IGN;
		if (sigemptyset(&ignored.sa_mask) != 0 || ::sigaction(SIGPIPE, &ignored, &previous) != 0) {
			return;
		}
		auto write  = detail::terminal_write{ .Text = aText };
		auto writer = pthread_t{ };
		if (::pthread_create(&writer, nullptr, &detail::write_terminal_record, &write) != 0) {
			static_cast<void>(::sigaction(SIGPIPE, &previous, nullptr));
			return;
		}
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 20 };
		while (!write.Complete.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		if (!write.Complete.load(std::memory_order_acquire)) {
			static_cast<void>(::pthread_cancel(writer));
		}
		static_cast<void>(::pthread_join(writer, nullptr));
		static_cast<void>(::sigaction(SIGPIPE, &previous, nullptr));
#endif
	}
} // namespace bpr::adapters::mcp

#endif
