/// \file
/// \brief Hostile local agent fixture for protocol and process-lifecycle tests.

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
	#include <process.h>

	// windows.h supplies the base types timeapi.h assumes are declared.
	#include <windows.h>

	#include <timeapi.h>
	#include <tlhelp32.h>
	#include <winternl.h>
// The UCRT already supplies `_environ`, as a macro over `__p__environ()`. A
// hand-written `extern "C"` declaration redeclares that accessor without its
// import linkage, so the fixture takes the header's spelling instead.
	#define BPR_ENVIRON _environ
#else
	#include <fcntl.h>
	#include <sys/types.h>
	#include <unistd.h>
extern "C" char** environ;
	#define BPR_ENVIRON environ
#endif

namespace {
	enum class protocol {
		codex_app_server,
		codex_jsonl,
		claude_stream_json,
		external_terminal,
	};

	[[nodiscard]] auto has_argument(
		const int              aArgumentCount,
		char** const           aArguments,
		const std::string_view aArgument) -> bool
	{
		for (auto index = 1; index < aArgumentCount; ++index) {
			if (std::string_view{ aArguments[index] } == aArgument) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] auto argument_after(
		const int              aArgumentCount,
		char** const           aArguments,
		const std::string_view aArgument)
		-> std::string
	{
		for (auto index = 1; index + 1 < aArgumentCount; ++index) {
			if (std::string_view{ aArguments[index] } == aArgument) {
				return aArguments[index + 1];
			}
		}
		return { };
	}

	[[nodiscard]] auto selected_protocol(
		const int aArgumentCount, char** const aArguments)
		-> protocol
	{
		if (has_argument(aArgumentCount, aArguments, "app-server")) {
			return protocol::codex_app_server;
		}
		if (has_argument(aArgumentCount, aArguments, "exec")
			&& has_argument(aArgumentCount, aArguments, "--json")) {
			return protocol::codex_jsonl;
		}
		if (has_argument(aArgumentCount, aArguments, "stream-json")) {
			return protocol::claude_stream_json;
		}
		return protocol::external_terminal;
	}

	[[nodiscard]] auto json_escape(const std::string_view aValue) -> std::string
	{
		auto output = std::string{ };
		for (const auto character : aValue) {
			switch (character) {
			case '"':
				output += "\\\"";
				break;
			case '\\':
				output += "\\\\";
				break;
			case '\n':
				output += "\\n";
				break;
			case '\r':
				output += "\\r";
				break;
			case '\t':
				output += "\\t";
				break;
			default:
				output += character;
				break;
			}
		}
		return output;
	}

	[[nodiscard]] auto lower_hex(const std::string_view aValue) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       output = std::string{ };
		output.reserve(aValue.size() * 2U);
		for (const auto character : aValue) {
			const auto byte = static_cast<unsigned char>(character);
			output.push_back(digits[byte >> 4U]);
			output.push_back(digits[byte & 0x0FU]);
		}
		return output;
	}

