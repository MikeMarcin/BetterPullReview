// [[BPR-0622]]. Bundled Windows process owner, the counterpart of
// unix_agent_supervisor.cpp. It sits outside the child's job and holds, in its
// own process, everything that must outlive the application: the job with its
// completion port and its own shared lock on the workspace lifetime guard.
//
// Why a separate process (measured before choosing this design): kill-on-close
// ends the child the moment the last job handle closes, which on application
// death is before anyone can confirm the job is empty; and a LockFileEx lock is
// owned by the process that took it, so the application's lock is gone the
// instant it dies. This supervisor keeps both until the job is confirmed empty.
//
// Command line: --workspace-owner <lifetime-pipe> <report-pipe>
//   <lifetime-pipe> read end: the launch specification, then end-of-file when
//                   the owner closes it or dies.
//   <report-pipe>   write end: one int32 start result.
// Standard handles belong to the selected agent protocol and pass straight to
// the child. No Qt, repository, credential, settings or network capability is
// linked here.
#include "bpr/adapters/agent/windows_workspace_guard.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {
	using bpr::adapters::agent::detail::windows_handle;

	constexpr int   Failure                = 125;
	constexpr DWORD MaximumSpecCharacters  = 32'767U;
	constexpr DWORD SpecificationMagic     = 0x53525042U; // "BPRS"
	constexpr DWORD SpecificationVersion   = 1U;
	constexpr DWORD WorkspaceLeaseFlag     = 1U;
	constexpr DWORD EmptyConfirmationSlice = 50U;

	[[nodiscard]] auto parse_handle(const wchar_t* aText) -> HANDLE
	{
		if (aText == nullptr || *aText == L'\0') {
			return nullptr;
		}
		wchar_t*   end   = nullptr;
		const auto value = std::wcstoull(aText, &end, 10);
		if (end == nullptr || *end != L'\0' || value == 0U) {
			return nullptr;
		}
		return reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(value));
	}

	[[nodiscard]] auto read_exact(const HANDLE aPipe, void* const aBuffer, const DWORD aBytes) -> bool
	{
		auto received = DWORD{ 0U };
		while (received < aBytes) {
			auto count = DWORD{ 0U };
			if (::ReadFile(aPipe, static_cast<char*>(aBuffer) + received, aBytes - received, &count, nullptr) == FALSE || count == 0U) {
				return false;
			}
			received += count;
		}
		return true;
	}

	[[nodiscard]] auto read_string(const HANDLE aPipe, std::wstring& aValue) -> bool
	{
		auto length = DWORD{ 0U };
		if (!read_exact(aPipe, &length, sizeof(length)) || length == 0U || length > MaximumSpecCharacters) {
			return false;
		}
		aValue.assign(length, L'\0');
		return read_exact(aPipe, aValue.data(), length * sizeof(wchar_t)) && aValue.find(L'\0') == std::wstring::npos;
	}

	[[nodiscard]] auto report_start(const HANDLE aReport, const std::int32_t aError) -> bool
	{
		auto written = DWORD{ 0U };
		return ::WriteFile(aReport, &aError, sizeof(aError), &written, nullptr) != FALSE && written == sizeof(aError);
	}

	struct launch_specification
	{
		std::wstring                  Executable;
		std::wstring                  CommandLine;
		std::wstring                  GuardPath;
		std::array<std::uint64_t, 4U> GuardIdentity{ };
		bool                          WorkspaceLease = false;
	};

	[[nodiscard]] auto read_specification(const HANDLE aLifetime, launch_specification& aSpecification) -> bool
	{
		std::array<DWORD, 3U> header{ };
		if (!read_exact(aLifetime, header.data(), sizeof(header)) || header[0] != SpecificationMagic
			|| header[1] != SpecificationVersion || (header[2] & ~WorkspaceLeaseFlag) != 0U) {
			return false;
		}
		aSpecification.WorkspaceLease = (header[2] & WorkspaceLeaseFlag) != 0U;
		if (!read_string(aLifetime, aSpecification.Executable) || !read_string(aLifetime, aSpecification.CommandLine)) {
			return false;
		}
		if (aSpecification.WorkspaceLease) {
			return read_string(aLifetime, aSpecification.GuardPath)
				&& read_exact(aLifetime, aSpecification.GuardIdentity.data(), sizeof(aSpecification.GuardIdentity));
		}
		return true;
	}

	/// Blocks until the owner closes its end or dies, then signals the event.
	/// The launch specification has already been consumed, so any further read
	/// that returns is end-of-file or a broken pipe.
	void watch_owner(const HANDLE aLifetime, const HANDLE aOwnerGone)
	{
		for (;;) {
			std::array<char, 64U> discard{ };
			auto                  count = DWORD{ 0U };
			if (::ReadFile(aLifetime, discard.data(), static_cast<DWORD>(discard.size()), &count, nullptr) == FALSE || count == 0U) {
				break;
			}
		}
		static_cast<void>(::SetEvent(aOwnerGone));
	}

	/// Terminates everything in the job and returns only once the job is
	/// confirmed empty, by the completion-port message or the accounting count.
	/// The guard is not released before this returns true.
	[[nodiscard]] auto terminate_and_confirm_empty(const HANDLE aJob, const HANDLE aPort) -> bool
	{
		if (::TerminateJobObject(aJob, static_cast<UINT>(Failure)) == FALSE) {
			return false;
		}
		for (;;) {
			auto accounting = JOBOBJECT_BASIC_ACCOUNTING_INFORMATION{ };
			if (::QueryInformationJobObject(aJob, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr) == FALSE) {
				return false;
			}
			if (accounting.ActiveProcesses == 0U) {
				return true;
			}
			auto        bytes      = DWORD{ 0U };
			auto        key        = ULONG_PTR{ 0U };
			OVERLAPPED* overlapped = nullptr;
			static_cast<void>(::GetQueuedCompletionStatus(aPort, &bytes, &key, &overlapped, EmptyConfirmationSlice));
		}
	}
}

