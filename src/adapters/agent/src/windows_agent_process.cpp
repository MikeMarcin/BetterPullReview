#include "windows_agent_process.hpp"

#include "agent_protocol_loop.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
				std::array<HANDLE, 3U>& aHandles) -> bool
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

		[[nodiscard]] auto write_all(
			const HANDLE              aPipe,
			const HANDLE              aProcess,
			const std::string_view    aBytes,
			const clock::time_point   aDeadline,
			const cancellation_probe& aCancelled) -> agent_protocol_write_status
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
				const cancellation_probe& aCancelled)
				: Input{ aInput }
				, Output{ aOutput }
				, Error{ aError }
				, Process{ aProcess }
				, Job{ aJob }
				, Deadline{ aDeadline }
				, Cancelled{ aCancelled }
			{
			}

			[[nodiscard]] auto write(const std::string_view aBytes)
				-> agent_protocol_write_status override
			{
				return write_all(
					Input.get(), Process, aBytes, Deadline, Cancelled);
			}

			void close_input() noexcept override
			{
				Input.reset();
			}

			[[nodiscard]] auto read_standard_output(std::span<char> aBuffer)
				-> agent_protocol_pipe_read_result override
			{
				return read_available(Output.get(), OutputBatchRemaining, aBuffer);
			}

			[[nodiscard]] auto read_standard_error(std::span<char> aBuffer)
				-> agent_protocol_pipe_read_result override
			{
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
			std::size_t               OutputBatchRemaining = 0U;
			std::size_t               ErrorBatchRemaining  = 0U;
		};
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