	/// \brief FNV-1a, 64-bit, as sixteen lower-case hex digits.
	[[nodiscard]] auto fnv1a_hex(const std::string_view aValue) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       hash   = std::uint64_t{ 14'695'981'039'346'656'037ULL };
		for (const auto character : aValue) {
			hash ^= static_cast<unsigned char>(character);
			hash *= 1'099'511'628'211ULL;
		}
		auto output = std::string(16U, '0');
		for (auto index = std::size_t{ 0U }; index < output.size(); ++index) {
			output[output.size() - 1U - index] = digits[(hash >> (index * 4U)) & 0x0FU];
		}
		return output;
	}

	void write_partial(const std::string& aFrame)
	{
		for (auto offset = std::size_t{ 0U }; offset < aFrame.size(); offset += 3U) {
			std::cout.write(
				aFrame.data() + offset,
				static_cast<std::streamsize>(
					std::min<std::size_t>(3U, aFrame.size() - offset)));
			std::cout.flush();
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
	}

	void emit(const protocol aProtocol, const std::string_view aMessage)
	{
		const auto           escaped  = json_escape(aMessage);
		static std::uint64_t nextItem = 0;
		const auto           itemId   = "item-" + std::to_string(++nextItem);
		switch (aProtocol) {
		case protocol::codex_app_server:
			write_partial(
				"{\"method\":\"item/agentMessage/delta\",\"params\":{"
				"\"threadId\":\"canary-thread\",\"turnId\":\"canary-turn\","
				"\"itemId\":\"canary-item\",\"delta\":\""
				+ escaped + "\"}}\n");
			break;
		case protocol::codex_jsonl:
			write_partial(
				"{\"type\":\"item.completed\",\"item\":{\"id\":\"" + itemId + "\","
																			  "\"type\":\"agent_message\",\"text\":\""
				+ escaped + "\"}}\n");
			break;
		case protocol::claude_stream_json:
			write_partial(
				"{\"type\":\"assistant\",\"session_id\":\"canary-session\","
				"\"message\":{\"role\":\"assistant\",\"model\":\"canary-served\",\"content\":[{"
				"\"type\":\"text\",\"text\":\""
				+ escaped + "\"}]}}\n");
			break;
		case protocol::external_terminal:
			write_partial(std::string{ aMessage } + "\n");
			break;
		}
	}

	void begin_output(const protocol aProtocol)
	{
		switch (aProtocol) {
		case protocol::codex_jsonl:
			write_partial(
				"{\"type\":\"thread.started\",\"thread_id\":\"canary-thread\"}\n"
				"{\"type\":\"turn.started\"}\n");
			break;
		case protocol::claude_stream_json:
			write_partial(
				"{\"type\":\"system\",\"subtype\":\"init\","
				"\"session_id\":\"canary-session\",\"model\":\"canary-configured\",\"claude_code_version\":\"fixture-1\"}\n");
			break;
		case protocol::codex_app_server:
		case protocol::external_terminal:
			break;
		}
	}

	/// \brief The exact structured-finding payload this fixture returns.
	///
	/// \details One well-formed finding, stated in the wire shape
	/// `decode_agent_finding_payload` documents, with every identity written as
	/// a fixed repeated hex digit so a parent can compare it field for field
	/// rather than searching the output for a substring. The session is the
	/// one the parent started the run with; a mismatch is what the parent's
	/// unknown-session course deliberately provokes.
	[[nodiscard]] auto findings_payload(const std::string& aSession) -> std::string
	{
		return "{\"session\":" + aSession
			+ ",\"findings\":[{"
			  "\"provider\":1,\"severity\":\"major\",\"confidence\":\"high\","
			  "\"category\":\"correctness\",\"path\":\"src/canary.cpp\","
			  "\"lines\":{\"begin\":10,\"end\":12},"
			  "\"anchor\":{\"line\":{\"comparison\":\""
			+ std::string(64U, '5')
			+ "\",\"side\":\"right\",\"line\":11}},"
			  "\"claim\":\"canary-claim\",\"evidence\":\"canary-evidence\","
			  "\"consequence\":\"canary-consequence\","
			  "\"suggestedComment\":\"canary-suggested-comment\","
			  "\"requiredContext\":\"canary-required-context\","
			  "\"analyzedBase\":\""
			+ std::string(40U, '1') + "\",\"analyzedHead\":\""
			+ std::string(40U, '2') + "\",\"analyzedCommits\":[\""
			+ std::string(40U, '3') + "\"]}]}";
	}

	[[nodiscard]] auto findings_member(const std::string& aPayload) -> std::string
	{
		return aPayload.empty() ? std::string{ }
								: ",\"bprFindings\":" + aPayload;
	}

	void finish_output(
		const protocol         aProtocol,
		const std::string_view aTerminalStatus,
		const std::string&     aFindings)
	{
		const auto findings = findings_member(aFindings);
		switch (aProtocol) {
		case protocol::codex_app_server:
			write_partial(
				"{\"method\":\"turn/completed\",\"params\":{"
				"\"threadId\":\"canary-thread\",\"turn\":{"
				"\"id\":\"canary-turn\",\"items\":[],\"status\":\""
				+ std::string{ aTerminalStatus } + "\"" + findings + "}}}\n");
			break;
		case protocol::codex_jsonl:
			if (aTerminalStatus == "failed") {
				write_partial("{\"type\":\"turn.failed\",\"error\":{}}\n");
			} else {
				write_partial(
					"{\"type\":\"turn.completed\",\"usage\":{"
					"\"input_tokens\":1,\"cached_input_tokens\":0,"
					"\"output_tokens\":1}"
					+ findings + "}\n");
			}
			break;
		case protocol::claude_stream_json:
			if (aTerminalStatus == "failed") {
				write_partial(
					"{\"type\":\"result\",\"subtype\":\"error_during_execution\","
					"\"is_error\":true,\"session_id\":\"canary-session\"}\n");
			} else {
				write_partial(
					"{\"type\":\"result\",\"subtype\":\"success\","
					"\"is_error\":false,\"result\":\"canary-complete\","
					"\"session_id\":\"canary-session\""
					+ findings + "}\n");
			}
			break;
		case protocol::external_terminal:
			break;
		}
	}

	/// \brief Writes one complete payload-bearing transcript and nothing else.
	///
	/// \details Reads no standard input and dumps no argv, environment, or
	/// workspace state, so a parent can drive it through an ordinary pipe
	/// without first arranging a protocol conversation. External terminal has
	/// no result frame to carry a payload, so this writes the sentinel that
	/// makes the attempt visible and refusable instead of inventing framing.
	void findings_transcript(const protocol aProtocol, const std::string& aSession)
	{
		if (aProtocol == protocol::external_terminal) {
			write_partial("BPR-FINDINGS/1 " + findings_payload(aSession) + "\n");
			return;
		}
		if (aProtocol == protocol::codex_app_server) {
			// The three responses the parent's follow-up writes unlock, in the
			// order they unlock them. Written here without reading standard
			// input so the transcript can be replayed through an ordinary
			// pipe; a parent still has to deliver them one frame at a time,
			// because delivering them together is the causal violation the
			// protocol loop already refuses.
			write_partial("{\"id\":0,\"result\":{}}\n");
			write_partial(
				"{\"id\":1,\"result\":{\"model\":\"canary-reported\",\"reasoningEffort\":\"low\",\"thread\":{\"id\":\"canary-thread\"}}}\n");
			write_partial(
				"{\"id\":2,\"result\":{\"turn\":{\"id\":\"canary-turn\","
				"\"items\":[],\"status\":\"inProgress\"}}}\n");
		}
		begin_output(aProtocol);
		emit(aProtocol, "canary-complete");
		finish_output(aProtocol, "completed", findings_payload(aSession));
	}

	[[nodiscard]] auto read_input(
		const protocol aProtocol, const bool aPreemptiveAppServer) -> std::string
	{
		if (aProtocol != protocol::codex_app_server) {
			return {
				std::istreambuf_iterator<char>{ std::cin },
				std::istreambuf_iterator<char>{ },
			};
		}

		auto input = std::string{ };
		auto line  = std::string{ };
		if (!std::getline(std::cin, line)) {
			return input;
		}
		input += line + '\n';
		if (aPreemptiveAppServer) {
			const auto preemptive = std::string{
				"{\"id\":0,\"result\":{}}\n"
				"{\"id\":1,\"result\":{\"model\":\"canary-reported\",\"reasoningEffort\":\"low\",\"thread\":{\"id\":\"canary-thread\"}}}\n"
				"{\"id\":2,\"result\":{\"turn\":{\"id\":\"canary-turn\","
				"\"items\":[],\"status\":\"inProgress\"}}}\n"
				"{\"method\":\"item/agentMessage/delta\",\"params\":{"
				"\"threadId\":\"canary-thread\",\"turnId\":\"canary-turn\","
				"\"itemId\":\"canary-item\",\"delta\":\"preemptive\"}}\n"
				"{\"method\":\"turn/completed\",\"params\":{"
				"\"threadId\":\"canary-thread\",\"turn\":{"
				"\"id\":\"canary-turn\",\"items\":[],"
				"\"status\":\"completed\"}}}\n"
			};
			std::cout.write(
				preemptive.data(), static_cast<std::streamsize>(preemptive.size()));
			std::cout.flush();
			while (std::getline(std::cin, line)) {
				input += line + '\n';
			}
			return input;
		}
		write_partial("{\"id\":0,\"result\":{}}\n");

		for (auto message = 0; message < 2; ++message) {
			if (!std::getline(std::cin, line)) {
				return input;
			}
			input += line + '\n';
		}
		write_partial(
			"{\"id\":1,\"result\":{\"model\":\"canary-reported\",\"reasoningEffort\":\"low\",\"thread\":{\"id\":\"canary-thread\"}}}\n");

		if (std::getline(std::cin, line)) {
			input += line + '\n';
			write_partial(
				"{\"id\":2,\"result\":{\"turn\":{\"id\":\"canary-turn\","
				"\"items\":[],\"status\":\"inProgress\"}}}\n");
		}
		return input;
	}

	[[noreturn]] void heartbeat(const std::string& aPath)
	{
		while (true) {
			auto marker = std::ofstream{ aPath, std::ios::app | std::ios::binary };
			marker.put('.');
			marker.flush();
			std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
		}
	}

#if defined(_WIN32)
	// [[BPR-0622]]. The parent process identity, read by the parent-death course
	// only to suspend the supervisor; no liveness claim is made from it.
	[[nodiscard]] auto parent_process_id() -> unsigned long
	{
		const auto self     = ::GetCurrentProcessId();
		const auto snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0U);
		if (snapshot == INVALID_HANDLE_VALUE) {
			return 0U;
		}
		auto entry   = PROCESSENTRY32W{ };
		entry.dwSize = sizeof(entry);
		auto parent  = 0UL;
		for (auto more = ::Process32FirstW(snapshot, &entry); more != FALSE; more = ::Process32NextW(snapshot, &entry)) {
			if (entry.th32ProcessID == self) {
				parent = entry.th32ParentProcessID;
				break;
			}
		}
		static_cast<void>(::CloseHandle(snapshot));
		return parent;
	}

	// Test fixture only: the object type name of one of this process's own
	// handles, through ntdll's ObjectTypeInformation class, which never blocks.
	[[nodiscard]] auto object_type_name(const HANDLE aHandle) -> std::wstring
	{
		auto buffer = std::array<std::byte, 4096U>{ };
		auto length = ULONG{ 0U };
		if (::NtQueryObject(aHandle, ObjectTypeInformation, buffer.data(), static_cast<ULONG>(buffer.size()), &length) < 0) {
			return { };
		}
		const auto* const information = static_cast<const PUBLIC_OBJECT_TYPE_INFORMATION*>(static_cast<const void*>(buffer.data()));
		return std::wstring{ information->TypeName.Buffer, information->TypeName.Length / sizeof(wchar_t) };
	}

	[[nodiscard]] auto same_final_path(const HANDLE aHandle, const std::wstring& aPath) -> bool
	{
		auto       name   = std::wstring(32'768U, L'\0');
		const auto length = ::GetFinalPathNameByHandleW(aHandle, name.data(), static_cast<DWORD>(name.size()), FILE_NAME_NORMALIZED);
		if (length == 0U || length >= name.size()) {
			return false;
		}
		name.resize(length);
		if (name.starts_with(L"\\\\?\\")) {
			name.erase(0U, 4U);
		}
		return ::CompareStringOrdinal(name.c_str(), static_cast<int>(name.size()), aPath.c_str(), static_cast<int>(aPath.size()), TRUE) == CSTR_EQUAL;
	}

	// [[BPR-0622]]. The child-side census behind the proof that a supervised
	// agent received only its standard handles. Every process owns completion
	// ports of its own thread pool, so type alone cannot tell an inherited port;
	// the course compares these counts with an unsupervised launch of the same
	// fixture, and the workspace guard is matched by its final path.
	void probe_inherited_handles(const protocol aProtocol, const std::string& aGuardPath)
	{
		const auto                   guardPath = std::filesystem::path{ aGuardPath }.wstring();
		const std::array<HANDLE, 3U> standard{ ::GetStdHandle(STD_INPUT_HANDLE), ::GetStdHandle(STD_OUTPUT_HANDLE), ::GetStdHandle(STD_ERROR_HANDLE) };
		auto                         guard = false;
		auto                         job = 0U, port = 0U, pipe = 0U;
		for (auto value = std::uintptr_t{ 4U }; value <= 0x10000U; value += 4U) {
			const auto handle = reinterpret_cast<HANDLE>(value);
			auto       flags  = DWORD{ 0U };
			if (::GetHandleInformation(handle, &flags) == FALSE || std::ranges::find(standard, handle) != standard.end()) {
				continue;
			}
			const auto type = object_type_name(handle);
			if (type == L"Job") {
				++job;
			} else if (type == L"IoCompletion") {
				++port;
			} else if (type == L"File") {
				const auto kind = ::GetFileType(handle);
				if (kind == FILE_TYPE_PIPE) {
					++pipe;
				} else if (kind == FILE_TYPE_DISK && same_final_path(handle, guardPath)) {
					guard = true;
				}
			}
		}
		emit(aProtocol, guard ? "inherited-guard-open" : "inherited-guard-closed");
		emit(aProtocol, "handle-count-job\t" + std::to_string(job));
		emit(aProtocol, "handle-count-port\t" + std::to_string(port));
		emit(aProtocol, "handle-count-extra-pipe\t" + std::to_string(pipe));
	}

	// [[BPR-0622]]. A descendant that asks to leave its job. The supervisor's
	// job never allows breakaway, so the request is refused; the descendant is
	// then created inside the job and must die at teardown with the rest.
	[[nodiscard]] auto spawn_breakaway_descendant(
		const std::string& aExecutable, const std::string& aMarker, long& aChildId, DWORD& aRefusal) -> bool
	{
		auto command = std::wstring{ L"\"" } + std::filesystem::path{ aExecutable }.wstring() + L"\" --descendant-heartbeat \""
			+ std::filesystem::path{ aMarker }.wstring() + L"\"";
		auto startup       = STARTUPINFOW{ };
		startup.cb         = sizeof(startup);
		auto       process = PROCESS_INFORMATION{ };
		const auto created = [&](const DWORD aFlags) {
			auto line = command;
			return ::CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, aFlags, nullptr, nullptr, &startup, &process) != FALSE;
		};
		aRefusal = 0U;
		if (!created(CREATE_BREAKAWAY_FROM_JOB)) {
			aRefusal = ::GetLastError();
			if (!created(0U)) {
				return false;
			}
		}
		aChildId = static_cast<long>(process.dwProcessId);
		static_cast<void>(::CloseHandle(process.hThread));
		static_cast<void>(::CloseHandle(process.hProcess));
		return true;
	}
