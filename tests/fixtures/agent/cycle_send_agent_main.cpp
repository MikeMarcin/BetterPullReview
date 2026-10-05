/// \file
/// \brief [[BPR-1180]]. A stand-in agent harness for Send to Agent courses.
/// \details Launched by the real agent adapter as the profile's executable,
/// copied by the course into a root of its own. It writes what it was given
/// beside itself (`send-N.args`, `send-N.stdin`, the first free N), reads
/// the round it was sent the way a recipient would (cycle, round, canonical
/// digest and items from the readable document) and answers in the
/// protocol its arguments name: Claude stream JSON (`-p --input-format
/// stream-json`, continuing `--resume ID`) or plain text. The round's own text
/// chooses the answer: `SEND_STOP_CANARY` ends it after acknowledging, with
/// no turn; `SEND_PROSE_CANARY` replies in prose; otherwise it replies with a
/// structured response to that round. It reads no environment, runs no
/// model and reaches no network.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif

namespace {
	/// Writes `aBytes` to a file that did not exist; false when it did.
	auto write_new(const std::filesystem::path& aPath, const std::string& aBytes) -> bool
	{
#if defined(_WIN32)
		// MSVC refuses fopen (C4996); the wide path also keeps a non-ANSI root.
		std::FILE* file = nullptr;
		if (_wfopen_s(&file, aPath.c_str(), L"wbx") != 0) {
			file = nullptr;
		}
#else
		auto* const file = std::fopen(aPath.string().c_str(), "wbx");
#endif
		if (file == nullptr) {
			return false;
		}
		const auto written = std::fwrite(aBytes.data(), 1U, aBytes.size(), file);
		return std::fclose(file) == 0 && written == aBytes.size();
	}

	auto json_escape(const std::string_view aText) -> std::string
	{
		auto escaped = std::string{ };
		for (const char symbol : aText) {
			if (symbol == '"' || symbol == '\\') {
				escaped += '\\';
				escaped += symbol;
			} else if (symbol == '\n') {
				escaped += "\\n";
			} else if (symbol == '\r') {
				escaped += "\\r";
			} else if (symbol == '\t') {
				escaped += "\\t";
			} else {
				escaped += symbol;
			}
		}
		return escaped;
	}

	/// The text of the one user message Claude stream JSON frames, unescaped.
	auto user_text(const std::string_view aFrame) -> std::string
	{
		constexpr auto marker = std::string_view{ "\"text\":\"" };
		const auto     start  = aFrame.find(marker);
		auto           text   = std::string{ };
		if (start == std::string_view::npos) {
			return text;
		}
		for (auto at = start + marker.size(); at < aFrame.size(); ++at) {
			const auto symbol = aFrame[at];
			if (symbol == '"') {
				break;
			}
			if (symbol != '\\' || at + 1U == aFrame.size()) {
				text += symbol;
				continue;
			}
			const auto next = aFrame[++at];
			if (next == 'n') {
				text += '\n';
			} else if (next == 'r') {
				text += '\r';
			} else if (next == 't') {
				text += '\t';
			} else if (next == 'b') {
				text += '\b';
			} else if (next == 'f') {
				text += '\f';
			} else if (next == 'u' && at + 4U < aFrame.size()) {
				text += static_cast<char>(std::stoi(std::string{ aFrame.substr(at + 1U, 4U) }, nullptr, 16));
				at += 4U;
			} else {
				text += next;
			}
		}
		return text;
	}

	/// What follows `aPrefix` on its line, up to `aEnd`.
	auto after(const std::string_view aText, const std::string_view aPrefix, const std::string_view aEnd) -> std::string
	{
		const auto start = aText.find(aPrefix);
		if (start == std::string_view::npos) {
			return { };
		}
		const auto from = start + aPrefix.size();
		const auto end  = aText.find(aEnd, from);
		return std::string{ aText.substr(from, end == std::string_view::npos ? std::string_view::npos : end - from) };
	}

