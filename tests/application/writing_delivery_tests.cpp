#include "bpr/application/agent_launch.hpp"
#include "bpr/application/writing_hooks.hpp"
#include <cstdlib>
#include <iostream>
namespace {
	using namespace bpr;
	void expect(bool ok, const char* message)
	{
		if (!ok) {
			std::cerr << message << '\n';
			std::exit(1);
		}
	}
}
int main()
{
	using namespace bpr;
	const std::vector<application::writing_source> sources = { { "pr-description", "Approved author explanation" } };
	const std::string                              prefix  = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":)";
	const auto                                     decode  = [&](std::string_view value, bool evidence = false) { return application::decode_writing_result(value, core::writing_hook::problem, sources, evidence); };
	expect(decode(prefix + R"("safe\r\n😀","notes":[]})").has_value(), "bounded UTF8 rewrite decoded");
	expect(!decode(prefix + R"("claim","notes":[]})", true), "empty field requires evidence");
	expect(decode(prefix + R"("claim","notes":[{"kind":"recovered","text":"Existing statement","source_refs":["pr-description"]}]})", true).has_value(), "supplied provenance passes");
	for (const auto& bad : std::vector<std::string>{
			 prefix + R"("x","notes":[],"publish":true})", prefix + R"("x","notes":[],"replacement_text":"y"})",
			 prefix + R"("x","notes":[{"kind":"recovered","text":"claim","source_refs":["made-up"]}]})",
			 prefix + R"("x","notes":[{"kind":"inferred","text":"claim","source_refs":[]}]})",
			 prefix + R"("x","notes":[]} trailing)", std::string{ "\xff" }, std::string(32769, 'x') }) {
		expect(!decode(bad), "malformed/extra/duplicate/forged/oversize rejected");
	}
	expect(decode(R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"insufficient_context","reason":"Which behavior should be explained?"})")->Insufficient, "insufficient is distinct");
	expect(application::decode_writing_result(R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[{"title":"Why","anchor_ref":"pr-description","related_refs":[],"reason":"Review the approach","author_question":""}]})", core::writing_hook::outline, sources, false).has_value(), "outline test uses exact immutable schema without bodies/status");
	expect(!application::decode_writing_result(R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[],"status":"ok"})", core::writing_hook::outline, sources, false), "outline extra status refused");
	core::writing_request request;
	request.Raw           = "Selected text";
	request.Selection.End = request.Raw.size();
	auto prompt           = application::build_writing_prompt({ }, request, sources);
	expect(prompt && prompt->find("pr-description") != std::string::npos, "bounded prompt contains supplied reference");
	request.Raw           = std::string(6001, 'x');
	request.Selection.End = request.Raw.size();
	expect(!application::build_writing_prompt({ }, request, sources), "oversize input refused not silently widened");
	for (const std::string& tools : { std::string{ "[]" }, std::string{ "[\"Bash\"]" } }) {
		application::agent_protocol_decoder decoder{ core::agent_protocol::claude_stream_json, true };
		auto                                init = decoder.push("{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"s\",\"tools\":" + tools + ",\"mcp_servers\":[]}\n");
		expect(init.has_value() == (tools == "[]"), "restricted protocol requires empty tool list");
		if (init) {
			expect(!decoder.push(R"({"type":"assistant","session_id":"s","message":{"role":"assistant","content":[{"type":"tool_use","name":"Bash"}]}})"
								 "\n"),
				   "tool block is protocol failure");
		}
	}
	std::cout << "writing-delivery-tests:pass\n";
}