#endif

	[[nodiscard]] auto spawn_descendant(
		const std::string& aExecutable, const std::string& aMarker, long& aChildId) -> bool
	{
#if defined(_WIN32)
		const auto spawned = ::_spawnl(
			_P_NOWAIT,
			aExecutable.c_str(),
			aExecutable.c_str(),
			"--descendant-heartbeat",
			aMarker.c_str(),
			nullptr);
		if (spawned == -1) {
			return false;
		}
		const auto child = reinterpret_cast<HANDLE>(spawned);
		aChildId         = static_cast<long>(::GetProcessId(child));
		static_cast<void>(::CloseHandle(child));
		return aChildId != 0;
#else
		const auto child = ::fork();
		if (child == 0) {
			::execl(
				aExecutable.c_str(),
				aExecutable.c_str(),
				"--descendant-heartbeat",
				aMarker.c_str(),
				nullptr);
			std::_Exit(EXIT_FAILURE);
		}
		aChildId = child;
		return child > 0;
#endif
	}

	[[nodiscard]] auto executable_mode(
		char** const aArguments, const std::string_view aMode) -> bool
	{
		return std::filesystem::path{ aArguments[0] }
				   .filename()
				   .string()
				   .find(aMode)
			!= std::string::npos;
	}

	/// \brief The heartbeat marker of a `…-descendant-wait` run that was
	/// given no `--spawn-descendant`.
	/// \details [[BPR-1139]]. The application starts these runs with no
	/// arguments and an allowlisted environment, so the course cannot pass a
	/// marker. It chooses where the canary's copy lives: in its owner-private
	/// data root. So the marker is beside the executable, in a directory the
	/// course made and removes, when it fails as when it passes. It was
	/// `bpr-agent-descendant-<pid>` in the temporary directory, and the
	/// descendant appended to whatever was at that name, through a link.
	/// \returns Nothing when the executable's path is relative, because then
	/// it names no directory the caller chose.
	[[nodiscard]] auto default_descendant_marker(char** const aArguments) -> std::string
	{
		if (executable_mode(aArguments, "crash-descendant-wait")) {
			return (std::filesystem::current_path() / ".bpr-crash-heartbeat").string();
		}
		// [[BPR-1138]]. Windows too: the launched canary can write in the course's
		// root there, which its cancellation contract measures.
		const auto executable = std::filesystem::path{ aArguments[0] };
		if (!executable.is_absolute()) {
			return { };
		}
		return (executable.parent_path() / ".bpr-descendant-heartbeat").string();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
#if defined(_WIN32)
	// Every raw-protocol fixture in this tree selects binary standard streams
	// first: the CRT's default text mode rewrites each protocol '\n' as
	// "\r\n" on the wire, so the bytes the parent drains would not be the
	// bytes this fixture wrote (BPR-0240). Best-effort by design: some
	// invocation courses launch this fixture without every standard stream
	// attached, a stream that does not exist cannot rewrite anything, and
	// handing its -2 descriptor to `_setmode` would trip the debug CRT's
	// invalid-parameter handler.
	for (FILE* stream : { stdin, stdout, stderr }) {
		const auto descriptor = _fileno(stream);
		if (descriptor >= 0) {
			static_cast<void>(_setmode(descriptor, _O_BINARY));
		}
	}
	// The partial-frame writer sleeps one millisecond per three-byte chunk,
	// which the default ~15.6 ms timer tick stretches sixteenfold — about
	// 180 bytes a second, and the app-server exchange alone is ~15 KB, so
	// every deadline was arithmetic before it was behavior. Millisecond
	// resolution makes the pacing mean on Windows what it already means on
	// the platforms this fixture passes on; best-effort, as above
	// (BPR-0240).
	static_cast<void>(timeBeginPeriod(1U));
	// [[BPR-1259]]. Windows 11 may ignore that request from a process it
	// treats as unseen, and a canary under the agent supervisor has no window:
	// its 1 ms sleeps took the default tick, and every course timed out
	// mid-output. Asking that the resolution not be ignored keeps it.
	auto throttling        = PROCESS_POWER_THROTTLING_STATE{ };
	throttling.Version     = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	throttling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
	throttling.StateMask   = 0U;
	static_cast<void>(::SetProcessInformation(::GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling)));
