#if !defined(_WIN32)
	#error "The AppContainer launcher is Windows-only"
#endif

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <sddl.h>
#include <userenv.h>

#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <string>
#include <string_view>
#include <vector>

namespace {
	// Windows offers an unprivileged process no network namespace, so the
	// default-deny boundary is one fixed AppContainer entered with zero
	// capabilities. Without internetClient or privateNetworkClientServer the
	// Windows Filtering Platform refuses every external connect with
	// WSAEACCES, and the container token is inherited by every descendant
	// unconditionally, which is what makes a whole test process tree safe.
	constexpr auto container_name        = L"BprAutomatedTest";
	constexpr auto container_description = L"BetterPullReview default-deny automated-test network boundary";

	// Configure time has to know the container SID before any target exists,
	// so the launcher answers one exact single-argument setup query as well.
	constexpr auto setup_option = L"--bpr-appcontainer-setup";

	[[noreturn]] void fail(const std::string_view aStage, const std::string_view aDetail)
	{
		std::fprintf(
			stderr,
			"BPR_NETWORK_ISOLATION_UNAVAILABLE:%.*s:%.*s\n",
			static_cast<int>(aStage.size()),
			aStage.data(),
			static_cast<int>(aDetail.size()),
			aDetail.data());
		std::fflush(stderr);
		::ExitProcess(125);
	}

	[[noreturn]] void fail_win32(const std::string_view aStage, const DWORD aError)
	{
		char detail[32] = { };
		std::snprintf(detail, sizeof(detail), "win32=%lu", aError);
		fail(aStage, detail);
	}

	[[noreturn]] void fail_result(const std::string_view aStage, const HRESULT aResult)
	{
		char detail[32] = { };
		std::snprintf(detail, sizeof(detail), "hr=0x%08lx", static_cast<unsigned long>(aResult));
		fail(aStage, detail);
	}

	/// \brief Refuse the launch unless the Windows Firewall service is running.
	/// \details AppContainer network restrictions are enforced by the Windows
	/// Filtering Platform, which MPSSVC hosts. With that service stopped the
	/// boundary can fail open, so an unproven boundary refuses the test instead
	/// of running it unprotected.
	void require_running_firewall_service()
	{
		const auto manager = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
		if (manager == nullptr) {
			fail_win32("firewall-service", ::GetLastError());
		}
		const auto service = ::OpenServiceW(manager, L"MPSSVC", SERVICE_QUERY_STATUS);
		if (service == nullptr) {
			const auto opened = ::GetLastError();
			static_cast<void>(::CloseServiceHandle(manager));
			fail_win32("firewall-service", opened);
		}

		auto       status  = SERVICE_STATUS_PROCESS{ };
		auto       written = DWORD{ 0 };
		const auto queried = ::QueryServiceStatusEx(
			service,
			SC_STATUS_PROCESS_INFO,
			reinterpret_cast<LPBYTE>(&status),
			static_cast<DWORD>(sizeof(status)),
			&written);
		const auto error = ::GetLastError();
		static_cast<void>(::CloseServiceHandle(service));
		static_cast<void>(::CloseServiceHandle(manager));
		if (queried == FALSE) {
			fail_win32("firewall-service", error);
		}
		if (status.dwCurrentState != SERVICE_RUNNING) {
			char detail[32] = { };
			std::snprintf(detail, sizeof(detail), "state=%lu", status.dwCurrentState);
			fail("firewall-service", detail);
		}
	}

	/// \brief Return the container SID, creating the profile on first use.
	/// \details Creation is the idempotent path: an existing profile reports
	/// ERROR_ALREADY_EXISTS and the same SID is then derived from the name.
	/// The caller owns the returned SID and releases it with FreeSid.
	[[nodiscard]] auto ensure_container_sid() -> PSID
	{
		auto       sid     = PSID{ nullptr };
		const auto created = ::CreateAppContainerProfile(
			container_name,
			container_name,
			container_description,
			nullptr,
			0,
			&sid);
		if (SUCCEEDED(created)) {
			return sid;
		}
		if (created != HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
			fail_result("profile", created);
		}
		const auto derived = ::DeriveAppContainerSidFromAppContainerName(container_name, &sid);
		if (FAILED(derived)) {
			fail_result("profile", derived);
		}
		return sid;
	}

