#include "bpr/application/agent_launch.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/sha256.hpp"
#include <cstdio>
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
	// [[BPR-1225]] A note about the person's own text may cite no source, but it is not evidence.
	expect(decode(prefix + R"("x","notes":[{"kind":"recovered","text":"Kept the author's wording","source_refs":[]}]})").has_value(), "uncited recovered note passes");
	expect(decode(prefix + R"("x","notes":[{"kind":"inferred","text":"claim","source_refs":[]}]})").has_value(), "uncited inferred note passes");
	expect(!decode(prefix + R"("x","notes":[{"kind":"recovered","text":"claim","source_refs":[]}]})", true), "an uncited note is not evidence for an empty field");
	for (const auto& bad : std::vector<std::string>{
			 prefix + R"("x","notes":[],"publish":true})", prefix + R"("x","notes":[],"replacement_text":"y"})",
			 prefix + R"("x","notes":[{"kind":"recovered","text":"claim","source_refs":["made-up"]}]})",
			 prefix + R"("x","notes":[]} trailing)", std::string{ "\xff" }, std::string(32769, 'x') }) {
		expect(!decode(bad), "malformed/extra/duplicate/forged/oversize rejected");
	}
	expect(decode(R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"insufficient_context","reason":"Which behavior should be explained?"})")->Insufficient, "insufficient is distinct");
	expect(application::decode_writing_result(R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[{"title":"Why","anchor_ref":"pr-description","related_refs":[],"reason":"Review the approach","author_question":""}]})", core::writing_hook::outline, sources, false).has_value(), "outline test uses exact immutable schema without bodies/status");
	expect(!application::decode_writing_result(R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[],"status":"ok"})", core::writing_hook::outline, sources, false), "outline extra status refused");
	const std::string outlinePrefix = R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[{"title":"Why","anchor_ref":"pr-description","reason":"Private reason","author_question":"","related_refs":[])";
	for (const auto& tail : { R"(,"body":"publish me"}]})", R"(,"Ready":true}]})", R"(,"tool":"Bash"}]})", R"(,"title":"duplicate"}]})" }) {
		expect(!application::decode_writing_result(outlinePrefix + tail, core::writing_hook::outline, sources, false), "outline body/Ready/tool/duplicate keys refused");
	}
	for (const auto& related : { R"(["pr-description"])", R"(["missing"])", R"(["pr-description","pr-description"])", "null" }) {
		const auto json = outlinePrefix.substr(0, outlinePrefix.size() - 2) + related + "}]}";
		expect(!application::decode_writing_result(json, core::writing_hook::outline, sources, false), "duplicate primary/related, unknown and malformed refs refused");
	}
	const auto duplicateSources = std::vector{ sources.front(), sources.front() };
	const auto validOutline     = outlinePrefix + "}]}";
	expect(!application::decode_writing_result(validOutline, core::writing_hook::outline, duplicateSources, false), "duplicate supplied IDs cannot resolve ambiguously");
	for (std::size_t index = 0; index < validOutline.size(); ++index) {
		expect(!application::decode_writing_result(std::string_view{ validOutline }.substr(0, index), core::writing_hook::outline, sources, false), "every truncated outline is refused atomically");
		auto mutated   = validOutline;
		mutated[index] = '\0';
		expect(!application::decode_writing_result(mutated, core::writing_hook::outline, sources, false), "embedded-control mutations cannot alter outline authority");
	}
	for (const auto& bad : { prefix + "\"" + std::string(16385, 'x') + R"(","notes":[]})", prefix + R"("\ud800","notes":[]})", prefix + R"("\u0000","notes":[]})", std::string(100, '[') + "0" + std::string(100, ']') }) {
		expect(!decode(bad), "decoded field bounds, invalid scalar and nesting budget refuse");
	}
	const auto outline = [&](std::size_t count, std::size_t titleBytes) {
		std::string json = R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[)";
		for (std::size_t i = 0; i < count; ++i) {
			if (i) {
				json += ',';
			}
			json += R"({"title":")" + std::string(titleBytes, 't') + R"(","anchor_ref":"pr-description","reason":"why","author_question":"","related_refs":[]})";
		}
		return application::decode_writing_result(json + "]}", core::writing_hook::outline, sources, false);
	};
	expect(outline(24, 180).has_value() && !outline(25, 180) && !outline(1, 181), "outline count and title admit boundary and refuse boundary plus one");
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
	application::agent_protocol_decoder writingMetadata{ core::agent_protocol::claude_stream_json, true };
	expect(writingMetadata.push(R"({"type":"system","subtype":"init","session_id":"writing","tools":[],"mcp_servers":[],"model":"reported-initial","claude_code_version":"2.0"})"
								"\n")
			   .has_value(),
		   "restricted init retains dedicated runtime facts");
	const auto output = writingMetadata.push(R"({"type":"assistant","session_id":"writing","message":{"role":"assistant","model":"reported-served","content":[{"type":"text","text":"exact writing bytes"}]}})"
											 "\n");
	expect(output && output->size() == 1 && output->front().Kind == application::agent_protocol_event_kind::output && output->front().Text == "exact writing bytes", "runtime metadata never enters captured writing text or emits a finding");
	const auto metadata = writingMetadata.metadata();
	expect(metadata.ProtocolReported.size() == 2 && metadata.ProtocolReported[0].Settings.Model == "reported-initial" && metadata.ProtocolReported[1].Settings.Model == "reported-served", "restricted writing preserves separately qualified model reports");
	expect(!writingMetadata.push(R"({"type":"result","subtype":"success","session_id":"writing","is_error":false,"result":"summary","bprFindings":{}})"
								 "\n")
			   && writingMetadata.metadata() == metadata,
		   "runtime facts cannot admit a prohibited finding carrier or change on its refusal");
	// [[BPR-1086]]. RFC-BPR-0048 "Tour compatibility" and R19: the tour's
	// problem, solution, outline and stop hooks keep their own purposes. No
	// shipped default asks for a feedback label or a decoration, so a stop the
	// agent drafts is plain unless its author labels it.
	for (const auto hook : { core::writing_hook::problem, core::writing_hook::solution, core::writing_hook::outline, core::writing_hook::comment }) {
		const auto text = application::default_writing_hook(hook);
		expect(!text.empty(), "every tour hook ships its default");
		for (std::size_t label = 0; label < core::feedback_label_count; ++label) {
			const auto name = std::string{ core::stable_name(static_cast<core::feedback_label>(label)) };
			expect(text.find(name + ":") == std::string_view::npos && text.find(name + " (") == std::string_view::npos, "no tour hook asks for a feedback label");
		}
		expect(text.find("(blocking)") == std::string_view::npos && text.find("Conventional Comments") == std::string_view::npos, "nor for a decoration or the convention");
	}
	// [[BPR-1090]] The fifth hook, review feedback: the default conventions
	// guidance (owner, 2026-10-08). It teaches the whole catalog without a
	// model, is versioned, follows a profile override, and is sent with a
	// workshop request only while the label helper is on.
	{
		const auto feedback = application::default_writing_hook(core::writing_hook::feedback);
		expect(!feedback.empty() && feedback.size() <= core::maximum_writing_instruction_bytes && core::valid_writing_utf8(feedback), "the feedback hook ships a default an override could replace");
		for (const auto& entry : core::feedback_label_catalog()) {
			const auto line = "`" + std::string{ entry.Id } + "`: ";
			expect(feedback.find(line) != std::string_view::npos, "the default teaches every catalog label");
			expect(core::find_feedback_label(entry.Id) == entry.Label, "by its exact catalog ID");
		}
		for (const auto* taught : { "`blocking`", "`non-blocking`", "`if-minor`", "GitHub suggested change", "fenced `suggestion` code block", "Request changes", "does not create a GitHub Issue", "Plain prose without a label is always valid", "never to fill a quota", "Keep uncertainty as a question" }) {
			expect(feedback.find(taught) != std::string_view::npos, "the default teaches the decorations and the native review and suggestion distinctions");
		}
		// The text is pinned to its version: changing it without renaming the version fails here.
		const auto  digest = core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(feedback.data()), feedback.size() });
		std::string hex;
		for (const auto byte : digest) {
			char pair[3] = { };
			std::snprintf(pair, sizeof pair, "%02x", byte);
			hex += pair;
		}
		expect(application::review_feedback_hook_version == "review-feedback-v1" && hex == "45579439d8fefcba722d502241f9ba8b96fe39eb035b90e03a71c76c701773fc", "review-feedback-v1 names exactly this default text");
		expect(application::writing_hook_version_of(core::writing_hook::feedback) == application::review_feedback_hook_version && application::writing_hook_version_of(core::writing_hook::comment) == application::writing_hook_version, "each hook reports its own default's version");

		core::writing_hook_settings settings;
		expect(application::effective_writing_hook(settings, core::writing_hook::feedback) == feedback, "an empty override follows the shipped default");
		expect(application::workshop_feedback_guidance(settings, application::review_label_helper::off).empty(), "the label helper off sends no guidance");
		expect(application::workshop_feedback_guidance(settings, application::review_label_helper::on) == feedback, "the label helper on sends the shipped default");
		settings.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)] = "Team rule: always decorate.";
		expect(application::effective_writing_hook(settings, core::writing_hook::feedback) == "Team rule: always decorate." && application::workshop_feedback_guidance(settings, application::review_label_helper::on) == "Team rule: always decorate.", "an override replaces the default and is what a workshop request carries");
		expect(application::workshop_feedback_guidance(settings, application::review_label_helper::off).empty(), "nor is an override sent while the helper is off");
		for (std::size_t hook = 0; hook < core::review_preparation_hook_count; ++hook) {
			expect(application::effective_writing_hook(settings, static_cast<core::writing_hook>(hook)) == application::default_writing_hook(static_cast<core::writing_hook>(hook)), "a feedback override leaves the tour hooks on their defaults");
		}
		settings.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)].clear();
		expect(application::effective_writing_hook(settings, core::writing_hook::feedback) == feedback, "reset restores the shipped default");

		// The feedback hook writes no tour field: it builds no writing prompt and decodes no writing result.
		core::writing_request feedbackRequest;
		feedbackRequest.Target.Hook   = core::writing_hook::feedback;
		feedbackRequest.Raw           = "The cache reduces repeated work.";
		feedbackRequest.Selection.End = feedbackRequest.Raw.size();
		expect(!application::build_writing_prompt(settings, feedbackRequest, sources), "no tour writing prompt is built from the feedback hook");
		expect(!application::decode_writing_result(prefix + R"("x","notes":[]})", core::writing_hook::feedback, sources, false), "and no writing result decodes against it");
	}
	std::cout << "writing-delivery-tests:pass\n";
}