#endif
	if (const auto marker = argument_after(argc, argv, "--descendant-heartbeat");
		!marker.empty()) {
		heartbeat(marker);
	}
	if (const auto marker
		= argument_after(argc, argv, "--pre-assignment-side-effect");
		!marker.empty()) {
		auto sideEffect = std::ofstream{ marker, std::ios::binary };
		sideEffect.put('r');
		sideEffect.flush();
	}

	const auto protocol = selected_protocol(argc, argv);
#if defined(_WIN32)
	if (const auto guard = argument_after(argc, argv, "--probe-inherited-handles"); !guard.empty()) {
		probe_inherited_handles(protocol, guard);
	}
#endif
#if !defined(_WIN32)
	if (const auto descriptor = argument_after(argc, argv, "--probe-inherited-fd"); !descriptor.empty()) {
		const auto value = std::stoi(descriptor);
		emit(protocol, ::fcntl(value, F_GETFD) < 0 && errno == EBADF ? "inherited-fd-closed" : "inherited-fd-open");
	}
	if (has_argument(argc, argv, "--probe-owner-channel")) {
		emit(protocol, ::fcntl(3, F_GETFD) < 0 && errno == EBADF ? "owner-channel-closed" : "owner-channel-open");
	}
#endif
	auto findingsSession = argument_after(argc, argv, "--findings-session");
	if (findingsSession.empty()) {
		findingsSession = "41";
	}
	if (has_argument(argc, argv, "--findings-transcript")) {
		findings_transcript(protocol, findingsSession);
		return EXIT_SUCCESS;
	}
	// [[BPR-0886]]. More than a pipe holds, written to stderr before the
	// request is read. A parent that writes its whole request before draining
	// anything waits on this child while this child waits on it.
	if (has_argument(argc, argv, "--stderr-before-input")) {
		std::cerr << std::string(64U * 1024U, 'e');
		std::cerr.flush();
	}
	if (argument_after(argc, argv, "--delay-input-ms") == "400") {
		std::this_thread::sleep_for(std::chrono::milliseconds{ 400 });
	}
	const auto input = read_input(
		protocol, has_argument(argc, argv, "--preemptive-app-server"));
	// Deliberately not `write_partial` and deliberately ahead of
	// `begin_output`: the whole refused exchange has to reach the pipe in one
	// write and be followed immediately by exit, so that the parent reads it in
	// the same drain that observes the child gone. Trickling it a few bytes at a
	// time would let an earlier drain see it while the child is still running,
	// which is the path every other switch already takes.
	if (has_argument(argc, argv, "--fast-remote-failure")) {
		std::cout << "{\"type\":\"thread.started\",\"thread_id\":\"canary-thread\"}\n"
					 "{\"type\":\"turn.started\"}\n"
					 "{\"type\":\"turn.failed\",\"error\":{}}\n";
		std::cout.flush();
		return EXIT_SUCCESS;
	}
	begin_output(protocol);
	// [[BPR-0886]]. This switch's request is larger than a pipe holds, and the
	// paced writer below sends three bytes a millisecond, so echoing it, and
	// its hex, would take the run to its timeout by itself. Its length and
	// digest say the child received every byte, in order.
	if (has_argument(argc, argv, "--stderr-before-input")) {
		emit(protocol, "stdin-bytes\t" + std::to_string(input.size()));
		emit(protocol, "stdin-fnv1a\t" + fnv1a_hex(input));
		emit(protocol, "canary-complete");
		finish_output(protocol, "completed", std::string{ });
		return EXIT_SUCCESS;
	}
	// Stops after a valid prefix. Every frame written decodes, so the drain
	// records nothing and the trailing decode is the first and only refusal.
	if (has_argument(argc, argv, "--truncated-exchange")) {
		return EXIT_SUCCESS;
	}
	if (has_argument(argc, argv, "--malformed-json")) {
		write_partial("{\"type\":\"turn.started\"\n");
		return EXIT_SUCCESS;
	}
	if (has_argument(argc, argv, "--malformed-frame")) {
		write_partial("{\"type\":\"wrong\"}\n");
		return EXIT_SUCCESS;
	}
	if (has_argument(argc, argv, "--oversize-frame")) {
		std::cout << "{\"type\":\"assistant\",\"text\":\""
				  << std::string(70U * 1024U, 'o') << "\"}\n";
		std::cout.flush();
		return EXIT_SUCCESS;
	}
	// Each line stays well inside the per-frame bound, so the total-output
	// ceiling is the only one this flood can reach. The parent's pipe holds far
	// less than the ceiling, so it drains and refuses while the child is still
	// writing.
	if (has_argument(argc, argv, "--stdout-flood")) {
		const auto line = std::string(32U * 1024U, 'o') + "\n";
		for (auto written = std::size_t{ 0U }; written < 1536U * 1024U;
			 written += line.size()) {
			std::cout << line;
		}
		std::cout.flush();
		return EXIT_SUCCESS;
	}
	// Retained stderr, not stdout: the parent must refuse to accumulate past
	// its own ceiling rather than growing the retained buffer without bound.
	if (has_argument(argc, argv, "--stderr-flood")) {
		const auto block = std::string(64U * 1024U, 'e');
		for (auto written = std::size_t{ 0U }; written < 1536U * 1024U;
			 written += block.size()) {
			std::cerr << block;
		}
		std::cerr.flush();
		return EXIT_SUCCESS;
	}
	if (has_argument(argc, argv, "--stderr-pressure")) {
		const auto block = std::string(4U * 1024U, 'e');
		for (auto written = std::size_t{ 0U }; written < 256U * 1024U;
			 written += block.size()) {
			std::cerr << block;
			std::cerr.flush();
		}
	}
