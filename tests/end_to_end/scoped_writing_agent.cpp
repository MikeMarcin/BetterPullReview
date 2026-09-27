#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
namespace {
	auto environment_present(const char* name) -> bool
	{
#if defined(_MSC_VER)
		char*       value = nullptr;
		std::size_t size  = 0;
		const auto  error = _dupenv_s(&value, &size, name);
		std::free(value);
		return error != 0 || size != 0;
#else
		return std::getenv(name) != nullptr;
#endif
	}
	auto escape(const std::string& s) -> std::string
	{
		std::string r;
		for (char c : s) {
			if (c == '"' || c == '\\') {
				r += '\\';
			}
			if (c == '\n') {
				r += "\\n";
				continue;
			}
			if (c == '\r') {
				r += "\\r";
				continue;
			}
			r += c;
		}
		return r;
	}
}
int main(int argc, char** argv)
{
	const std::vector<std::string> expected = { "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose", "--tools", "", "--disallowedTools", "*", "--strict-mcp-config", "--mcp-config", "{\"mcpServers\":{}}", "--setting-sources", "", "--bare", "--disable-slash-commands", "--no-session-persistence", "--settings", "{\"disableAllHooks\":true}" };
	std::vector<std::string>       actual;
	for (int i = 1; i < argc; ++i) {
		actual.emplace_back(argv[i]);
	}
	if (actual != expected || environment_present("GITHUB_TOKEN") || environment_present("GH_TOKEN")) {
		return 3;
	}
	std::string input{ std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{ } };
	if (input.size() > 100000 || input.find("PRIVATE_UNUSED_CANARY") != std::string::npos || input.find("note.count\\t0") == std::string::npos || input.find("draft.count\\t0") == std::string::npos) {
		return 4;
	}
	std::cout << "{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"fixture\",\"tools\":[],\"mcp_servers\":[]}\n"
			  << std::flush;
	if (input.find("WAIT_CANARY") != std::string::npos) {
		std::this_thread::sleep_for(std::chrono::milliseconds{ 700 });
	}
	if (input.find("EMPTY_CANARY") != std::string::npos && input.find("stop-source") == std::string::npos) {
		return 5;
	}
	std::string result = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":"Rewritten 😀\r\nexact.","notes":[]})";
	if (input.find("INVALID_CANARY") != std::string::npos) {
		result = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":"evil","notes":[],"publish":true})";
	}
	if (input.find("INSUFFICIENT_CANARY") != std::string::npos) {
		result = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"insufficient_context","reason":"Need an author explanation."})";
	}
	if (input.find("EMPTY_CANARY") != std::string::npos) {
		result = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":"Evidence-backed draft.","notes":[{"kind":"inferred","text":"Bounded source","source_refs":["pr-description"]}]})";
	}
	if (input.find("For this outline operation") != std::string::npos) {
		result = R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[{"title":"Generated decision","anchor_ref":"selected-source","related_refs":[],"reason":"PRIVATE_OUTLINE_REASON","author_question":"What should the reader check?"},{"title":"Unresolved intent","anchor_ref":"pr-description","related_refs":[],"reason":"Explain the stated purpose","author_question":"Where is the supporting code?"}]})";
	}
	const auto encoded = escape(result);
	std::cout << "{\"type\":\"assistant\",\"session_id\":\"fixture\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"" << encoded << "\"}]}}\n";
	std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"fixture\",\"is_error\":false,\"result\":\"" << encoded << "\"}\n";
}