auto wmain(const int argc, wchar_t** const argv) -> int
{
	// A crash dialog in an unattended owner would hold the guard indefinitely
	// while nobody watches; failures are exit codes.
	static_cast<void>(::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX));
	if (argc != 4 || std::wstring_view{ argv[1] } != L"--workspace-owner") {
		return Failure;
	}
	const auto lifetime = windows_handle{ parse_handle(argv[2]) };
	const auto report   = windows_handle{ parse_handle(argv[3]) };
	if (!lifetime || !report || ::GetFileType(lifetime.get()) != FILE_TYPE_PIPE || ::GetFileType(report.get()) != FILE_TYPE_PIPE
		|| ::SetHandleInformation(lifetime.get(), HANDLE_FLAG_INHERIT, 0U) == FALSE
		|| ::SetHandleInformation(report.get(), HANDLE_FLAG_INHERIT, 0U) == FALSE) {
		return Failure;
	}
	auto specification = launch_specification{ };
	if (!read_specification(lifetime.get(), specification)) {
		return Failure;
	}

	auto guard = windows_handle{ };
	if (specification.WorkspaceLease) {
		guard = bpr::adapters::agent::detail::open_windows_workspace_guard(
			specification.GuardPath, specification.GuardIdentity, bpr::adapters::agent::detail::windows_guard_lock::shared);
		if (!guard) {
			static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(ERROR_LOCK_VIOLATION)));
			return Failure;
		}
	}

	// Kill-on-close covers this supervisor's own death; no breakaway limit is
	// ever set, so a descendant asking for CREATE_BREAKAWAY_FROM_JOB is refused
	// and stays inside. Nested under whatever job this supervisor inherited.
	const auto job                          = windows_handle{ ::CreateJobObjectW(nullptr, nullptr) };
	const auto port                         = windows_handle{ ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0U, 1U) };
	auto       limits                       = JOBOBJECT_EXTENDED_LIMIT_INFORMATION{ };
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	auto association                        = JOBOBJECT_ASSOCIATE_COMPLETION_PORT{ job.get(), port.get() };
	if (!job || !port
		|| ::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) == FALSE
		|| ::SetInformationJobObject(job.get(), JobObjectAssociateCompletionPortInformation, &association, sizeof(association)) == FALSE) {
		static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(::GetLastError())));
		return Failure;
	}

	// The child inherits exactly its three standard handles and nothing else:
	// not the guard, the job, the port or either owner pipe.
	const std::array<HANDLE, 3U> standard{ ::GetStdHandle(STD_INPUT_HANDLE), ::GetStdHandle(STD_OUTPUT_HANDLE), ::GetStdHandle(STD_ERROR_HANDLE) };
	// A handle list may not name one handle twice; output and error can share one.
	auto inherited = std::vector<HANDLE>{ };
	for (const auto handle : standard) {
		if (std::ranges::find(inherited, handle) == inherited.end()) {
			inherited.push_back(handle);
		}
	}
	for (const auto handle : inherited) {
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE
			|| ::SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) == FALSE) {
			static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(ERROR_INVALID_HANDLE)));
			return Failure;
		}
	}
	auto attributeSize = SIZE_T{ 0U };
	static_cast<void>(::InitializeProcThreadAttributeList(nullptr, 1U, 0U, &attributeSize));
	auto attributeBuffer           = std::vector<BYTE>(attributeSize);
	auto startup                   = STARTUPINFOEXW{ };
	startup.StartupInfo.cb         = sizeof(startup);
	startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput  = standard[0];
	startup.StartupInfo.hStdOutput = standard[1];
	startup.StartupInfo.hStdError  = standard[2];
	startup.lpAttributeList        = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(static_cast<void*>(attributeBuffer.data()));
	if (attributeSize == 0U || ::InitializeProcThreadAttributeList(startup.lpAttributeList, 1U, 0U, &attributeSize) == FALSE
		|| ::UpdateProcThreadAttribute(startup.lpAttributeList, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(), inherited.size() * sizeof(HANDLE), nullptr, nullptr) == FALSE) {
		static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(::GetLastError())));
		return Failure;
	}
	auto processInformation = PROCESS_INFORMATION{ };
	auto commandLine        = specification.CommandLine;
	// The environment block and working directory are this supervisor's own,
	// which the owner created exactly for the child.
	const auto created = ::CreateProcessW(
		specification.Executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
		CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &processInformation);
	const auto createError = ::GetLastError();
	::DeleteProcThreadAttributeList(startup.lpAttributeList);
	if (created == FALSE) {
		static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(createError)));
		return Failure;
	}
	const auto child       = windows_handle{ processInformation.hProcess };
	const auto childThread = windows_handle{ processInformation.hThread };
	if (::AssignProcessToJobObject(job.get(), child.get()) == FALSE) {
		const auto assignError = ::GetLastError();
		static_cast<void>(::TerminateProcess(child.get(), static_cast<UINT>(Failure)));
		static_cast<void>(::WaitForSingleObject(child.get(), INFINITE));
		static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(assignError)));
		return Failure;
	}
	if (::ResumeThread(childThread.get()) == std::numeric_limits<DWORD>::max()) {
		const auto resumeError = ::GetLastError();
		static_cast<void>(terminate_and_confirm_empty(job.get(), port.get()));
		static_cast<void>(report_start(report.get(), static_cast<std::int32_t>(resumeError)));
		return Failure;
	}
	if (!report_start(report.get(), 0)) {
		static_cast<void>(terminate_and_confirm_empty(job.get(), port.get()));
		return Failure;
	}

	const auto ownerGone = windows_handle{ ::CreateEventW(nullptr, TRUE, FALSE, nullptr) };
	if (!ownerGone) {
		static_cast<void>(terminate_and_confirm_empty(job.get(), port.get()));
		return Failure;
	}
	std::thread watcher{ watch_owner, lifetime.get(), ownerGone.get() };

	const std::array<HANDLE, 2U> waits{ child.get(), ownerGone.get() };
	const auto                   woke     = ::WaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE, INFINITE);
	auto                         exitCode = static_cast<DWORD>(Failure);
	if (woke == WAIT_OBJECT_0) {
		static_cast<void>(::GetExitCodeProcess(child.get(), &exitCode));
	}
	// A normal exit still ends any descendant, as the Unix group kill does.
	const auto emptied = terminate_and_confirm_empty(job.get(), port.get());
	// Closing the lifetime pipe while the watcher is inside a synchronous
	// ReadFile on it would block until that read returns, which it never does
	// while the owner lives; this supervisor would then never exit and never
	// release the child's standard handles. Cancel the read and join first.
	while (::WaitForSingleObject(watcher.native_handle(), 10U) == WAIT_TIMEOUT) {
		static_cast<void>(::CancelSynchronousIo(watcher.native_handle()));
	}
	watcher.join();
	if (!emptied) {
		return Failure;
	}
	guard.reset();
	return woke == WAIT_OBJECT_0 ? static_cast<int>(exitCode) : Failure;
}