#if defined(_WIN32)
	if (const auto marker = argument_after(argc, argv, "--spawn-breakaway-descendant"); !marker.empty()) {
		long  childId = 0;
		DWORD refusal = 0U;
		if (!spawn_breakaway_descendant(argv[0], marker, childId, refusal)) {
			return 32;
		}
		// A timed-out run publishes no events, so the answer is also a file.
		auto answer = std::ofstream{ marker + ".breakaway", std::ios::binary };
		answer << (refusal == 0U ? std::string{ "granted" } : "refused " + std::to_string(refusal));
		answer.close();
		emit(protocol, refusal == 0U ? "breakaway-granted" : "breakaway-refused\t" + std::to_string(refusal));
		emit(protocol, "descendant-id\t" + std::to_string(childId));
	}
#endif
	if (has_argument(argc, argv, "--spawn-descendant")
		|| executable_mode(argv, "descendant-wait")) {
		auto marker = argument_after(argc, argv, "--spawn-descendant");
		if (marker.empty()) {
			marker = default_descendant_marker(argv);
			if (!marker.empty()) {
				emit(protocol, "descendant-marker\t" + marker);
			}
		}
		long childId = 0;
		if (marker.empty() || !spawn_descendant(argv[0], marker, childId)) {
			return 31;
		}
		emit(protocol, "descendant-started");
		if (has_argument(argc, argv, "--report-process-id")
			|| executable_mode(argv, "crash-descendant-wait")) {
			emit(protocol, "descendant-id\t" + std::to_string(childId));
		}
	}

	for (auto index = 0; index < argc; ++index) {
		emit(protocol, "argv\t" + std::string{ argv[index] });
	}
	for (auto** entry = BPR_ENVIRON; entry != nullptr && *entry != nullptr;
		 ++entry) {
		emit(protocol, "env\t" + std::string{ *entry });
	}
	const auto cwd = std::filesystem::current_path();
	emit(protocol, "cwd\t" + cwd.string());
	const auto readFile = [](const std::filesystem::path& aPath) {
		auto stream = std::ifstream{ aPath, std::ios::binary };
		return std::string{
			std::istreambuf_iterator<char>{ stream },
			std::istreambuf_iterator<char>{ }
		};
	};
	const auto head   = readFile(cwd / ".git" / "HEAD");
	const auto config = readFile(cwd / ".git" / "config");
	emit(protocol, "head\t" + head);
	emit(
		protocol,
		"origin\t"
			+ std::string{ config.find("remote \"origin\"") == std::string::npos
							   ? "absent"
							   : "present" });
	for (const auto& [name, path] : {
			 std::pair{ "alternates", cwd / ".git" / "objects" / "info" / "alternates" },
			 std::pair{ "http-alternates", cwd / ".git" / "objects" / "info" / "http-alternates" },
			 std::pair{ "hooks", cwd / ".git" / "hooks" },
			 std::pair{ "worktrees", cwd / ".git" / "worktrees" },
		 }) {
		emit(
			protocol,
			std::string{ name } + "\t"
				+ (std::filesystem::exists(path) ? "present" : "absent"));
	}
	emit(protocol, "stdin\t" + input);
	emit(protocol, "stdin-hex\t" + lower_hex(input));
	// `std::getenv` is the only portable spelling this Qt-free fixture can use;
	// the MSVC CRT deprecates it in favour of the nonportable `_dupenv_s`. The
	// suppression covers exactly this read and nothing else in the fixture.
