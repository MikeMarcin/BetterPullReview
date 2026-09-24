#include "windows_agent_process.hpp"

#include "agent_protocol_loop.hpp"
#include "bpr/adapters/agent/windows_workspace_guard.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace bpr::adapters::agent::detail {
	namespace {
		using clock = std::chrono::steady_clock;

		constexpr auto io_poll_interval = std::chrono::milliseconds{ 10 };
		constexpr auto reap_timeout     = std::chrono::seconds{ 5 };

#if defined(BPR_BUILD_TESTING)
		std::atomic_bool ForceJobAssignmentFailure = false;
#endif

		class unique_handle final
		{
		public:
			unique_handle() noexcept = default;
			explicit unique_handle(HANDLE aHandle) noexcept
				: Handle{ aHandle }
			{
			}
			unique_handle(const unique_handle&)                    = delete;
			auto operator=(const unique_handle&) -> unique_handle& = delete;
			unique_handle(unique_handle&& aOther) noexcept
				: Handle{ std::exchange(aOther.Handle, nullptr) }
			{
			}
			auto operator=(unique_handle&& aOther) noexcept -> unique_handle&
			{
				if (this != &aOther) {
					reset(std::exchange(aOther.Handle, nullptr));
				}
				return *this;
			}
			~unique_handle()
			{
				reset();
			}

			[[nodiscard]] auto get() const noexcept -> HANDLE
			{
				return Handle;
			}

			[[nodiscard]] explicit operator bool() const noexcept
			{
				return Handle != nullptr && Handle != INVALID_HANDLE_VALUE;
			}

			void reset(HANDLE aReplacement = nullptr) noexcept
			{
				if (Handle != nullptr && Handle != INVALID_HANDLE_VALUE) {
					static_cast<void>(CloseHandle(Handle));
				}
				Handle = aReplacement;
			}

		private:
			HANDLE Handle = nullptr;
		};

		struct pipe_pair
		{
			unique_handle Parent;
			unique_handle Child;
		};

		class attribute_list final
		{
		public:
			[[nodiscard]] auto initialize() -> bool
			{
				auto bytes = SIZE_T{ 0U };
				static_cast<void>(InitializeProcThreadAttributeList(
					nullptr, 1U, 0U, &bytes));
				if (bytes == 0U) {
					return false;
				}
				Storage.resize(bytes);
				List = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
					Storage.data());
				return InitializeProcThreadAttributeList(
						   List, 1U, 0U, &bytes)
					!= FALSE;
			}

			[[nodiscard]] auto set_handle_list(
				const std::span<HANDLE> aHandles) -> bool
			{
				return UpdateProcThreadAttribute(
						   List,
						   0U,
						   PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
						   aHandles.data(),
						   aHandles.size() * sizeof(HANDLE),
						   nullptr,
						   nullptr)
					!= FALSE;
			}

			[[nodiscard]] auto get() const noexcept
				-> LPPROC_THREAD_ATTRIBUTE_LIST
			{
				return List;
			}

			~attribute_list()
			{
				if (List != nullptr) {
					DeleteProcThreadAttributeList(List);
				}
			}

		private:
			std::vector<std::byte>       Storage;
			LPPROC_THREAD_ATTRIBUTE_LIST List = nullptr;
		};

		[[nodiscard]] auto utf16(const std::string_view aValue)
			-> std::optional<std::wstring>
		{
			if (
				aValue.find('\0') != std::string_view::npos
				|| aValue.size()
					> static_cast<std::size_t>(std::numeric_limits<int>::max())) {
				return std::nullopt;
			}
			if (aValue.empty()) {
				return std::wstring{ };
			}
			const auto required = MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				aValue.data(),
				static_cast<int>(aValue.size()),
				nullptr,
				0);
			if (required <= 0) {
				return std::nullopt;
			}
			auto converted = std::wstring(static_cast<std::size_t>(required), L'\0');
			if (
				MultiByteToWideChar(
					CP_UTF8,
					MB_ERR_INVALID_CHARS,
					aValue.data(),
					static_cast<int>(aValue.size()),
					converted.data(),
					required)
				!= required) {
				return std::nullopt;
			}
			return converted;
		}

		[[nodiscard]] auto case_insensitive_less(
			const std::wstring& aLeft, const std::wstring& aRight) noexcept -> bool
		{
			return CompareStringOrdinal(
					   aLeft.data(),
					   static_cast<int>(aLeft.size()),
					   aRight.data(),
					   static_cast<int>(aRight.size()),
					   TRUE)
				== CSTR_LESS_THAN;
		}

		[[nodiscard]] auto equal_case_insensitive(
			const std::wstring& aLeft, const std::wstring& aRight) noexcept -> bool
		{
			return CompareStringOrdinal(
					   aLeft.data(),
					   static_cast<int>(aLeft.size()),
					   aRight.data(),
					   static_cast<int>(aRight.size()),
					   TRUE)
				== CSTR_EQUAL;
		}

		[[nodiscard]] auto environment_block(
			const std::vector<std::pair<std::string, std::string>>& aEnvironment)
			-> std::optional<std::vector<wchar_t>>
		{
			auto entries = std::vector<std::pair<std::wstring, std::wstring>>{ };
			entries.reserve(aEnvironment.size());
			for (const auto& [name, value] : aEnvironment) {
				auto wideName  = utf16(name);
				auto wideValue = utf16(value);
				if (
					!wideName || !wideValue || wideName->empty()
					|| wideName->find(L'=') != std::wstring::npos) {
					return std::nullopt;
				}
				entries.emplace_back(std::move(*wideName), std::move(*wideValue));
			}
			std::ranges::sort(entries, [](const auto& aLeft, const auto& aRight) {
				return case_insensitive_less(aLeft.first, aRight.first);
			});
			for (auto index = std::size_t{ 1U }; index < entries.size(); ++index) {
				if (equal_case_insensitive(
						entries[index - 1U].first, entries[index].first)) {
					return std::nullopt;
				}
			}

			auto block = std::vector<wchar_t>{ };
			for (const auto& [name, value] : entries) {
				block.insert(block.end(), name.begin(), name.end());
				block.push_back(L'=');
				block.insert(block.end(), value.begin(), value.end());
				block.push_back(L'\0');
			}
			block.push_back(L'\0');
			if (entries.empty()) {
				block.push_back(L'\0');
			}
			return block;
		}

		void append_quoted_argument(
			std::wstring& aCommandLine, const std::wstring_view aArgument)
		{
			if (!aCommandLine.empty()) {
				aCommandLine.push_back(L' ');
			}
			aCommandLine.push_back(L'"');
			auto backslashes = std::size_t{ 0U };
			for (const auto character : aArgument) {
				if (character == L'\\') {
					++backslashes;
					continue;
				}
				if (character == L'"') {
					aCommandLine.append(backslashes * 2U + 1U, L'\\');
					aCommandLine.push_back(L'"');
					backslashes = 0U;
					continue;
				}
				aCommandLine.append(backslashes, L'\\');
				backslashes = 0U;
				aCommandLine.push_back(character);
			}
			aCommandLine.append(backslashes * 2U, L'\\');
			aCommandLine.push_back(L'"');
		}

		[[nodiscard]] auto command_line(
			const std::wstring&             aExecutable,
			const std::vector<std::string>& aArguments)
			-> std::optional<std::wstring>
		{
			auto result = std::wstring{ };
			append_quoted_argument(result, aExecutable);
			for (const auto& argument : aArguments) {
				auto wideArgument = utf16(argument);
				if (!wideArgument) {
					return std::nullopt;
				}
				append_quoted_argument(result, *wideArgument);
			}
			result.push_back(L'\0');
			return result;
		}

		[[nodiscard]] auto make_pipe(const bool aParentWrites)
			-> std::optional<pipe_pair>
		{
			auto attributes                 = SECURITY_ATTRIBUTES{ };
			attributes.nLength              = sizeof(attributes);
			attributes.bInheritHandle       = TRUE;
			attributes.lpSecurityDescriptor = nullptr;
			auto readHandle                 = HANDLE{ nullptr };
			auto writeHandle                = HANDLE{ nullptr };
			if (
				CreatePipe(&readHandle, &writeHandle, &attributes, 0U)
				== FALSE) {
				return std::nullopt;
			}
			auto read   = unique_handle{ readHandle };
			auto write  = unique_handle{ writeHandle };
			auto result = pipe_pair{
				.Parent = aParentWrites ? std::move(write) : std::move(read),
				.Child  = aParentWrites ? std::move(read) : std::move(write),
			};
			if (
				SetHandleInformation(
					result.Parent.get(), HANDLE_FLAG_INHERIT, 0U)
				== FALSE) {
				return std::nullopt;
			}
			return result;
		}

		[[nodiscard]] auto assign_process_to_job(
			const HANDLE aJob, const HANDLE aProcess) noexcept -> bool
		{
#if defined(BPR_BUILD_TESTING)
			if (ForceJobAssignmentFailure.load(std::memory_order_acquire)) {
				return false;
			}
#endif
			return AssignProcessToJobObject(aJob, aProcess) != FALSE;
		}

		/// \brief Writes aBytes to the child's input, servicing aWhileWriting
		/// between polls until every byte is taken, the deadline passes, the
		/// run is cancelled or the child exits.
		[[nodiscard]] auto write_all(
			const HANDLE                 aPipe,
			const HANDLE                 aProcess,
			const std::string_view       aBytes,
			const clock::time_point      aDeadline,
			const cancellation_probe&    aCancelled,
			const std::function<void()>& aWhileWriting) -> agent_protocol_write_status
		{
			auto completed = std::atomic_bool{ false };
			auto succeeded = std::atomic_bool{ false };
			auto writer    = std::jthread{ [&] {
				auto offset = std::size_t{ 0U };
				while (offset < aBytes.size()) {
					const auto requested = static_cast<DWORD>(std::min<std::size_t>(
						aBytes.size() - offset,
						16U * 1024U));
					auto       written   = DWORD{ 0U };
					if (
						WriteFile(
							aPipe,
							aBytes.data() + offset,
							requested,
							&written,
							nullptr)
							== FALSE
						|| written == 0U) {
						completed.store(true, std::memory_order_release);
						return;
					}
					offset += written;
				}
				succeeded.store(true, std::memory_order_release);
				completed.store(true, std::memory_order_release);
			} };

			auto interrupted = agent_protocol_write_status::failed;
			while (!completed.load(std::memory_order_acquire)) {
				if (aCancelled()) {
					interrupted = agent_protocol_write_status::cancelled;
					break;
				}
				if (clock::now() >= aDeadline) {
					interrupted = agent_protocol_write_status::timeout;
					break;
				}
				if (WaitForSingleObject(aProcess, 0U) == WAIT_OBJECT_0) {
					interrupted = agent_protocol_write_status::process_exited;
					break;
				}
				aWhileWriting();
				static_cast<void>(WaitForSingleObject(
					writer.native_handle(),
					static_cast<DWORD>(io_poll_interval.count())));
			}
			if (!completed.load(std::memory_order_acquire)) {
				static_cast<void>(CancelIoEx(aPipe, nullptr));
				static_cast<void>(CancelSynchronousIo(writer.native_handle()));
			}
			writer.join();
			if (interrupted != agent_protocol_write_status::failed) {
				return interrupted;
			}
			return succeeded.load(std::memory_order_acquire)
				? agent_protocol_write_status::success
				: agent_protocol_write_status::failed;
		}

		[[nodiscard]] auto read_available(
			const HANDLE          aPipe,
			std::size_t&          aBatchRemaining,
			const std::span<char> aBuffer)
			-> agent_protocol_pipe_read_result
		{
			if (aBatchRemaining == 0U) {
				auto available = DWORD{ 0U };
				if (
					PeekNamedPipe(aPipe, nullptr, 0U, nullptr, &available, nullptr)
					== FALSE) {
					return GetLastError() == ERROR_BROKEN_PIPE
						? agent_protocol_pipe_read_result{
							  .Status = agent_protocol_pipe_read_status::closed,
						  }
						: agent_protocol_pipe_read_result{
							  .Status = agent_protocol_pipe_read_status::failed,
						  };
				}
				aBatchRemaining = static_cast<std::size_t>(available);
			}
			if (aBatchRemaining == 0U) {
				return { .Status = agent_protocol_pipe_read_status::no_data };
			}
			const auto requestedSize
				= std::min(aBuffer.size(), aBatchRemaining);
			const auto requested = static_cast<DWORD>(requestedSize);
			auto       read      = DWORD{ 0U };
			if (
				ReadFile(aPipe, aBuffer.data(), requested, &read, nullptr) == FALSE) {
				const auto error = GetLastError();
				aBatchRemaining  = 0U;
				return error == ERROR_BROKEN_PIPE
					? agent_protocol_pipe_read_result{
						  .Status = agent_protocol_pipe_read_status::closed,
					  }
					: agent_protocol_pipe_read_result{
						  .Status = agent_protocol_pipe_read_status::failed,
					  };
			}
			if (read == 0U || read > aBatchRemaining) {
				aBatchRemaining = 0U;
				return { .Status = agent_protocol_pipe_read_status::failed };
			}
			aBatchRemaining -= static_cast<std::size_t>(read);
			return {
				.Status        = agent_protocol_pipe_read_status::data,
				.Count         = static_cast<std::size_t>(read),
				.BatchComplete = aBatchRemaining == 0U,
			};
		}

		/// \brief Child output read while a request was being written.
		///
		/// \details [[BPR-0886]]. Kept in order and served before whatever the
		/// pipe still holds, so the protocol loop reads exactly the bytes the
		/// child wrote, only later.
		struct parked_output
		{
			std::string Bytes;
			std::size_t Offset = 0U;

			[[nodiscard]] auto pending() const noexcept -> std::size_t
			{
				return Bytes.size() - Offset;
			}
		};

		/// \brief Moves what aPipe holds now into aParked, up to aCeiling parked
		/// bytes, without blocking.
		///
		/// \details A batch the protocol loop has already begun on this pipe is
		/// left for it to finish, so no byte is read twice or out of order. The
		/// ceiling is one past the loop's own, so a child that writes too much
		/// before reading still meets the loop's refusal.
		void park_available(
			const HANDLE      aPipe,
			const std::size_t aBatchRemaining,
			parked_output&    aParked,
			const std::size_t aCeiling)
		{
			if (aBatchRemaining != 0U) {
				return;
			}
			while (aParked.pending() < aCeiling) {
				auto available = DWORD{ 0U };
				if (
					PeekNamedPipe(aPipe, nullptr, 0U, nullptr, &available, nullptr) == FALSE
					|| available == 0U) {
					return;
				}
				const auto requested = static_cast<DWORD>(std::min({
					static_cast<std::size_t>(available),
					aCeiling - aParked.pending(),
					std::size_t{ 64U * 1024U },
				}));
				const auto start     = aParked.Bytes.size();
				aParked.Bytes.resize(start + requested);
				auto read = DWORD{ 0U };
				if (
					ReadFile(aPipe, aParked.Bytes.data() + start, requested, &read, nullptr) == FALSE
					|| read == 0U) {
					aParked.Bytes.resize(start);
					return;
				}
				aParked.Bytes.resize(start + read);
			}
		}

		/// \brief Serves parked bytes, as one batch, before the pipe's own.
		[[nodiscard]] auto read_parked(parked_output& aParked, const std::span<char> aBuffer)
			-> std::optional<agent_protocol_pipe_read_result>
		{
			if (aParked.pending() == 0U) {
				return std::nullopt;
			}
			const auto count = std::min(aBuffer.size(), aParked.pending());
			std::copy_n(aParked.Bytes.data() + aParked.Offset, count, aBuffer.data());
			aParked.Offset += count;
			const auto complete = aParked.pending() == 0U;
			if (complete) {
				aParked.Bytes.clear();
				aParked.Offset = 0U;
			}
			return agent_protocol_pipe_read_result{
				.Status        = agent_protocol_pipe_read_status::data,
				.Count         = count,
				.BatchComplete = complete,
			};
		}

		[[nodiscard]] auto active_job_processes(const HANDLE aJob)
			-> std::optional<DWORD>
		{
			auto accounting = JOBOBJECT_BASIC_ACCOUNTING_INFORMATION{ };
			if (
				QueryInformationJobObject(
					aJob,
					JobObjectBasicAccountingInformation,
					&accounting,
					sizeof(accounting),
					nullptr)
				== FALSE) {
				return std::nullopt;
			}
			return accounting.ActiveProcesses;
		}

		/// [[BPR-0622]]. A supervised child is owned by the supervisor's job, not
		/// by one this process holds. Closing the owner pipe asks the supervisor
		/// to terminate that job and confirm it empty before it exits; only if it
		/// does not exit in time is it terminated, which fires its own job's
		/// kill-on-close. The supervisor's exit is what releases its guard lock.
		[[nodiscard]] auto end_supervisor(
			unique_handle& aOwner, const HANDLE aSupervisor, const HANDLE aInput, const HANDLE aOutput, const HANDLE aError) -> bool
		{
			for (const auto handle : { aInput, aOutput, aError }) {
				if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
					static_cast<void>(CancelIoEx(handle, nullptr));
				}
			}
			aOwner.reset();
			if (WaitForSingleObject(aSupervisor, static_cast<DWORD>(reap_timeout.count() * 1'000)) == WAIT_OBJECT_0) {
				return true;
			}
			static_cast<void>(TerminateProcess(aSupervisor, 1U));
			static_cast<void>(WaitForSingleObject(aSupervisor, static_cast<DWORD>(reap_timeout.count() * 1'000)));
			return false;
		}

		[[nodiscard]] auto terminate_and_reap(
			const HANDLE aJob,
			const HANDLE aProcess,
			const HANDLE aInput,
			const HANDLE aOutput,
			const HANDLE aError) -> bool
		{
			for (const auto handle : { aInput, aOutput, aError }) {
				if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
					static_cast<void>(CancelIoEx(handle, nullptr));
				}
			}
			if (TerminateJobObject(aJob, 1U) == FALSE) {
				return false;
			}
			const auto deadline = clock::now() + reap_timeout;
			if (
				WaitForSingleObject(
					aProcess, static_cast<DWORD>(reap_timeout.count() * 1'000))
				!= WAIT_OBJECT_0) {
				return false;
			}
			while (clock::now() < deadline) {
				const auto active = active_job_processes(aJob);
				if (active && *active == 0U) {
					return true;
				}
				std::this_thread::sleep_for(io_poll_interval);
			}
			return false;
		}

		class windows_agent_protocol_transport final
			: public agent_protocol_transport
		{
		public:
			windows_agent_protocol_transport(
				unique_handle&            aInput,
				unique_handle&            aOutput,
				unique_handle&            aError,
				const HANDLE              aProcess,
				const HANDLE              aJob,
				const clock::time_point   aDeadline,
				const cancellation_probe& aCancelled,
				unique_handle* const      aSupervisorOwner = nullptr)
				: Input{ aInput }
				, Output{ aOutput }
				, Error{ aError }
				, Process{ aProcess }
				, Job{ aJob }
				, Deadline{ aDeadline }
				, Cancelled{ aCancelled }
				, SupervisorOwner{ aSupervisorOwner }
			{
			}

			[[nodiscard]] auto write(const std::string_view aBytes)
				-> agent_protocol_write_status override
			{
				// [[BPR-0886]]. A child may write before it reads. The pipes hold
				// a few kilobytes, so a child whose output fills its pipe stops
				// reading its input, and a request larger than the input pipe
				// then waits on it for good. The child's output is read into
				// order-keeping buffers while the request is written, as the
				// POSIX transport's QProcess reads while it writes.
				return write_all(Input.get(), Process, aBytes, Deadline, Cancelled, [this] {
					park_available(
						Output.get(), OutputBatchRemaining, ParkedOutput, application::maximum_agent_stdout_bytes + 1U);
					park_available(
						Error.get(), ErrorBatchRemaining, ParkedError, application::maximum_agent_stderr_bytes + 1U);
				});
			}

			void close_input() noexcept override
			{
				Input.reset();
			}

			[[nodiscard]] auto read_standard_output(std::span<char> aBuffer)
				-> agent_protocol_pipe_read_result override
			{
				if (auto parked = read_parked(ParkedOutput, aBuffer)) {
					return *parked;
				}
				return read_available(Output.get(), OutputBatchRemaining, aBuffer);
			}

			[[nodiscard]] auto read_standard_error(std::span<char> aBuffer)
				-> agent_protocol_pipe_read_result override
			{
				if (auto parked = read_parked(ParkedError, aBuffer)) {
					return *parked;
				}
				return read_available(Error.get(), ErrorBatchRemaining, aBuffer);
			}

			[[nodiscard]] auto wait() -> agent_protocol_wait_result override
			{
				if (Cancelled()) {
					return { .Status = agent_protocol_wait_status::cancelled };
				}
				if (clock::now() >= Deadline) {
					return { .Status = agent_protocol_wait_status::timeout };
				}
				const auto state = WaitForSingleObject(Process, 0U);
				if (state == WAIT_OBJECT_0) {
					auto exitCode = DWORD{ 0U };
					if (GetExitCodeProcess(Process, &exitCode) == FALSE) {
						return { .Status = agent_protocol_wait_status::failed };
					}
					return {
						.Status   = agent_protocol_wait_status::exited,
						.ExitCode = static_cast<int>(exitCode),
					};
				}
				if (state == WAIT_FAILED) {
					return { .Status = agent_protocol_wait_status::failed };
				}
				static_cast<void>(WaitForSingleObject(
					Process, static_cast<DWORD>(io_poll_interval.count())));
				return { };
			}

			[[nodiscard]] auto terminate() noexcept -> bool override
			{
				if (SupervisorOwner != nullptr) {
					return end_supervisor(*SupervisorOwner, Process, Input.get(), Output.get(), Error.get());
				}
				return terminate_and_reap(
					Job, Process, Input.get(), Output.get(), Error.get());
			}

		private:
			unique_handle&            Input;
			unique_handle&            Output;
			unique_handle&            Error;
			HANDLE                    Process;
			HANDLE                    Job;
			clock::time_point         Deadline;
			const cancellation_probe& Cancelled;
			unique_handle*            SupervisorOwner      = nullptr;
			std::size_t               OutputBatchRemaining = 0U;
			std::size_t               ErrorBatchRemaining  = 0U;
			parked_output             ParkedOutput;
			parked_output             ParkedError;
		};
		[[nodiscard]] auto supervisor_executable() -> std::wstring
		{
#if defined(BPR_AGENT_SUPERVISOR_EXECUTABLE_W)
			return BPR_AGENT_SUPERVISOR_EXECUTABLE_W;
#else
			// Bundled beside the application image, exactly as on Unix.
			auto       path   = std::wstring(32'768U, L'\0');
			const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
			if (length == 0U || length >= path.size()) {
				return { };
			}
			path.resize(length);
			const auto separator = path.find_last_of(L'\\');
			if (separator == std::wstring::npos) {
				return { };
			}
			path.resize(separator + 1U);
			return path + L"better_pull_review_agent_supervisor.exe";
#endif
		}

		[[nodiscard]] auto write_value(const HANDLE aPipe, const void* const aData, const DWORD aBytes) -> bool
		{
			auto written = DWORD{ 0U };
			while (written < aBytes) {
				auto count = DWORD{ 0U };
				if (WriteFile(aPipe, static_cast<const char*>(aData) + written, aBytes - written, &count, nullptr) == FALSE || count == 0U) {
					return false;
				}
				written += count;
			}
			return true;
		}

		[[nodiscard]] auto write_string(const HANDLE aPipe, const std::wstring& aValue) -> bool
		{
			const auto length = static_cast<DWORD>(aValue.size());
			return write_value(aPipe, &length, sizeof(length))
				&& write_value(aPipe, aValue.data(), length * static_cast<DWORD>(sizeof(wchar_t)));
		}

		/// [[BPR-0622]]. A guarded launch: this process keeps its own shared
		/// guard lock, and the bundled supervisor, outside any job this process
		/// holds, takes its own lock, owns the child's job and releases the
		/// guard only once that job is confirmed empty.
		[[nodiscard]] auto run_supervised_agent_process(
			const application::agent_launch_request& aRequest,
			const std::wstring&                      aExecutable,
			const std::wstring&                      aCommand,
			std::vector<wchar_t>&                    aEnvironment,
			const std::wstring&                      aWorkingDirectory,
			pipe_pair&                               aInput,
			pipe_pair&                               aOutput,
			pipe_pair&                               aError,
			const int                                aTimeoutMilliseconds,
			const application::agent_output_sink&    aSink,
			const cancellation_probe&                aCancelled) -> application::agent_run_result
		{
			auto        result     = application::agent_run_result{ .Session = aRequest.Session };
			const auto& requested  = *aRequest.WorkspaceLifetimeGuard;
			const auto  guardPath  = utf16(requested.Path);
			auto        ownerGuard = guardPath
				? open_windows_workspace_guard(*guardPath, requested.FileIdentity, windows_guard_lock::shared)
				: windows_handle{ };
			if (!ownerGuard) {
				result.StandardError = "workspace lifetime guard could not be retained";
				return result;
			}
			const auto supervisor = supervisor_executable();
			// Not the workspace: the supervisor's current directory handle would
			// keep the workspace from being renamed while the supervisor lives.
			const auto supervisorDirectory = std::filesystem::path{ supervisor }.parent_path().wstring();
			auto       lifetime            = make_pipe(true);
			auto       report              = make_pipe(false);
			if (supervisor.empty() || supervisorDirectory.empty() || !lifetime || !report) {
				result.StandardError = "agent supervisor channel creation failed";
				return result;
			}
			auto inheritedHandles = std::array<HANDLE, 5U>{
				aInput.Child.get(), aOutput.Child.get(), aError.Child.get(), lifetime->Child.get(), report->Child.get()
			};
			auto attributes = attribute_list{ };
			if (!attributes.initialize() || !attributes.set_handle_list(inheritedHandles)) {
				result.StandardError = "agent private handle-list creation failed";
				return result;
			}
			auto supervisorCommand = std::wstring{ L"\"" } + supervisor + L"\" --workspace-owner "
				+ std::to_wstring(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(lifetime->Child.get()))) + L" "
				+ std::to_wstring(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(report->Child.get())));
			auto startup                   = STARTUPINFOEXW{ };
			startup.StartupInfo.cb         = sizeof(startup);
			startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
			startup.StartupInfo.hStdInput  = aInput.Child.get();
			startup.StartupInfo.hStdOutput = aOutput.Child.get();
			startup.StartupInfo.hStdError  = aError.Child.get();
			startup.lpAttributeList        = attributes.get();
			auto       processInformation  = PROCESS_INFORMATION{ };
			const auto created             = CreateProcessW(
												 supervisor.c_str(),
												 supervisorCommand.data(),
												 nullptr,
												 nullptr,
												 TRUE,
												 CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
												 aEnvironment.data(),
												 supervisorDirectory.c_str(),
												 &startup.StartupInfo,
												 &processInformation)
				!= FALSE;
			auto process = unique_handle{ processInformation.hProcess };
			auto thread  = unique_handle{ processInformation.hThread };
			thread.reset();
			aInput.Child.reset();
			aOutput.Child.reset();
			aError.Child.reset();
			lifetime->Child.reset();
			report->Child.reset();
			if (!created) {
				result.StandardError = "agent supervisor process creation failed";
				return result;
			}

			const auto deadline = clock::now()
				+ std::chrono::milliseconds{ std::max(aTimeoutMilliseconds, 0) };
			// The command line arrives as a mutable CreateProcessW buffer with its
			// terminator; the specification carries text, which may not contain NUL.
			auto commandText = aCommand;
			while (!commandText.empty() && commandText.back() == L'\0') {
				commandText.pop_back();
			}
			const auto magic       = DWORD{ 0x53525042U };
			const auto version     = DWORD{ 2U };
			const auto flags       = DWORD{ 1U };
			const auto specWritten = write_value(lifetime->Parent.get(), &magic, sizeof(magic))
				&& write_value(lifetime->Parent.get(), &version, sizeof(version))
				&& write_value(lifetime->Parent.get(), &flags, sizeof(flags))
				&& write_string(lifetime->Parent.get(), aExecutable)
				&& write_string(lifetime->Parent.get(), commandText)
				&& write_string(lifetime->Parent.get(), aWorkingDirectory)
				&& write_string(lifetime->Parent.get(), *guardPath)
				&& write_value(lifetime->Parent.get(), requested.FileIdentity.data(), sizeof(requested.FileIdentity));

			auto startError = std::int32_t{ ERROR_BROKEN_PIPE };
			auto received   = DWORD{ 0U };
			auto outcome    = application::agent_run_outcome::failed_to_start;
			while (specWritten && received < sizeof(startError)) {
				if (aCancelled() || clock::now() >= deadline) {
					outcome = aCancelled() ? application::agent_run_outcome::cancelled : application::agent_run_outcome::timeout;
					break;
				}
				auto available = DWORD{ 0U };
				if (PeekNamedPipe(report->Parent.get(), nullptr, 0U, nullptr, &available, nullptr) == FALSE) {
					break;
				}
				if (available == 0U) {
					std::this_thread::sleep_for(io_poll_interval);
					continue;
				}
				auto count = DWORD{ 0U };
				if (ReadFile(report->Parent.get(), reinterpret_cast<char*>(&startError) + received, sizeof(startError) - received, &count, nullptr) == FALSE || count == 0U) {
					break;
				}
				received += count;
			}
			if (received != sizeof(startError) || startError != 0) {
				static_cast<void>(end_supervisor(lifetime->Parent, process.get(), aInput.Parent.get(), aOutput.Parent.get(), aError.Parent.get()));
				result.Outcome = outcome;
				if (outcome == application::agent_run_outcome::failed_to_start) {
					result.StandardError = startError == static_cast<std::int32_t>(ERROR_LOCK_VIOLATION)
						? "workspace lifetime guard could not be retained"
						: "agent supervisor could not start the agent (Windows error " + std::to_string(startError)
							+ ", supervisor exit " + std::to_string([&] { auto code = DWORD{ 0U }; return GetExitCodeProcess(process.get(), &code) != FALSE ? code : DWORD{ 0xFFFFFFFFU }; }()) + ")";
				}
				return result;
			}

			auto transport = windows_agent_protocol_transport{
				aInput.Parent,
				aOutput.Parent,
				aError.Parent,
				process.get(),
				nullptr,
				deadline,
				aCancelled,
				&lifetime->Parent,
			};
			auto run = run_agent_protocol_loop(aRequest, aSink, transport);
			// The protocol loop observed the supervisor exit or ended it; either
			// way this process's own guard lock is released only after that.
			static_cast<void>(WaitForSingleObject(process.get(), static_cast<DWORD>(reap_timeout.count() * 1'000)));
			ownerGuard.reset();
			return run;
		}
	} // namespace

	auto run_windows_agent_process(
		const application::agent_launch_request& aRequest,
		const int                                aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink,
		const cancellation_probe&                aCancelled)
		-> application::agent_run_result
	{
		auto result           = application::agent_run_result{ .Session = aRequest.Session };
		auto executable       = utf16(aRequest.Executable);
		auto workingDirectory = utf16(aRequest.WorkingDirectory);
		auto environment      = environment_block(aRequest.Environment);
		if (
			!executable || executable->empty() || !workingDirectory
			|| workingDirectory->empty() || !environment) {
			result.StandardError = "agent process input is not valid UTF-8";
			return result;
		}
		auto command = command_line(*executable, aRequest.Arguments);
		if (!command) {
			result.StandardError = "agent process arguments are not valid UTF-8";
			return result;
		}
		if (aCancelled()) {
			result.Outcome = application::agent_run_outcome::cancelled;
			return result;
		}

		auto input  = make_pipe(true);
		auto output = make_pipe(false);
		auto error  = make_pipe(false);
		if (!input || !output || !error) {
			result.StandardError = "agent private standard-I/O pipe creation failed";
			return result;
		}

		if (aRequest.WorkspaceLifetimeGuard.has_value()) {
			return run_supervised_agent_process(
				aRequest, *executable, *command, *environment, *workingDirectory,
				*input, *output, *error, aTimeoutMilliseconds, aSink, aCancelled);
		}

		auto job = unique_handle{ CreateJobObjectW(nullptr, nullptr) };
		if (!job) {
			result.StandardError = "agent containment creation failed";
			return result;
		}
		auto limits                             = JOBOBJECT_EXTENDED_LIMIT_INFORMATION{ };
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (
			SetInformationJobObject(
				job.get(),
				JobObjectExtendedLimitInformation,
				&limits,
				sizeof(limits))
			== FALSE) {
			result.StandardError = "agent containment policy configuration failed";
			return result;
		}

		auto inheritedHandles = std::array<HANDLE, 3U>{
			input->Child.get(), output->Child.get(), error->Child.get()
		};
		auto attributes = attribute_list{ };
		if (!attributes.initialize() || !attributes.set_handle_list(inheritedHandles)) {
			result.StandardError = "agent private handle-list creation failed";
			return result;
		}
		auto startup                   = STARTUPINFOEXW{ };
		startup.StartupInfo.cb         = sizeof(startup);
		startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
		startup.StartupInfo.hStdInput  = input->Child.get();
		startup.StartupInfo.hStdOutput = output->Child.get();
		startup.StartupInfo.hStdError  = error->Child.get();
		startup.lpAttributeList        = attributes.get();
		auto       processInformation  = PROCESS_INFORMATION{ };
		const auto creationFlags       = DWORD{
			CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT
			| EXTENDED_STARTUPINFO_PRESENT
		};
		const auto created = CreateProcessW(
								 executable->c_str(),
								 command->data(),
								 nullptr,
								 nullptr,
								 TRUE,
								 creationFlags,
								 environment->data(),
								 workingDirectory->c_str(),
								 &startup.StartupInfo,
								 &processInformation)
			!= FALSE;
		auto process = unique_handle{ processInformation.hProcess };
		auto thread  = unique_handle{ processInformation.hThread };
		input->Child.reset();
		output->Child.reset();
		error->Child.reset();
		if (!created) {
			result.StandardError = "agent process creation failed";
			return result;
		}

		const auto deadline = clock::now()
			+ std::chrono::milliseconds{ std::max(aTimeoutMilliseconds, 0) };
		if (aCancelled() || clock::now() >= deadline) {
			static_cast<void>(TerminateProcess(process.get(), 1U));
			static_cast<void>(WaitForSingleObject(
				process.get(), static_cast<DWORD>(reap_timeout.count() * 1'000)));
			result.Outcome = aCancelled()
				? application::agent_run_outcome::cancelled
				: application::agent_run_outcome::timeout;
			return result;
		}
		if (!assign_process_to_job(job.get(), process.get())) {
			static_cast<void>(TerminateProcess(process.get(), 1U));
			static_cast<void>(WaitForSingleObject(
				process.get(), static_cast<DWORD>(reap_timeout.count() * 1'000)));
			result.StandardError = "agent containment assignment failed before resume";
			return result;
		}
		if (aCancelled() || clock::now() >= deadline) {
			static_cast<void>(terminate_and_reap(
				job.get(),
				process.get(),
				input->Parent.get(),
				output->Parent.get(),
				error->Parent.get()));
			result.Outcome = aCancelled()
				? application::agent_run_outcome::cancelled
				: application::agent_run_outcome::timeout;
			return result;
		}
		if (ResumeThread(thread.get()) == std::numeric_limits<DWORD>::max()) {
			static_cast<void>(terminate_and_reap(
				job.get(),
				process.get(),
				input->Parent.get(),
				output->Parent.get(),
				error->Parent.get()));
			result.StandardError = "agent process resume failed";
			return result;
		}
		thread.reset();

		auto transport = windows_agent_protocol_transport{
			input->Parent,
			output->Parent,
			error->Parent,
			process.get(),
			job.get(),
			deadline,
			aCancelled,
		};
		return run_agent_protocol_loop(aRequest, aSink, transport);
	}

#if defined(BPR_BUILD_TESTING)
	void force_windows_job_assignment_failure_for_testing(
		const bool aEnabled) noexcept
	{
		ForceJobAssignmentFailure.store(aEnabled, std::memory_order_release);
	}
#endif
} // namespace bpr::adapters::agent::detail
