/// \file
/// \brief Hostile local agent fixture for protocol and process-lifecycle tests.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
	#include <process.h>
// The UCRT already supplies `_environ`, as a macro over `__p__environ()`. A
// hand-written `extern "C"` declaration redeclares that accessor without its
// import linkage, so the fixture takes the header's spelling instead.
	#define BPR_ENVIRON _environ
#else
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
		const auto escaped = json_escape(aMessage);
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
				"{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\","
				"\"type\":\"agent_message\",\"text\":\""
				+ escaped + "\"}}\n");
			break;
		case protocol::claude_stream_json:
			write_partial(
				"{\"type\":\"assistant\",\"session_id\":\"canary-session\","
				"\"message\":{\"role\":\"assistant\",\"content\":[{"
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
				"\"session_id\":\"canary-session\"}\n");
			break;
		case protocol::codex_app_server:
		case protocol::external_terminal:
			break;
		}
	}

	void finish_output(
		const protocol aProtocol, const std::string_view aTerminalStatus)
	{
		switch (aProtocol) {
		case protocol::codex_app_server:
			write_partial(
				"{\"method\":\"turn/completed\",\"params\":{"
				"\"threadId\":\"canary-thread\",\"turn\":{"
				"\"id\":\"canary-turn\",\"items\":[],\"status\":\""
				+ std::string{ aTerminalStatus } + "\"}}}\n");
			break;
		case protocol::codex_jsonl:
			if (aTerminalStatus == "failed") {
				write_partial("{\"type\":\"turn.failed\",\"error\":{}}\n");
			} else {
				write_partial(
					"{\"type\":\"turn.completed\",\"usage\":{"
					"\"input_tokens\":1,\"cached_input_tokens\":0,"
					"\"output_tokens\":1}}\n");
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
					"\"session_id\":\"canary-session\"}\n");
			}
			break;
		case protocol::external_terminal:
			break;
		}
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
				"{\"id\":1,\"result\":{\"thread\":{\"id\":\"canary-thread\"}}}\n"
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
			"{\"id\":1,\"result\":{\"thread\":{\"id\":\"canary-thread\"}}}\n");

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

	[[nodiscard]] auto spawn_descendant(
		const std::string& aExecutable, const std::string& aMarker) -> bool
	{
#if defined(_WIN32)
		return ::_spawnl(
				   _P_NOWAIT,
				   aExecutable.c_str(),
				   aExecutable.c_str(),
				   "--descendant-heartbeat",
				   aMarker.c_str(),
				   nullptr)
			!= -1;
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
} // namespace

auto main(int argc, char** argv) -> int
{
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
	const auto input    = read_input(
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
	if (has_argument(argc, argv, "--spawn-descendant")
		|| executable_mode(argv, "descendant-wait")) {
		auto marker = argument_after(argc, argv, "--spawn-descendant");
		if (marker.empty()) {
			marker = (std::filesystem::temp_directory_path()
					  / ("bpr-agent-descendant-" + std::to_string(
#if defined(_WIN32)
							 ::_getpid()
#else
							 ::getpid()
#endif
								 )))
						 .string();
			emit(protocol, "descendant-marker\t" + marker);
		}
		if (marker.empty() || !spawn_descendant(argv[0], marker)) {
			return 31;
		}
		emit(protocol, "descendant-started");
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
		auto input = std::ifstream{ aPath, std::ios::binary };
		return std::string{
			std::istreambuf_iterator<char>{ input },
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
	finish_output(protocol, terminalStatus);
	if (executable_mode(argv, "cleanup-retry-wait")) {
		const auto release = cwd / ".bpr-cleanup-retry-release";
		while (!std::filesystem::exists(release)) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
		}
	} else if (has_argument(argc, argv, "--wait-forever")
			   || executable_mode(argv, "wait")) {
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