#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4996)
#endif
	const auto identity = std::getenv("BPR_CANARY_AGENT_IDENTITY");
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif
	emit(
		protocol,
		"identity\t" + std::string{ identity == nullptr ? "absent" : identity });

	emit(protocol, "canary-complete");
	const auto terminalStatus = has_argument(argc, argv, "--remote-failed")
		? std::string_view{ "failed" }
		: has_argument(argc, argv, "--remote-interrupted")
		? std::string_view{ "interrupted" }
		: std::string_view{ "completed" };
	finish_output(
		protocol,
		terminalStatus,
		has_argument(argc, argv, "--emit-findings")
			? findings_payload(findingsSession)
			: std::string{ });
	if (executable_mode(argv, "cleanup-retry-wait")) {
#if defined(_WIN32)
		// Windows holds a process's current directory handle without delete
		// sharing, so the course cannot rename the live workspace from under
		// this process the way POSIX permits. Vacating the cwd to the
		// workspace's parent releases that handle while this process stays
		// alive and paused; the release poll below reads the captured
		// absolute path, not the OS cwd, so it still observes the substitute
		// the course creates at the same literal path (BPR-0465).
		std::error_code vacateError;
		std::filesystem::current_path(cwd.parent_path(), vacateError);
#endif
		const auto release = cwd / ".bpr-cleanup-retry-release";
		while (!std::filesystem::exists(release)) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
		}
	} else if (has_argument(argc, argv, "--wait-forever")
			   || executable_mode(argv, "wait")) {
		if (has_argument(argc, argv, "--report-process-id")
			|| executable_mode(argv, "crash-descendant-wait")) {
#if defined(_WIN32)
			emit(protocol, "supervisor-id\t" + std::to_string(parent_process_id()));
			emit(protocol, "process-id\t" + std::to_string(::_getpid()));
#else
			emit(protocol, "supervisor-id\t" + std::to_string(::getppid()));
			emit(protocol, "process-id\t" + std::to_string(::getpid()));
#endif
		}
		while (true) {
			std::this_thread::sleep_for(std::chrono::hours{ 1 });
		}
	}
	if (identity != nullptr && std::string_view{ identity } == "denied") {
		return 23;
	}
	if (has_argument(argc, argv, "--nonzero-exit")) {
		return 17;
	}
	return EXIT_SUCCESS;
}
