#include "windows_agent_process.hpp"

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
			const std::wstring& aExecutable,
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
			auto attributes            = SECURITY_ATTRIBUTES{ };
			attributes.nLength         = sizeof(attributes);
			attributes.bInheritHandle  = TRUE;
			attributes.lpSecurityDescriptor = nullptr;
			auto readHandle  = HANDLE{ nullptr };
			auto writeHandle = HANDLE{ nullptr };
			if (
				CreatePipe(&readHandle, &writeHandle, &attributes, 0U)
				== FALSE) {
				return std::nullopt;
			}
			auto read  = unique_handle{ readHandle };
			auto write = unique_handle{ writeHandle };
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

		enum class write_status : std::uint8_t {
			success,
			cancelled,
			timeout,
			process_exited,
			failed,
		};

		[[nodiscard]] auto write_all(
			const HANDLE              aPipe,
			const HANDLE              aProcess,
			const std::string_view    aBytes,
			const clock::time_point   aDeadline,
			const cancellation_probe& aCancelled) -> write_status
		{
			auto completed = std::atomic_bool{ false };
			auto succeeded = std::atomic_bool{ false };
			auto writer    = std::jthread{ [&] {
				auto offset = std::size_t{ 0U };
				while (offset < aBytes.size()) {
					const auto requested = static_cast<DWORD>(std::min<std::size_t>(
						aBytes.size() - offset,
						16U * 1024U));
					auto written = DWORD{ 0U };
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

			auto interrupted = write_status::failed;
			while (!completed.load(std::memory_order_acquire)) {
				if (aCancelled()) {
					interrupted = write_status::cancelled;
					break;
				}
				if (clock::now() >= aDeadline) {
					interrupted = write_status::timeout;
					break;
				}
				if (WaitForSingleObject(aProcess, 0U) == WAIT_OBJECT_0) {
					interrupted = write_status::process_exited;
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
			if (interrupted != write_status::failed) {
				return interrupted;
			}
			return succeeded.load(std::memory_order_acquire)
				? write_status::success
				: write_status::failed;
		}

		enum class pipe_read_status : std::uint8_t {
			no_data,
			data,
			closed,
			failed,
		};

		[[nodiscard]] auto read_available(
			const HANDLE aPipe, std::array<char, 16U * 1024U>& aBuffer)
			-> std::pair<pipe_read_status, std::size_t>
		{
			auto available = DWORD{ 0U };
			if (
				PeekNamedPipe(aPipe, nullptr, 0U, nullptr, &available, nullptr)
				== FALSE) {
				return GetLastError() == ERROR_BROKEN_PIPE
					? std::pair{ pipe_read_status::closed, std::size_t{ 0U } }
					: std::pair{ pipe_read_status::failed, std::size_t{ 0U } };
			}
			if (available == 0U) {
				return { pipe_read_status::no_data, 0U };
			}
			const auto requested = static_cast<DWORD>(
				std::min<std::size_t>(aBuffer.size(), available));
			auto read = DWORD{ 0U };
			if (
				ReadFile(aPipe, aBuffer.data(), requested, &read, nullptr) == FALSE) {
				return GetLastError() == ERROR_BROKEN_PIPE
					? std::pair{ pipe_read_status::closed, std::size_t{ 0U } }
					: std::pair{ pipe_read_status::failed, std::size_t{ 0U } };
			}
			return { pipe_read_status::data, read };
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

		[[nodiscard]] auto protocol_result(
			const application::agent_launch_request& aRequest,
			const application::agent_output_sink&    aSink,
			const cancellation_probe&                aCancelled,
			const clock::time_point                  aDeadline,
			unique_handle&                           aInput,
			unique_handle&                           aOutput,
			unique_handle&                           aError,
			const HANDLE                             aProcess,
			const HANDLE                             aJob)
			-> application::agent_run_result
		{
			auto result = application::agent_run_result{ .Session = aRequest.Session };
			auto decoder = application::agent_protocol_decoder{ aRequest.Protocol };
			auto sequence = std::uint64_t{ 0U };
			auto protocolFailure
				= std::optional<application::agent_protocol_failure>{ };
			auto forcedOutcome
				= std::optional<application::agent_run_outcome>{ };
			auto appServerPhase = std::uint8_t{ 0U };
			const auto isAppServer
				= aRequest.Protocol == core::agent_protocol::codex_app_server;

			const auto set_write_failure = [&](const write_status aStatus) {
				switch (aStatus) {
				case write_status::success:
					return;
				case write_status::cancelled:
					forcedOutcome = application::agent_run_outcome::cancelled;
					return;
				case write_status::timeout:
					forcedOutcome = application::agent_run_outcome::timeout;
					return;
				case write_status::process_exited:
				case write_status::failed:
					protocolFailure
						= application::agent_protocol_failure::malformed_frame;
					return;
				}
			};

			set_write_failure(write_all(
				aInput.get(),
				aProcess,
				aRequest.Input,
				aDeadline,
				aCancelled));
			if (!isAppServer) {
				aInput.reset();
			}

			auto publish = [&](const application::agent_protocol_event& aMessage) {
				auto event = application::agent_stream_event{
					.Session  = aRequest.Session,
					.Sequence = sequence++,
					.Text     = aMessage.Text,
				};
				result.Events.push_back(event);
				if (aSink) {
					try {
						aSink(event);
					} catch (...) {
						return false;
					}
				}
				return true;
			};

			auto outputBuffer = std::array<char, 16U * 1024U>{ };
			auto errorBuffer  = std::array<char, 16U * 1024U>{ };
			auto drainError   = [&] {
				while (!protocolFailure) {
					const auto [status, count]
						= read_available(aError.get(), errorBuffer);
					if (
						status == pipe_read_status::no_data
						|| status == pipe_read_status::closed) {
						return;
					}
					if (status == pipe_read_status::failed) {
						protocolFailure
							= application::agent_protocol_failure::malformed_frame;
						return;
					}
					const auto remaining = application::maximum_agent_stderr_bytes
						- result.StandardError.size();
					if (count > remaining) {
						protocolFailure
							= application::agent_protocol_failure::output_too_large;
						return;
					}
					result.StandardError.append(errorBuffer.data(), count);
				}
			};

			auto drainOutput = [&] {
				const auto [status, count]
					= read_available(aOutput.get(), outputBuffer);
				if (status == pipe_read_status::failed) {
					protocolFailure
						= application::agent_protocol_failure::malformed_frame;
					return false;
				}
				if (status != pipe_read_status::data) {
					return false;
				}
				const auto decoded = decoder.push(
					std::string_view{ outputBuffer.data(), count });
				if (!decoded) {
					protocolFailure = decoded.error();
					return false;
				}
				for (const auto& message : *decoded) {
					if (message.Kind == application::agent_protocol_event_kind::output) {
						if (!publish(message)) {
							protocolFailure
								= application::agent_protocol_failure::malformed_frame;
							return false;
						}
						continue;
					}
					const auto expectedPhase
						= message.Kind
							== application::agent_protocol_event_kind::initialization_complete
						? 0U
						: 1U;
					if (!isAppServer || appServerPhase != expectedPhase) {
						protocolFailure
							= application::agent_protocol_failure::malformed_frame;
						return false;
					}
					const auto followup = application::build_agent_protocol_followup(
						aRequest, message);
					if (!followup) {
						protocolFailure = followup.error();
						return false;
					}
					set_write_failure(write_all(
						aInput.get(),
						aProcess,
						*followup,
						aDeadline,
						aCancelled));
					if (forcedOutcome || protocolFailure) {
						return false;
					}
					const auto acknowledged
						= decoder.acknowledge_followup(message.Kind);
					if (!acknowledged) {
						protocolFailure = acknowledged.error();
						return false;
					}
					++appServerPhase;
					if (appServerPhase == 2U) {
						aInput.reset();
					}
				}
				return true;
			};

			while (!protocolFailure && !forcedOutcome) {
				drainError();
				drainOutput();
				if (protocolFailure || forcedOutcome) {
					break;
				}
				if (aCancelled()) {
					forcedOutcome = application::agent_run_outcome::cancelled;
					break;
				}
				if (clock::now() >= aDeadline) {
					forcedOutcome = application::agent_run_outcome::timeout;
					break;
				}
				if (WaitForSingleObject(aProcess, 0U) == WAIT_OBJECT_0) {
					break;
				}
				static_cast<void>(WaitForSingleObject(
					aProcess, static_cast<DWORD>(io_poll_interval.count())));
			}

			aInput.reset();
			auto exitCode = DWORD{ 0U };
			const auto processExited
				= WaitForSingleObject(aProcess, 0U) == WAIT_OBJECT_0;
			if (processExited) {
				static_cast<void>(GetExitCodeProcess(aProcess, &exitCode));
			}
			const auto reaped = terminate_and_reap(
				aJob,
				aProcess,
				nullptr,
				aOutput.get(),
				aError.get());
			if (!reaped) {
				protocolFailure = application::agent_protocol_failure::malformed_frame;
				forcedOutcome.reset();
			}
			if (!protocolFailure && !forcedOutcome) {
				drainError();
				while (drainOutput()) {
					if (protocolFailure || forcedOutcome) {
						break;
					}
				}
			}

			if (forcedOutcome) {
				result.Events.clear();
				result.Outcome = *forcedOutcome;
				return result;
			}
			if (protocolFailure) {
				result.Events.clear();
				result.Outcome         = application::agent_run_outcome::protocol_failure;
				result.ProtocolFailure = protocolFailure;
				return result;
			}

			const auto trailing = decoder.finish();
			if (!trailing || (isAppServer && appServerPhase != 2U)) {
				result.Events.clear();
				result.Outcome = application::agent_run_outcome::protocol_failure;
				result.ProtocolFailure = trailing
					? application::agent_protocol_failure::malformed_frame
					: trailing.error();
				return result;
			}
			for (const auto& message : *trailing) {
				if (
					message.Kind != application::agent_protocol_event_kind::output
					|| !publish(message)) {
					result.Events.clear();
					result.Outcome
						= application::agent_run_outcome::protocol_failure;
					result.ProtocolFailure
						= application::agent_protocol_failure::malformed_frame;
					return result;
				}
			}

			result.ExitCode = static_cast<int>(exitCode);
			if (!processExited || exitCode != 0U) {
				result.Events.clear();
				result.Outcome = application::agent_run_outcome::nonzero_exit;
				return result;
			}
			result.Outcome = application::agent_run_outcome::success;
			return result;
		}
	} // namespace

	auto run_windows_agent_process(
		const application::agent_launch_request& aRequest,
		const int                                aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink,
		const cancellation_probe&                aCancelled)
		-> application::agent_run_result
	{
		auto result = application::agent_run_result{ .Session = aRequest.Session };
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
		auto limits = JOBOBJECT_EXTENDED_LIMIT_INFORMATION{ };
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
		auto startup                     = STARTUPINFOEXW{ };
		startup.StartupInfo.cb           = sizeof(startup);
		startup.StartupInfo.dwFlags      = STARTF_USESTDHANDLES;
		startup.StartupInfo.hStdInput    = input->Child.get();
		startup.StartupInfo.hStdOutput   = output->Child.get();
		startup.StartupInfo.hStdError    = error->Child.get();
		startup.lpAttributeList          = attributes.get();
		auto processInformation          = PROCESS_INFORMATION{ };
		const auto creationFlags = DWORD{
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

		return protocol_result(
			aRequest,
			aSink,
			aCancelled,
			aDeadline,
			input->Parent,
			output->Parent,
			error->Parent,
			process.get(),
			job.get());
	}

#if defined(BPR_BUILD_TESTING)
	void force_windows_job_assignment_failure_for_testing(
		const bool aEnabled) noexcept
	{
		ForceJobAssignmentFailure.store(aEnabled, std::memory_order_release);
	}
#endif
} // namespace bpr::adapters::agent::detail