	/// \brief Print the container SID so configure time can grant it filesystem access.
	[[nodiscard]] auto report_container_sid() -> int
	{
		const auto sid  = ensure_container_sid();
		auto       text = LPWSTR{ nullptr };
		if (::ConvertSidToStringSidW(sid, &text) == FALSE) {
			const auto converted = ::GetLastError();
			static_cast<void>(::FreeSid(sid));
			fail_win32("profile-sid", converted);
		}
		auto       encoded = std::string(256, '\0');
		const auto written = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			text,
			-1,
			encoded.data(),
			static_cast<int>(encoded.size()),
			nullptr,
			nullptr);
		const auto error = ::GetLastError();
		static_cast<void>(::LocalFree(text));
		static_cast<void>(::FreeSid(sid));
		if (written <= 0) {
			fail_win32("profile-sid", error);
		}
		std::printf("BPR_APPCONTAINER_SID=%s\n", encoded.c_str());
		return 0;
	}

	[[nodiscard]] auto skip_blanks(const wchar_t* aCursor) -> const wchar_t*
	{
		while (*aCursor == L' ' || *aCursor == L'\t') {
			++aCursor;
		}
		return aCursor;
	}

	/// \brief Skip the leading token Windows itself treats as this program's name.
	/// \details The program token is quoted or blank-delimited and, unlike the
	/// arguments after it, carries no backslash escaping.
	[[nodiscard]] auto skip_program_token(const wchar_t* aCursor) -> const wchar_t*
	{
		if (*aCursor != L'"') {
			while (*aCursor != L'\0' && *aCursor != L' ' && *aCursor != L'\t') {
				++aCursor;
			}
			return aCursor;
		}
		++aCursor;
		while (*aCursor != L'\0' && *aCursor != L'"') {
			++aCursor;
		}
		return *aCursor == L'"' ? aCursor + 1 : aCursor;
	}

	/// \brief Return the caller's command line with this launcher's own name removed.
	/// \details Forwarding the original text keeps the test's arguments exactly
	/// as CTest quoted them; rebuilding a command line out of argv would have to
	/// re-quote and could change what the test actually receives.
	[[nodiscard]] auto contained_command_line() -> std::wstring
	{
		const auto* const raw = ::GetCommandLineW();
		if (raw == nullptr) {
			return { };
		}
		return std::wstring{ skip_blanks(skip_program_token(skip_blanks(raw))) };
	}

	/// \brief Return one standard handle prepared for inheritance by the child.
	[[nodiscard]] auto inheritable_standard_handle(const DWORD aStream) -> HANDLE
	{
		const auto handle = ::GetStdHandle(aStream);
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
			return nullptr;
		}
		// A stream CTest already redirected is inheritable; re-flagging it is
		// harmless, and a handle that refuses the flag simply cannot be
		// forwarded, which is not a boundary failure.
		static_cast<void>(::SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));
		return handle;
	}

	/// \brief Owns the process attribute list that carries the container token.
	class attribute_list final
	{
	public:
		attribute_list()
		{
			auto bytes = SIZE_T{ 0 };
			static_cast<void>(::InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes));
			if (bytes == 0) {
				fail_win32("attribute-list", ::GetLastError());
			}
			Storage.resize(bytes);
			List = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(Storage.data());
			if (::InitializeProcThreadAttributeList(List, 1, 0, &bytes) == FALSE) {
				fail_win32("attribute-list", ::GetLastError());
			}
		}
		attribute_list(const attribute_list&)                    = delete;
		attribute_list(attribute_list&&)                         = delete;
		auto operator=(const attribute_list&) -> attribute_list& = delete;
		auto operator=(attribute_list&&) -> attribute_list&      = delete;
		~attribute_list()
		{
			::DeleteProcThreadAttributeList(List);
		}

		/// \brief Bind the zero-capability container token to the next launch.
		/// \details The referenced capabilities stay owned by the caller and
		/// must outlive the CreateProcessW call that consumes this list.
		void adopt_security_capabilities(SECURITY_CAPABILITIES& aCapabilities)
		{
			if (
				::UpdateProcThreadAttribute(
					List,
					0,
					PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
					&aCapabilities,
					sizeof(aCapabilities),
					nullptr,
					nullptr)
				== FALSE) {
				fail_win32("security-capabilities", ::GetLastError());
			}
		}

		[[nodiscard]] auto get() const noexcept -> LPPROC_THREAD_ATTRIBUTE_LIST
		{
			return List;
		}

	private:
		std::vector<std::byte>       Storage;
		LPPROC_THREAD_ATTRIBUTE_LIST List = nullptr;
	};

	/// \brief Run one command line inside the container and return its exit code.
	/// \details A null application name keeps execvp-shaped resolution: the
	/// system searches the command line's own directory and PATH exactly as an
	/// uncontained launch would.
	[[nodiscard]] auto run_contained(const PSID aContainerSid, wchar_t* const aCommandLine) -> DWORD
	{
		// A child inherits this process's error mode. Without it a test whose
		// loader cannot find a library raises a modal dialog on the desktop
		// and the run waits on a person; with it the launch fails at once
		// with STATUS_DLL_NOT_FOUND, which is the verdict an unattended
		// runner can act on. Crash dialogs are suppressed for the same reason.
		static_cast<void>(::SetErrorMode(
			SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX));

		auto capabilities            = SECURITY_CAPABILITIES{ };
		capabilities.AppContainerSid = aContainerSid;

		auto attributes = attribute_list{ };
		attributes.adopt_security_capabilities(capabilities);

		auto startup                   = STARTUPINFOEXW{ };
		startup.StartupInfo.cb         = sizeof(startup);
		startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
		startup.StartupInfo.hStdInput  = inheritable_standard_handle(STD_INPUT_HANDLE);
		startup.StartupInfo.hStdOutput = inheritable_standard_handle(STD_OUTPUT_HANDLE);
		startup.StartupInfo.hStdError  = inheritable_standard_handle(STD_ERROR_HANDLE);
		startup.lpAttributeList        = attributes.get();

		auto process = PROCESS_INFORMATION{ };
		if (
			::CreateProcessW(
				nullptr,
				aCommandLine,
				nullptr,
				nullptr,
				TRUE,
				EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
				nullptr,
				nullptr,
				&startup.StartupInfo,
				&process)
			== FALSE) {
			fail_win32("launch", ::GetLastError());
		}
		static_cast<void>(::CloseHandle(process.hThread));
		if (::WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0) {
			fail_win32("wait", ::GetLastError());
		}
		auto exitCode = DWORD{ 0 };
		if (::GetExitCodeProcess(process.hProcess, &exitCode) == FALSE) {
			fail_win32("exit-code", ::GetLastError());
		}
		static_cast<void>(::CloseHandle(process.hProcess));
		return exitCode;
	}
} // namespace

auto wmain(const int argc, wchar_t** const argv) -> int
{
	if (argc == 2 && argv[1] != nullptr && std::wcscmp(argv[1], setup_option) == 0) {
		return report_container_sid();
	}
	if (argc < 2 || argv[1] == nullptr || argv[1][0] == L'\0') {
		fail("command", "missing");
	}
	require_running_firewall_service();
	auto commandLine = contained_command_line();
	if (commandLine.empty()) {
		fail("command", "unparsed");
	}
	const auto containerSid = ensure_container_sid();
	const auto exitCode     = run_contained(containerSid, commandLine.data());
	static_cast<void>(::FreeSid(containerSid));
	return static_cast<int>(exitCode);
}