	/// A structured response to the round the document is.
	auto response_to(const std::string_view aDocument) -> std::string
	{
		const auto title  = after(aDocument, "# Review Cycle ", "\n");
		const auto dash   = title.find(" \xE2\x80\x94 ");
		const auto cycle  = title.substr(0U, dash);
		const auto round  = dash == std::string::npos ? std::string{ } : title.substr(dash + 5U);
		const auto digest = after(aDocument, "Canonical round SHA-256: `", "`");
		auto       items  = std::string{ };
		for (auto at = aDocument.find("\n## I"); at != std::string_view::npos; at = aDocument.find("\n## I", at + 1U)) {
			// An item's heading, `## I<n> — item revision <r>`; not "## Included source context".
			if (at + 6U >= aDocument.size() || aDocument[at + 5U] < '0' || aDocument[at + 5U] > '9') {
				continue;
			}
			const auto line     = after(aDocument.substr(at), "\n## ", "\n");
			const auto id       = line.substr(0U, line.find(' '));
			const auto revision = line.substr(line.rfind(' ') + 1U);
			items += std::string{ items.empty() ? "" : "," } + R"({"item_id":")" + id + R"(","item_revision":")" + revision
				+ R"(","disposition":"addressed","body":"Done as asked.","evidence":[{"kind":"text","value":"Fixture reply; nothing was run."}]})";
		}
		return R"({"schema":"bpr.cycle.response.v1","response_id":"FIXTURE-)" + round + R"(","cycle_id":")" + cycle + R"(","round_id":")" + round + R"(","round_payload_sha256":")" + digest
			+ R"(","claimed_author":{"display_name":"Fixture agent","kind":"agent","provider":null,"model":null,"effort":null},"result_source_revision_id":null,"items":[)" + items + "]}";
	}
}

auto main(const int aArgc, char** aArgv) -> int
{
#if defined(_WIN32)
	static_cast<void>(_setmode(_fileno(stdin), _O_BINARY));
	static_cast<void>(_setmode(_fileno(stdout), _O_BINARY));
#endif
	auto arguments = std::vector<std::string>{ };
	auto joined    = std::string{ };
	for (auto index = 1; index < aArgc; ++index) {
		arguments.emplace_back(aArgv[index]);
		joined += (index == 1 ? "" : "\n") + arguments.back();
	}
	const auto input  = std::string{ std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{ } };
	const auto beside = std::filesystem::path{ aArgv[0] }.parent_path();
	auto       number = 1;
	while (!write_new(beside / ("send-" + std::to_string(number) + ".stdin"), input)) {
		if (++number > 64) {
			return 70;
		}
	}
	if (!write_new(beside / ("send-" + std::to_string(number) + ".args"), joined)) {
		return 71;
	}

	const auto claude = !arguments.empty() && arguments.front() == "-p";
	const auto text   = claude ? user_text(input) : input;
	auto       reply  = text.find("SEND_PROSE_CANARY") != std::string::npos ? std::string{ "I changed it as asked; the tests pass." } : "```json\n" + response_to(text) + "\n```";
	if (!claude) {
		if (text.find("SEND_STOP_CANARY") != std::string::npos) {
			return 9;
		}
		std::cout << reply << '\n'
				  << std::flush;
		return 0;
	}
	auto session = "fixture-session-" + std::to_string(number);
	for (std::size_t index = 0; index + 1U < arguments.size(); ++index) {
		if (arguments[index] == "--resume") {
			session = arguments[index + 1U];
		}
	}
	std::cout << R"({"type":"system","subtype":"init","session_id":")" << session << R"(","model":"fixture-model"})" << '\n'
			  << std::flush;
	if (text.find("SEND_STOP_CANARY") != std::string::npos) {
		return 9;
	}
	std::cout << R"({"type":"assistant","session_id":")" << session << R"(","message":{"role":"assistant","model":"fixture-model","content":[{"type":"text","text":")" << json_escape(reply)
			  << R"("}]}})" << '\n';
	std::cout << R"({"type":"result","subtype":"success","is_error":false,"session_id":")" << session << R"(","result":"done"})" << '\n'
			  << std::flush;
	return 0;
}
