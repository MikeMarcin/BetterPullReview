#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
namespace {
	auto environment_present(const char* aName) -> bool
	{
#if defined(_MSC_VER)
		char*       value = nullptr;
		std::size_t size  = 0;
		const auto  error = _dupenv_s(&value, &size, name);
		std::free(value);
		return error != 0 || size != 0;
#else
		return std::getenv(aName) != nullptr;
#endif
	}
	auto escape(const std::string& aS) -> std::string
	{
		std::string r;
		for (char c : aS) {
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
	// [[BPR-1375]]. Print mode on the person's own sign-in: no `--bare`, and
	// what it skipped held off by settings. An optional `--model NAME` follows
	// `--verbose` when the profile names a model.
	const std::vector<std::string> restrictions = { "--tools", "", "--disallowedTools", "*", "--strict-mcp-config", "--mcp-config", "{\"mcpServers\":{}}", "--setting-sources", "", "--disable-slash-commands", "--no-session-persistence", "--settings",
													R"({"disableAllHooks":true,"autoMemoryEnabled":false,"enabledPlugins":{"cc-plugin-agents-md@builtin":false,"cc-plugin-telemetry@builtin":false,"cc-plugin-plugin-authoring@builtin":false}})" };
	std::vector<std::string>       expected     = { "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose" };
	std::vector<std::string>       actual;
	for (int i = 1; i < argc; ++i) {
		actual.emplace_back(argv[i]);
	}
	std::string model;
	if (actual.size() > 7U && actual[6] == "--model") {
		model = actual[7];
		expected.insert(expected.end(), { "--model", model });
	}
	expected.insert(expected.end(), restrictions.begin(), restrictions.end());
	if (actual != expected || environment_present("GITHUB_TOKEN") || environment_present("GH_TOKEN")) {
		return 3;
	}
	// [[BPR-1375]]. The request starts in an empty folder of its own, never
	// the review workspace.
	std::error_code folderError;
	const auto      folder = std::filesystem::current_path(folderError);
	if (folderError || folder.filename().string().rfind("bpr-writing-", 0) != 0 || std::filesystem::directory_iterator{ folder, folderError } != std::filesystem::directory_iterator{ } || folderError) {
		return 6;
	}
	std::string input{ std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{ } };
	if (input.size() > 100000 || input.find("PRIVATE_UNUSED_CANARY") != std::string::npos || input.find("note.count\\t0") == std::string::npos || input.find("draft.count\\t0") == std::string::npos) {
		return 4;
	}
	// Claude 2.1.296 can write a `ui_invalidate` system frame before init.
	std::cout << "{\"type\":\"system\",\"subtype\":\"ui_invalidate\",\"session_id\":\"fixture\"}\n";
	if (input.find("PLUGIN_CANARY") != std::string::npos) {
		std::cout << "{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"fixture\",\"tools\":[],\"mcp_servers\":[],\"plugins\":[{\"name\":\"late-builtin\"}]}\n";
	} else {
		std::cout << "{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"fixture\",\"tools\":[],\"mcp_servers\":[],\"plugins\":[],\"model\":\"" << (model.empty() ? "default" : model) << "\"}\n";
	}
	std::cout << std::flush;
	if (input.find("SIGNED_OUT_CANARY") != std::string::npos) {
		std::cout << R"({"type":"assistant","message":{"id":"signed-out","model":"<synthetic>","role":"assistant","content":[{"type":"text","text":"Not logged in · Please run /login"}]},"session_id":"fixture","error":"authentication_failed","is_api_error_message":true})" << '\n'
				  << R"({"type":"result","subtype":"success","is_error":true,"session_id":"fixture","result":"Not logged in · Please run /login"})" << '\n'
				  << std::flush;
		return 1;
	}
	if (input.find("WAIT_CANARY") != std::string::npos) {
		std::this_thread::sleep_for(std::chrono::milliseconds{ 700 });
	}
	if (input.find("EMPTY_CANARY") != std::string::npos && input.find("stop-source") == std::string::npos) {
		return 5;
	}
	if (input.find("PROCESS_EXIT_CANARY") != std::string::npos) {
		std::_Exit(17);
	}
	if (input.find("TOOL_USE_CANARY") != std::string::npos) {
		std::cout << R"({"type":"assistant","session_id":"fixture","message":{"role":"assistant","content":[{"type":"tool_use","name":"Bash","input":{"command":"PRIVATE_WIRE_CANARY"}}]}})" << '\n'
				  << std::flush;
		return 0;
	}
	if (input.find("UTF8_CANARY") != std::string::npos) {
		std::cout << "\xff\n"
				  << std::flush;
		return 0;
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
	if (!model.empty()) {
		result = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":"Model )" + model + R"(.","notes":[]})";
	}
	if (input.find("OVERSIZE_CANARY") != std::string::npos) {
		result = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":")" + std::string(40000, 'x') + R"(","notes":[]})";
	}
	const auto encoded = escape(result);
	std::cout << "{\"type\":\"assistant\",\"session_id\":\"fixture\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"" << encoded << "\"}]}}\n";
	std::cout << "{\"type\":\"rate_limit_event\",\"session_id\":\"fixture\",\"rate_limit_info\":{}}\n";
	std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"fixture\",\"is_error\":false,\"result\":\"" << encoded << "\"}\n";
}
