#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>
namespace {
	auto environment_present(const char* aName) -> bool
	{
#if defined(_MSC_VER)
		char*       value = nullptr;
		std::size_t size  = 0;
		const auto  error = _dupenv_s(&value, &size, aName);
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
			if (c == '\t') {
				r += "\\t";
				continue;
			}
			r += c;
		}
		return r;
	}
	const std::string writing_settings = R"({"disableAllHooks":true,"autoMemoryEnabled":false,"enabledPlugins":{"cc-plugin-agents-md@builtin":false,"cc-plugin-telemetry@builtin":false,"cc-plugin-plugin-authoring@builtin":false}})";
	auto              read_only_restrictions() -> const std::vector<std::string>&
	{
		static const std::vector<std::string> restrictions = { "--tools", "Read,Grep,Glob,Bash", "--allowedTools",
															   "Read,Grep,Glob,Bash(rg:*),Bash(grep:*),Bash(ls:*),Bash(cat:*),Bash(head:*),Bash(tail:*),Bash(wc:*),Bash(nl:*),Bash(find:*),Bash(git log:*),Bash(git show:*),Bash(git blame:*),Bash(git diff:*),Bash(git grep:*),Bash(git ls-files:*)",
															   "--disallowedTools", "Edit,Write,MultiEdit,NotebookEdit,WebFetch,WebSearch,Task", "--permission-mode", "default", "--strict-mcp-config", "--mcp-config", "{\"mcpServers\":{}}",
															   "--setting-sources", "", "--disable-slash-commands", "--no-session-persistence", "--settings", writing_settings };
		return restrictions;
	}
	void emit_tool(const std::string& aId, const std::string& aName, const std::string& aInput, const std::string& aResult)
	{
		std::cout << R"({"type":"assistant","session_id":"fixture","message":{"id":"msg-)" << aId << R"(","role":"assistant","content":[{"type":"tool_use","id":")" << aId << R"(","name":")" << aName << R"(","input":)" << aInput << "}]}}\n";
		std::cout << R"({"type":"user","session_id":"fixture","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":")" << aId << R"(","content":")" << escape(aResult) << "\"}]}}\n"
				  << std::flush;
	}
	/// [[BPR-1900]]. A drafting run in the review's workspace: it really looks
	/// at the files there, says what it searched and read the way Claude Code
	/// does (tool uses and their results), and answers. Run as the `claude` in
	/// `attended-drafting-claude` it takes a few seconds, so a look sees it
	/// running in the background.
	auto draft_in_workspace(const std::string& aModel, const bool aSlow, std::string& aInput) -> std::optional<int>
	{
		if (!environment_present("CLAUDE_CODE_DISABLE_AUTO_MEMORY")) {
			return 7;
		}
		std::error_code error;
		const auto      folder = std::filesystem::current_path(error);
		if (error || folder.filename().string().rfind("bpr-writing-", 0) == 0) {
			return 6;
		}
		aInput.assign(std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{ });
		const auto& input = aInput;
		if (input.size() > 400000 || input.find("note.count\\t0") == std::string::npos) {
			return 4;
		}
		// The workspace's files, and which of them the request names: the
		// pull request's.
		struct file
		{
			std::string Relative;
			std::size_t Lines = 0U;
		};
		std::vector<file> named;
		std::size_t       seen = 0U;
		for (auto it = std::filesystem::recursive_directory_iterator{ folder, std::filesystem::directory_options::none, error }; !error && it != std::filesystem::recursive_directory_iterator{ } && seen < 4000U; it.increment(error)) {
			++seen;
			if (it->path().filename() == ".git") {
				it.disable_recursion_pending();
				continue;
			}
			if (!it->is_regular_file(error) || it->is_symlink(error)) {
				continue;
			}
			const auto relative = std::filesystem::relative(it->path(), folder, error).generic_string();
			if (relative.empty() || input.find(relative) == std::string::npos) {
				continue;
			}
			std::ifstream stream{ it->path(), std::ios::binary };
			std::string   text{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
			auto          lines = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
			if (!text.empty() && text.back() != '\n') {
				++lines;
			}
			if (lines > 0U) {
				named.push_back({ relative, lines });
			}
		}
		// Code before documents, as a reader would visit them.
		std::ranges::sort(named, [](const file& aLeft, const file& aRight) {
			const auto code = [](const std::string& aPath) { return aPath.ends_with(".cpp") || aPath.ends_with(".hpp") || aPath.ends_with(".h"); };
			return code(aLeft.Relative) != code(aRight.Relative) ? code(aLeft.Relative) : aLeft.Relative < aRight.Relative;
		});
		std::cout << R"({"type":"system","subtype":"init","session_id":"fixture","tools":["Read","Grep","Glob","Bash"],"mcp_servers":[],"plugins":[],"model":")" << (aModel.empty() ? "default" : aModel) << "\"}\n"
				  << std::flush;
		const auto pause = [aSlow] {
			if (aSlow) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 900 });
			}
		};
		pause();
		const auto first = named.empty() ? std::string{ "." } : named.front().Relative;
		emit_tool("t1", "Grep", R"({"pattern":"draw","path":"."})", std::to_string(named.size()) + " files of this pull request found");
		pause();
		emit_tool("t2", "Bash", R"({"command":"rg -n \"clear|present\" )" + escape(first) + R"("})", "matches in " + first);
		pause();
		emit_tool("t3", "Read", R"({"file_path":")" + escape((folder / first).generic_string()) + R"("})", "read " + first);
		emit_tool("t4", "Bash", R"({"command":"git log --oneline -3"})", "three commits");
		if (input.find("WRITE_CANARY") != std::string::npos) {
			// A run that tries to write: the app must stop it here.
			emit_tool("t5", "Edit", R"({"file_path":"README.md","old_string":"a","new_string":"b"})", "edited");
		}
		pause();
		// Any other request is answered as a tool-free one is.
		if (input.find("bpr.tour-draft.v1") == std::string::npos) {
			return std::nullopt;
		}
		std::string result;
		{
			std::string stops;
			const char* titles[]       = { "Where the change starts", "What it calls next", "How it ends" };
			const char* explanations[] = { "Start here: this is where the new behaviour enters. Read these lines first.", "These lines carry the change through. Check that every caller sees the new order.", "This is where the change finishes. Notice what happens when it fails." };
			for (std::size_t index = 0U; index < named.size() && index < 3U; ++index) {
				const auto last = std::min<std::size_t>(named[index].Lines, 4U);
				stops += (stops.empty() ? "" : ",") + std::string{ R"({"path":")" } + escape(named[index].Relative) + R"(","first_line":1,"last_line":)" + std::to_string(last) + R"(,"title":")" + titles[index] + R"(","explanation":")" + explanations[index] + "\"}";
			}
			// One stop the core has to flag: not a file of this pull request.
			stops += (stops.empty() ? "" : ",") + std::string{ R"({"path":"src/not_in_this_pull_request.cpp","first_line":3,"last_line":9,"title":"A file it imagined","explanation":"This file is not part of the change."})" };
			result = R"({"schema":"bpr.tour-draft.v1","problem":"Two views that cleared in the same frame drew over each other, so the second view showed the first one's scene.","solution":"Each view now clears and draws its own scene between begin and present, so the frames no longer overlap.","stops":[)" + stops + "]}";
		}
		const auto encoded = escape(result);
		std::cout << R"({"type":"assistant","session_id":"fixture","message":{"id":"msg-answer","role":"assistant","content":[{"type":"text","text":")" << encoded << "\"}]}}\n";
		std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"fixture\",\"is_error\":false,\"result\":\"" << encoded << "\"}\n";
		return 0;
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
	// [[BPR-1894]]. The agent setup's Check it works asks a signed-in
	// `claude` for its status and then its version, exactly so; it sends no
	// question and no code.
	if (actual == std::vector<std::string>{ "auth", "status", "--json" }) {
		std::cout << R"({"loggedIn":true,"authMethod":"claude.ai","apiProvider":"firstParty"})" << '\n';
		return EXIT_SUCCESS;
	}
	if (actual == std::vector<std::string>{ "--version" }) {
		std::cout << "2.1.314 (Claude Code)\n";
		return EXIT_SUCCESS;
	}
	std::string model;
	if (actual.size() > 7U && actual[6] == "--model") {
		model = actual[7];
		expected.insert(expected.end(), { "--model", model });
	}
	// [[BPR-1900]]. A read-only tour drafting run: the review's workspace,
	// read, search and read-only git, nothing that writes.
	auto readOnly = expected;
	readOnly.insert(readOnly.end(), read_only_restrictions().begin(), read_only_restrictions().end());
	expected.insert(expected.end(), restrictions.begin(), restrictions.end());
	// The effort the reviewer chose rides after the model.
	auto withoutEffort = actual;
	if (const auto effort = std::ranges::find(withoutEffort, std::string{ "--effort" }); effort != withoutEffort.end() && effort + 1 != withoutEffort.end()) {
		withoutEffort.erase(effort, effort + 2);
	}
	std::string input;
	const auto  readOnlyRun = withoutEffort == readOnly && !environment_present("GITHUB_TOKEN") && !environment_present("GH_TOKEN");
	if (readOnlyRun) {
		if (const auto ended = draft_in_workspace(model, argc > 0 && std::filesystem::path{ argv[0] }.parent_path().filename() == "attended-drafting-claude", input)) {
			return *ended;
		}
	} else if (actual != expected || environment_present("GITHUB_TOKEN") || environment_present("GH_TOKEN")) {
		return 3;
	}
	// [[BPR-1896]]. A writing launch turns auto-memory off in the
	// environment too, where a wrapper's settings cannot reach it.
	if (!environment_present("CLAUDE_CODE_DISABLE_AUTO_MEMORY")) {
		return 7;
	}
	// [[BPR-1375]]. The request starts in an empty folder of its own, never
	// the review workspace; [[BPR-1900]] a read-only run in the workspace.
	std::error_code folderError;
	const auto      folder = std::filesystem::current_path(folderError);
	if (!readOnlyRun && (folderError || folder.filename().string().rfind("bpr-writing-", 0) != 0 || std::filesystem::directory_iterator{ folder, folderError } != std::filesystem::directory_iterator{ } || folderError)) {
		return 6;
	}
	if (!readOnlyRun) {
		input.assign(std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{ });
	}
	if (input.size() > 400000 || input.find("PRIVATE_UNUSED_CANARY") != std::string::npos || input.find("note.count\\t0") == std::string::npos || input.find("draft.count\\t0") == std::string::npos) {
		return 4;
	}
	// [[BPR-1892]]. Run as the `claude` in `attended-failing-claude`, every
	// writing request fails as a crashed agent does: a line on its error
	// stream and a nonzero exit, nothing on its output.
	if (argc > 0 && std::filesystem::path{ argv[0] }.parent_path().filename() == "attended-failing-claude") {
		std::cerr << "fixture: the drafting agent stopped before answering\n";
		return 1;
	}
	// Claude 2.1.296 can write a `ui_invalidate` system frame before init.
	if (readOnlyRun) {
		// The read-only run already said init and searched.
	} else if (std::cout << "{\"type\":\"system\",\"subtype\":\"ui_invalidate\",\"session_id\":\"fixture\"}\n"; input.find("PLUGIN_CANARY") != std::string::npos) {
		// [[BPR-1891]]. Claude Code 2.1.314 on a machine with managed
		// settings, which holds this builtin on; the writing run reads past it.
		std::cout << R"({"type":"system","subtype":"init","cwd":"/fixture/bpr-writing","session_id":"fixture","tools":[],"mcp_servers":[],"model":"claude-opus-5-5","permissionMode":"default","slash_commands":[],"apiKeySource":"apiKeyHelper","claude_code_version":"2.1.314","output_style":"default","agents":["claude","Explore","general-purpose","Plan","statusline-setup"],"skills":[],"plugins":[{"name":"cc-plugin-sec-default","path":"builtin","source":"cc-plugin-sec-default@builtin"}],"analytics_disabled":true,"product_feedback_disabled":true,"uuid":"fixture-init","original_cwd":"/fixture/bpr-writing","additional_directories":["/fixture/bpr-writing"],"messaging_socket_path":"/fixture/cc.sock","fast_mode_state":"off"})"
				  << '\n';
	} else if (input.find("MEMORY_CANARY") != std::string::npos) {
		// [[BPR-1896]]. Claude Code 2.1.314 through a wrapper whose own
		// `--settings` replaced this app's: auto-memory and the builtins on.
		std::cout << R"({"type":"system","subtype":"init","cwd":"/fixture/bpr-writing","session_id":"fixture","tools":[],"mcp_servers":[],"model":"claude-opus-5-5","permissionMode":"default","slash_commands":[],"apiKeySource":"apiKeyHelper","claude_code_version":"2.1.314","output_style":"default","agents":["claude","Explore","general-purpose","Plan","statusline-setup"],"skills":[],"plugins":[{"name":"cc-plugin-sec-default","path":"builtin","source":"cc-plugin-sec-default@builtin"},{"name":"cc-plugin-agents-md","path":"builtin","source":"cc-plugin-agents-md@builtin"},{"name":"cc-plugin-plugin-authoring","path":"builtin","source":"cc-plugin-plugin-authoring@builtin"}],"memory_paths":{"auto":"/fixture/home/.claude/projects/-fixture-bpr-writing/memory/"},"capabilities":[],"analytics_disabled":true,"product_feedback_disabled":true,"uuid":"fixture-init","original_cwd":"/fixture/bpr-writing","additional_directories":["/fixture/bpr-writing"],"messaging_socket_path":"/fixture/cc.sock","fast_mode_state":"off","fast_mode_disabled_reason":"not_first_party","per_turn_effort_active":false,"view_mode":"default"})"
				  << '\n';
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
	if (input.find("THINKING_CANARY") != std::string::npos) {
		// [[BPR-1899]]. Thinking turned on over this app's settings: Claude
		// Code writes each block as its own assistant frame under one id.
		std::cout << R"({"type":"assistant","session_id":"fixture","message":{"id":"thought","role":"assistant","content":[{"type":"thinking","thinking":"THOUGHT_CONTENT_CANARY","signature":"THOUGHT_SIGNATURE_CANARY"}]}})" << '\n'
				  << R"({"type":"assistant","session_id":"fixture","message":{"id":"thought","role":"assistant","content":[{"type":"redacted_thinking","data":"THOUGHT_REDACTED_CANARY"}]}})" << '\n';
	}
	std::cout << "{\"type\":\"assistant\",\"session_id\":\"fixture\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"" << encoded << "\"}]}}\n";
	std::cout << "{\"type\":\"rate_limit_event\",\"session_id\":\"fixture\",\"rate_limit_info\":{}}\n";
	std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"fixture\",\"is_error\":false,\"result\":\"" << encoded << "\"}\n";
}
