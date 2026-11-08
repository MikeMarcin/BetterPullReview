#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
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
	void answer_with(const std::string& aResult)
	{
		const auto encoded = escape(aResult);
		std::cout << R"({"type":"assistant","session_id":"fixture","message":{"id":"msg-answer","role":"assistant","content":[{"type":"text","text":")" << encoded << "\"}]}}\n";
		std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"fixture\",\"is_error\":false,\"result\":\"" << encoded << "\"}\n";
	}
	/// [[BPR-1914]]. A turn of the tour conversation. A question explains
	/// the passage from the code it read. A follow-up that refers back
	/// ("that") checks that the earlier turn came back with it
	/// (`conversation_so_far` holds the first reply's words); without them it
	/// says it has nothing to go on. Asked to rewrite, it proposes a new
	/// Solution in Markdown.
	auto ask_in_workspace(const std::string& aInput, const std::string& aFolder, const std::string& aTarget, const std::function<void()>& aPause) -> int
	{
		emit_tool("a1", "Bash", R"({"command":"rg -n \"begin_frame|present\" )" + escape(aTarget) + R"("})", "3:begin_frame(); 8:present();");
		aPause();
		emit_tool("a2", "Read", R"({"file_path":")" + escape(aFolder + "/" + aTarget) + R"("})", "read " + aTarget);
		aPause();
		const auto evidence = aInput.find("Untrusted evidence JSON");
		const auto thisTurn = evidence == std::string::npos ? std::string::npos : aInput.find("this_turn", evidence);
		// The question alone: canonical JSON sorts it after the passage and
		// before tour_so_far.
		const auto asked    = thisTurn == std::string::npos ? std::string::npos : aInput.find("question", thisTurn);
		const auto after    = asked == std::string::npos ? std::string::npos : aInput.find("tour_so_far", asked);
		const auto question = asked == std::string::npos ? std::string{ } : aInput.substr(asked, after == std::string::npos ? std::string::npos : after - asked);
		// [[BPR-1941]]. For the tour chat's status look: one question fails
		// as a crashed agent does, and one is thought over for minutes, so
		// the live line can be watched and the turn stopped.
		if (question.find("under load") != std::string::npos) {
			std::cerr << "fixture: the agent stopped before answering\n";
			return 1;
		}
		if (question.find("every caller") != std::string::npos) {
			for (int second = 0; second < 600; ++second) {
				std::this_thread::sleep_for(std::chrono::seconds{ 1 });
			}
		}
		const auto rewrite   = question.find("ewrite") != std::string::npos;
		const auto continued = aInput.find("conversation_so_far") != std::string::npos && aInput.find("the window in which one frame is drawn") != std::string::npos;
		auto       reply     = std::string{ };
		auto       solution  = std::string{ };
		const auto refers    = question.find("that") != std::string::npos || question.find(" it ") != std::string::npos;
		// [[BPR-1914]] v002. A question about a stop's code: lines of it
		// quoted in this turn's passage, sorted before its question.
		const auto code = thisTurn != std::string::npos && asked != std::string::npos && aInput.find("stop_code", thisTurn) < asked;
		// [[BPR-1914]] v003. "Update this section from our chat": the part
		// rewritten, for the Solution's chat or a stop's (its number from the
		// thread's name). Without the chat's earlier turns it has nothing to
		// update from.
		const auto updateAt  = question.find("update");
		const auto update    = updateAt != std::string::npos && question.find("true", updateAt) != std::string::npos && question.find("true", updateAt) < updateAt + 12U;
		auto       stopTexts = std::string{ };
		if (update) {
			const auto stopAt = question.find("thread");
			const auto named  = stopAt == std::string::npos ? std::string::npos : question.find("stop ", stopAt);
			if (!continued && question.find("stop ") == std::string::npos) {
				reply = "There is nothing in this chat yet to update from.";
			} else if (named != std::string::npos && named < stopAt + 16U) {
				const auto number = question.substr(named + 5U, question.find_first_not_of("0123456789", named + 5U) - named - 5U);
				reply             = "I rewrote this stop's explanation from our chat.";
				stopTexts         = R"({"stop":)" + number + R"(,"explanation":")" + escape("Start here: `clear()` now runs before `draw()`, so each view starts from an empty target.\n\n- `begin_frame()` opens the frame.\n- `present()` shows it.") + R"("})";
			} else {
				reply    = "I rewrote the Solution from our chat: the views share one frame, and each clears its own target.";
				solution = "Both views still draw inside one frame, between `begin_frame()` and `present()`, but each now clears its own target first.\n\n"
						   "- `clear()` takes the view whose target it clears.\n"
						   "- `draw()` draws only that view's scene.\n\n"
						   "So neither view can draw over the other's scene.";
			}
		} else if (code) {
			reply = "Here `clear()` (`" + aTarget + ":5`) runs before `draw()` (`" + aTarget + ":6`) so each view starts from an empty target:\n\n"
																							   "```cpp\nclear();\ndraw_scene();\n```\n\n"
																							   "Swapped, `clear()` would wipe what `draw()` just drew.";
		} else if (!continued && !refers) {
			reply = "Here *between begin and present* means the window in which one frame is drawn:\n\n"
					"- `begin_frame()` opens the frame (`"
				+ aTarget + ":3`).\n"
							"- Each view then calls `clear()` and `draw()` on its own target (`"
				+ aTarget + ":4-7`).\n"
							"- `present()` shows the finished frame (`"
				+ aTarget + ":8`).\n\n"
							"Before the change both views cleared the same target inside that window, so the second view drew over the first.";
		} else if (!continued) {
			reply = "I can't see what we said before, so I don't know what \"that\" refers to.";
		} else if (rewrite) {
			reply    = "I proposed a new Solution that keeps your point from my last answer: the views share one frame, and each clears its own target. Accept, edit or discard it right here, under this thread.";
			solution = "Both views still draw inside one frame, between `begin_frame()` and `present()`, but each now clears its own target first.\n\n"
					   "- `clear()` takes the view whose target it clears.\n"
					   "- `draw()` draws only that view's scene.\n\n"
					   "So neither view can draw over the other's scene.";
		} else {
			reply = "Following on from the window in which one frame is drawn: `present()` (`" + aTarget + ":8`) is the only call that shows anything, so whatever a view clears or draws before it stays hidden until then.";
		}
		answer_with(R"({"schema":"bpr.tour-ask.v1","reply":")" + escape(reply) + R"(","problem":"","solution":")" + escape(solution) + R"(","stops":[],"stop_texts":[)" + stopTexts + "]}");
		return 0;
	}
	/// [[BPR-1900]]. The stops the drafting run proposes on the
	/// conventional-comments fixture, as JSON objects; `aSkip` leaves out
	/// those whose title it names.
	auto renderer_stops(const std::string& aSkip = { }) -> std::string
	{
		std::string stops;
		const auto  stop = [&stops, &aSkip](const std::string& aPath, const std::size_t aFirst, const std::size_t aLast, const std::string& aTitle, const std::string& aExplanation) {
			if (!aSkip.empty() && aSkip.find(aTitle) != std::string::npos) {
				return;
			}
			stops += (stops.empty() ? "" : ",") + std::string{ R"({"path":")" } + escape(aPath) + R"(","first_line":)" + std::to_string(aFirst) + R"(,"last_line":)" + std::to_string(aLast) + R"(,"title":")" + escape(aTitle) + R"(","explanation":")" + escape(aExplanation) + "\"}";
		};
		// [[BPR-1900]], owner 2026-11-06: one stop off the change on a single
		// line (it still publishes), one off it on several lines (it offers
		// Move to nearest change), and the rest on the conventional-comments
		// fixture's hunks; listed off-change first, so the proposal's own
		// order shows.
		stop("src/main.cpp", 1U, 2U, "Where the program starts", "Worth reading first because main() decides the frame size before anything draws. These lines are above the change.");
		stop("src/engine/renderer.cpp", 1U, 1U, "The header it draws with", "Worth a look because every call below comes from this header, which the change does not touch.");
		stop("src/engine/renderer.cpp", 3U, 8U, "Where the change starts", "Start here: this is where the new behaviour enters.\n\nRead `begin_frame()` first, then the `clear()` call under it.");
		stop("src/engine/renderer.h", 4U, 10U, "What it calls next", "These lines carry the change through:\n\n- `clear()` now takes the view it clears.\n- `draw()` draws only that view's scene.\n\nCheck that every caller passes its own view.");
		stop("src/main.cpp", 3U, 9U, "How it ends", "This is where the change finishes. Notice what `present()` does when a view drew nothing.");
		return stops;
	}
	/// [[BPR-1930]]. The stops of the tour so far, or the waiting ones, as the
	/// request numbers them: each one's number and path.
	/// The request reaches the agent as a JSON string inside its input
	/// stream, so the evidence's own quotes arrive escaped: `\"key\":`.
	auto quoted_key(const std::string& aKey) -> std::string { return std::string{ R"(\")" } + aKey + R"(\":)"; }
	auto quoted_value(const std::string& aInput, const std::size_t aKeyAt, const std::string& aKey) -> std::string
	{
		const auto start = aKeyAt + quoted_key(aKey).size() + 2U;
		const auto end   = aInput.find(R"(\")", start);
		return end == std::string::npos ? std::string{ } : aInput.substr(start, end - start);
	}
	auto numbered_stops(const std::string& aInput, const std::string& aKey, const std::string& aEnd) -> std::vector<std::pair<std::size_t, std::string>>
	{
		auto       found = std::vector<std::pair<std::size_t, std::string>>{ };
		const auto from  = aInput.rfind(quoted_key(aKey));
		if (from == std::string::npos) {
			return found;
		}
		const auto to = aEnd.empty() ? aInput.size() : std::min(aInput.size(), aInput.find(quoted_key(aEnd), from));
		auto       at = from;
		while ((at = aInput.find(quoted_key("number"), at)) != std::string::npos && at < to) {
			at += quoted_key("number").size();
			const auto number = static_cast<std::size_t>(std::strtoul(aInput.c_str() + at, nullptr, 10));
			const auto path   = aInput.find(quoted_key("path"), at);
			if (path == std::string::npos || path > to) {
				break;
			}
			found.emplace_back(number, quoted_value(aInput, path, "path"));
		}
		return found;
	}
	/// [[BPR-1930]]. A turn of the tour's own chat. While the draft's stops
	/// still wait, "fewer stops" revises them: the header-only stop is left
	/// out and the rest replace the waiting ones. On a tour with stops, "too
	/// many stops" or "merge" merges stops 2 and 3 and removes the stops in
	/// `src/main.cpp` outside them; "focus on X" sets the focus to X.
	auto revise_in_workspace(const std::string& aInput, const std::string& aTarget, const std::function<void()>& aPause) -> int
	{
		emit_tool("r1", "Bash", R"({"command":"rg -n \"clear|present\" )" + escape(aTarget) + R"("})", "3:begin_frame(); 8:present();");
		aPause();
		const auto thisTurn = aInput.rfind(quoted_key("this_turn"));
		const auto asked    = thisTurn == std::string::npos ? std::string::npos : aInput.find(quoted_key("question"), thisTurn);
		const auto question = asked == std::string::npos ? std::string{ } : quoted_value(aInput, asked, "question");
		auto       lower    = question;
		std::ranges::transform(lower, lower.begin(), [](const unsigned char aCharacter) { return static_cast<char>(std::tolower(aCharacter)); });
		const auto tour    = numbered_stops(aInput, "tour_so_far", "waiting_stops");
		const auto waiting = numbered_stops(aInput, "waiting_stops", "");
		auto       focus   = std::string{ };
		if (const auto on = lower.find("focus on "); on != std::string::npos) {
			focus = question.substr(on + 9U, question.find_first_of(".,;!?", on + 9U) == std::string::npos ? std::string::npos : question.find_first_of(".,;!?", on + 9U) - on - 9U);
		}
		auto reply   = std::string{ };
		auto stops   = std::string{ };
		auto removed = std::string{ };
		auto merges  = std::string{ };
		auto replace = false;
		if (tour.empty() && !waiting.empty()) {
			// The draft revised before it is accepted: it starts at the header
			// that declares the change, resize gets its own stop, and the two
			// stops off the change go. Every stop is on a hunk, so this order
			// is the order the proposal shows.
			replace         = true;
			const auto stop = [&stops](const std::string& aPath, const std::size_t aFirst, const std::size_t aLast, const std::string& aTitle, const std::string& aExplanation) {
				stops += (stops.empty() ? "" : ",") + std::string{ R"({"path":")" } + escape(aPath) + R"(","first_line":)" + std::to_string(aFirst) + R"(,"last_line":)" + std::to_string(aLast) + R"(,"title":")" + escape(aTitle) + R"(","explanation":")" + escape(aExplanation) + "\"}";
			};
			stop("src/engine/renderer.h", 4U, 10U, "What it declares", "Start here: the header declares the change.\n\n- `clear()` and `draw_scene()` now draw one view at a time.\n- `resize()` is new.");
			stop("src/engine/renderer.cpp", 3U, 8U, "Where it draws", "Each view now clears and draws its own scene between `begin_frame()` and `present()`.");
			stop("src/engine/renderer.cpp", 10U, 14U, "Where it resizes", "`resize()` keeps the size it is given, so the next frame draws at it.");
			stop("src/main.cpp", 3U, 9U, "Where it's called", "`main()` sizes the renderer once, before its first `draw()`.");
			reply = "I revised the draft before you accept it: it starts at the header that declares the change, `resize()` has a stop of its own, and the two stops off the change are gone. 4 stops wait.";
		} else if (lower.find("too many") != std::string::npos || lower.find("merge") != std::string::npos) {
			if (tour.size() >= 3U) {
				merges = R"({"stops":[2,3],"title":"Where it draws and resizes","explanation":")" + escape("One stop for the renderer's change: each view clears and draws its own scene between `begin_frame()` and `present()`, and `resize()` keeps the size the next frame draws at.") + R"("})";
			}
			for (const auto& [number, path] : tour) {
				if (path == "src/main.cpp" && number != 2U && number != 3U) {
					removed += (removed.empty() ? "" : ",") + std::to_string(number);
				}
			}
			reply = "Fewer stops: I'd merge stops 2 and 3, which read the same file, and remove the stop in `src/main.cpp`, which only calls the renderer.";
		} else {
			reply = "The tour reads well as it is. Tell me what to change: fewer stops, a merge, a better order, or a part to focus on.";
		}
		if (!focus.empty()) {
			reply += " I'll keep the tour focused on " + focus + ".";
		}
		answer_with(R"({"schema":"bpr.tour-revise.v1","reply":")" + escape(reply) + R"(","problem":"","solution":"","stops":[)" + stops + R"(],"stop_texts":[],"remove_stops":[)" + removed + R"(],"merge_stops":[)" + merges + R"(],"split_stops":[],"order":[],"focus":")" + escape(focus)
					+ R"(","replace_waiting_stops":)" + (replace ? "true" : "false") + "}");
		return 0;
	}
	/// [[BPR-1900]]. A drafting run in the review's workspace: it really looks
	/// at the files there, says what it searched and read the way Claude Code
	/// does (tool uses and their results), and answers. Run as the `claude` in
	/// `attended-drafting-claude` it takes a few seconds, so a look sees it
	/// running in the background.
	auto draft_in_workspace(const std::string& aModel, const bool aSlow, const bool aFailing, std::string& aInput) -> std::optional<int>
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
		if (input.find("bpr.tour-revise.v1") != std::string::npos) {
			return revise_in_workspace(input, first, pause);
		}
		if (input.find("bpr.tour-ask.v1") != std::string::npos) {
			const auto renderer = std::ranges::find(named, std::string{ "src/engine/renderer.cpp" }, &file::Relative) != named.end();
			return ask_in_workspace(input, folder.generic_string(), renderer ? std::string{ "src/engine/renderer.cpp" } : first, pause);
		}
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
		// [[BPR-1912]]. The owner's probe on Claude Code 2.1.314: Claude ran
		// `awk` without asking, outside the read-only list. Reported, the run
		// goes on.
		std::cout << R"({"type":"assistant","session_id":"fixture","message":{"id":"msg-t6","role":"assistant","content":[{"type":"tool_use","id":"t6","name":"Bash","input":{"command":"awk 'NR<3' )" << escape(first) << R"("}}]}})" << '\n';
		std::cout << R"({"type":"user","session_id":"fixture","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"t6","content":"two lines","is_error":false}]}})" << '\n'
				  << std::flush;
		// [[BPR-1913]]. Run as the `claude` in `attended-drafting-failing-claude`,
		// the drafting run then reaches for the network, which a drafting
		// run never loads, and the app ends it as `tool_refused`.
		if (aFailing) {
			pause();
			std::cout << R"({"type":"assistant","session_id":"fixture","message":{"id":"msg-t7","role":"assistant","content":[{"type":"tool_use","id":"t7","name":"WebFetch","input":{"url":"https://example.com/renderer"}}]}})" << '\n'
					  << std::flush;
			return 0;
		}
		pause();
		// Any other request is answered as a tool-free one is.
		if (input.find("bpr.tour-draft.v1") == std::string::npos) {
			return std::nullopt;
		}
		std::string result;
		{
			std::string stops;
			const char* titles[] = { "Where the change starts", "What it calls next", "How it ends" };
			// [[BPR-1915]]. Drafted as the style section asks: short
			// paragraphs, a list where it helps, inline code for names.
			const char* explanations[] = { "Start here: this is where the new behaviour enters.\n\nRead `begin_frame()` first, then the `clear()` call under it.",
										   "These lines carry the change through:\n\n- `clear()` now takes the view it clears.\n- `draw()` draws only that view's scene.\n\nCheck that every caller passes its own view.",
										   "This is where the change finishes. Notice what `present()` does when a view drew nothing." };
			const auto  stop           = [&stops](const std::string& aPath, const std::size_t aFirst, const std::size_t aLast, const std::string& aTitle, const std::string& aExplanation) {
				stops += (stops.empty() ? "" : ",") + std::string{ R"({"path":")" } + escape(aPath) + R"(","first_line":)" + std::to_string(aFirst) + R"(,"last_line":)" + std::to_string(aLast) + R"(,"title":")" + escape(aTitle) + R"(","explanation":")" + escape(aExplanation) + "\"}";
			};
			const auto has = [&named](const std::string& aPath) { return std::ranges::find(named, aPath, &file::Relative) != named.end(); };
			if (has("src/engine/renderer.cpp") && has("src/engine/renderer.h") && has("src/main.cpp")) {
				stops = renderer_stops();
			} else {
				for (std::size_t index = 0U; index < named.size() && index < 3U; ++index) {
					stop(named[index].Relative, 1U, std::min<std::size_t>(named[index].Lines, 4U), titles[index], explanations[index]);
				}
			}
			// One stop the core leaves out: not a file of this pull request.
			stops += (stops.empty() ? "" : ",") + std::string{ R"({"path":"src/not_in_this_pull_request.cpp","first_line":3,"last_line":9,"title":"A file it imagined","explanation":"This file is not part of the change."})" };
			result = R"({"schema":"bpr.tour-draft.v1","problem":"Two views that cleared in the same frame drew over each other, so the second view showed the first one's scene.\n\nIt happened whenever both views drew between one `begin_frame()` and its `present()`.","solution":"Each view now clears and draws its own scene between begin and present, so the frames no longer overlap.\n\n- `renderer::clear()` takes the view it clears.\n- `renderer::draw()` draws only that view's scene.\n\nThe order of calls in `src/engine/renderer.cpp` is unchanged.","stops":[)" + stops + "]}";
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
		const auto folder  = argc > 0 ? std::filesystem::path{ argv[0] }.parent_path().filename() : std::filesystem::path{ };
		const auto failing = folder == "attended-drafting-failing-claude";
		if (const auto ended = draft_in_workspace(model, folder == "attended-drafting-claude" || failing, failing, input)) {
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
