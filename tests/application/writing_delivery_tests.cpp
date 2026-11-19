#include "bpr/application/agent_launch.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/prompt_features.hpp"
#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/sha256.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
namespace {
	using namespace bpr;
	void expect(bool aOk, const char* aMessage)
	{
		if (!aOk) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
}
int main()
{
	using namespace bpr;
	const std::vector<application::writing_source> sources = { { "pr-description", "Approved author explanation" } };
	const std::string                              prefix  = R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":)";
	const auto                                     decode  = [&](std::string_view aValue, bool aEvidence = false) { return application::decode_writing_result(aValue, core::writing_hook::problem, sources, aEvidence); };
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
	const auto outline = [&](std::size_t aCount, std::size_t aTitleBytes) {
		std::string json = R"({"schema":"bpr.review-preparation.result.v1","kind":"outline","stops":[)";
		for (std::size_t i = 0; i < aCount; ++i) {
			if (i) {
				json += ',';
			}
			json += R"({"title":")" + std::string(aTitleBytes, 't') + R"(","anchor_ref":"pr-description","reason":"why","author_question":"","related_refs":[]})";
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
		expect(application::review_feedback_hook_version == "review-feedback-v3" && hex == "250c466b2676beb62f00e78df1775fbe13723e247c03a31a9f61e7e947abc771", "review-feedback-v3 names exactly this default text");
		expect(application::writing_hook_version_of(core::writing_hook::feedback) == application::review_feedback_hook_version && application::writing_hook_version_of(core::writing_hook::comment) == application::tour_stop_comment_hook_version && application::writing_hook_version_of(core::writing_hook::problem) == application::writing_hook_version, "each hook reports its own default's version");

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
	// [[BPR-2051]] The Enhance prompts: a tour stop's, and (owner look,
	// 2026-11-10) one Enhance writing prompt for comments, replies and notes.
	// Their own defaults, versions, stable purpose IDs and digests; the run
	// takes the prompt it was built with.
	{
		const auto rewrite = std::string{ R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":)" };
		const auto stop    = application::default_writing_hook(core::writing_hook::enhance_stop);
		const auto writing = application::default_writing_hook(core::writing_hook::enhance_writing);
		expect(stop.starts_with("# Enhance a tour stop") && writing.starts_with("# Enhance writing") && writing.find("writing-kind") != std::string_view::npos, "each Enhance prompt ships its own default; the writing one reads which kind of text it is");
		expect(application::writing_hook_version_of(core::writing_hook::enhance_stop) == "enhance-tour-stop-v1" && application::writing_hook_version_of(core::writing_hook::enhance_writing) == "enhance-writing-v1",
			   "each Enhance prompt has its own built-in version");
		const auto purposes = std::array<std::string_view, core::writing_hook_count>{ "tour.problem", "tour.solution", "tour.outline", "tour.stop-comment", "review.feedback", "enhance.tour-stop", "enhance.writing", "review.suggested-comment",
																					  "local.description", "local.discussion", "local.commit-message",
																					  "review.latest-upload", "review.proposed-fix", "review.explain-file", "review.suggest-change", "local-draft.run-check",
																					  "feature.writing-style", "feature.comment-labels" };
		for (std::size_t index = 0; index < core::writing_hook_count; ++index) {
			const auto hook = static_cast<core::writing_hook>(index);
			expect(application::writing_hook_purpose(hook) == purposes[index] && application::writing_hook_of_purpose(purposes[index]) == hook, "every prompt has one stable purpose ID, and the ID names it");
			expect(application::default_writing_hook_digest(hook) == core::writing_digest(application::default_writing_hook(hook)), "the built-in digest is the default text's");
		}
		expect(!application::writing_hook_of_purpose("enhance.unknown") && !application::writing_hook_of_purpose(""), "an unrecognized purpose names no prompt");
		expect(application::writing_hook_of_purpose("enhance.comment") == core::writing_hook::enhance_writing && application::writing_hook_of_purpose("enhance.note") == core::writing_hook::enhance_writing,
			   "the retired comment and note purposes name the Enhance writing prompt that replaced them");
		// [[BPR-2059]] The suggested-comment format: its default, version and
		// purpose, and what every Agent tab brief carries of it.
		{
			auto hooks = core::writing_hook_settings{ };
			expect(application::default_writing_hook(core::writing_hook::suggested_comment).starts_with("# Suggested comments")
					   && application::writing_hook_version_of(core::writing_hook::suggested_comment) == "suggested-comment-v3" && !core::enhance_hook(core::writing_hook::suggested_comment),
				   "the suggested-comment format ships its own default and version, and is not an Enhance prompt");
			const auto shipped = application::suggested_comment_format(hooks);
			expect(shipped && shipped->Source == core::feedback_guidance_source::shipped && shipped->Version == "suggested-comment-v3"
					   && shipped->Text == application::default_writing_hook(core::writing_hook::suggested_comment),
				   "with no override the brief carries the shipped default");
			hooks.Overrides[static_cast<std::size_t>(core::writing_hook::suggested_comment)] = "Use Conventional Comments labels.";
			const auto mine                                                                  = application::suggested_comment_format(hooks);
			expect(mine && mine->Source == core::feedback_guidance_source::profile_override && mine->Text == "Use Conventional Comments labels.", "an override is what the brief carries");
		}

		core::writing_hook_settings settings;
		core::writing_request       enhanceRequest;
		enhanceRequest.Target.Hook   = core::writing_hook::enhance_writing;
		enhanceRequest.Raw           = "nit: this could maybe be simpler i think";
		enhanceRequest.Selection.End = enhanceRequest.Raw.size();
		auto built                   = application::build_writing_prompt(settings, enhanceRequest, sources);
		expect(built && built->find("# Enhance writing") != std::string::npos && built->find("# Tour-stop comment") == std::string::npos, "a comment's or a note's Enhance sends the Enhance writing prompt");
		settings.Overrides[static_cast<std::size_t>(core::writing_hook::enhance_writing)] = "Team voice: short and kind.";
		built                                                                             = application::build_writing_prompt(settings, enhanceRequest, sources);
		expect(built && built->find("Team voice: short and kind.") != std::string::npos && built->find("# Enhance writing") == std::string::npos, "an override is what the next press sends");
		enhanceRequest.Target.Hook = core::writing_hook::comment;
		built                      = application::build_writing_prompt(settings, enhanceRequest, sources);
		expect(built && built->find("# Enhance a tour stop") != std::string::npos && built->find("# Tour-stop comment") == std::string::npos, "a stop with text is Enhance and sends the stop's Enhance prompt");
		enhanceRequest.Scope = core::writing_scope::empty_field;
		enhanceRequest.Raw.clear();
		enhanceRequest.Selection.End = 0;
		built                        = application::build_writing_prompt(settings, enhanceRequest, sources);
		expect(built && built->find("# Tour-stop comment") != std::string::npos && built->find("# Enhance a tour stop") == std::string::npos, "an empty stop is Generate comment and keeps the stop-comment prompt");
		enhanceRequest.Target.Hook   = core::writing_hook::enhance_stop;
		enhanceRequest.Scope         = core::writing_scope::field;
		enhanceRequest.Raw           = "text";
		enhanceRequest.Selection.End = enhanceRequest.Raw.size();
		expect(!application::build_writing_prompt(settings, enhanceRequest, sources), "the stop's Enhance prompt is instructions, never a target");
		expect(application::decode_writing_result(rewrite + R"("Clearer.","notes":[]})", core::writing_hook::enhance_writing, sources, false).has_value(),
			   "Enhance writing decodes the same rewrite result as a stop's Enhance");
	}
	// [[BPR-2264]]. The Writing style Feature: its shipped text, its bound,
	// its one helper, and an override that changes what every request sends.
	{
		const auto shipped = application::default_writing_hook(core::writing_hook::writing_style);
		expect(!shipped.empty() && shipped.size() <= 2048U, "the shipped Writing style is under 2 KiB");
		expect(shipped.find("point first") != std::string_view::npos && shipped.find("question") != std::string_view::npos && shipped.find("No praise") != std::string_view::npos,
			   "it asks for the point first, an uncertain claim as a question, and no praise or sign-off");
		expect(application::writing_hook_purpose(core::writing_hook::writing_style) == "feature.writing-style"
				   && application::writing_hook_of_purpose("feature.writing-style") == core::writing_hook::writing_style
				   && application::writing_hook_version_of(core::writing_hook::writing_style) == application::writing_style_hook_version,
			   "it travels under its own purpose ID and version");
		const auto defaults = core::writing_hook_settings{ };
		const auto style    = application::writing_style_for(defaults, true);
		expect(style.starts_with(application::writing_style_preamble()) && style.ends_with(shipped), "a person-read request carries the fixed preamble, then the text");
		expect(style.find("win over it on any conflict") != std::string::npos && style.find("only the text inside string values") != std::string::npos,
			   "the preamble keeps every format in charge and confines the style to JSON string values");
		expect(application::writing_style_for(defaults, false).empty(), "a request nobody reads, naming no Feature, carries none");
		const auto own = std::array{ std::string_view{ "Explain it. /bpr:writing-style" }, std::string_view{ "and again /bpr:writing-style" } };
		expect(application::writing_style_for(defaults, false, own) == style, "a prompt that names /bpr:writing-style gets the same text");
		const auto named = application::writing_style_for(defaults, true, own);
		expect(named == style && named.find(application::writing_style_preamble(), 1U) == std::string::npos, "named and attached, it is still sent once");
		auto hooks                                                                   = core::writing_hook_settings{ };
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] = "Follow my house-style skill.";
		const auto overridden                                                        = application::writing_style_for(hooks, true);
		expect(overridden == std::string{ application::writing_style_preamble() } + "Follow my house-style skill." && overridden.find("point first") == std::string::npos,
			   "an override replaces the shipped text, sent as written, naming a skill");
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] = std::string(core::maximum_writing_instruction_bytes, 'x');
		expect(core::valid(hooks) && application::writing_style_for(hooks, true).size() == application::writing_style_preamble().size() + core::maximum_writing_instruction_bytes,
			   "the longest override a profile keeps is sent whole");
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] += "x";
		expect(!core::valid(hooks), "a longer override is refused, as every prompt's is");
	}
	// [[BPR-2264]] (owner, 2026-11-18). A weight decoration only where it adds
	// to the label: the review prompt, the label helper's guidance and the
	// suggested-comment format all say so.
	{
		const auto feedback = application::default_writing_hook(core::writing_hook::feedback);
		expect(feedback.find("Add a weight decoration only when it says something the label does not") != std::string_view::npos
				   && feedback.find("`suggestion (blocking)`") != std::string_view::npos && feedback.find("`issue (non-blocking)`") != std::string_view::npos
				   && feedback.find("Never write `issue (blocking)` or `suggestion (non-blocking)`") != std::string_view::npos,
			   "the label helper says when a decoration is warranted");
		const auto macros = core::shipped_prompt_macros();
		auto       review = std::string{ };
		for (const auto& builtin : macros) {
			if (builtin.Macro.Id == "agent-tab.review-pull-request") {
				review = builtin.Macro.Body.Template;
			}
		}
		expect(review.find("Add (blocking) or (non-blocking) only when the weight differs from the label's") != std::string::npos
				   && review.find("with (blocking) or (non-blocking) after issue and suggestion") == std::string::npos,
			   "the shipped review asks for a decoration only where it differs from the label");
		const auto format = application::default_writing_hook(core::writing_hook::suggested_comment);
		expect(format.find("the problem in one sentence, what goes wrong because of it, then the fix") != std::string_view::npos, "a suggested comment is the problem, its consequence and the fix");
	}
	// [[BPR-2270]]. The Comment labels Feature: the shipped convention, the
	// one helper that attaches it to a request that posts comments (once,
	// even when named), an override, and a blank override that sends nothing.
	{
		const auto labelsHook = static_cast<std::size_t>(core::writing_hook::comment_labels);
		const auto shipped    = application::default_writing_hook(core::writing_hook::comment_labels);
		expect(!shipped.empty() && shipped.size() <= 2048U, "the shipped Comment labels is under 2 KiB");
		for (const auto label : { "praise", "nitpick", "suggestion", "issue", "todo", "question", "thought", "chore", "note", "typo" }) {
			expect(shipped.find(std::string{ "`" } + label + "`") != std::string_view::npos && core::find_feedback_label(label).has_value(),
				   "the convention names each label, and each is one the app recognizes");
		}
		expect(shipped.find("Add `(blocking)` or `(non-blocking)` after the label only when the weight differs from the label's") != std::string_view::npos
				   && shipped.find("Never write `issue (blocking)`, `todo (non-blocking)` or `suggestion (non-blocking)`") != std::string_view::npos,
			   "it decorates only where the weight differs from the label's ([[BPR-2267]])");
		expect(shipped.find("only `issue` blocks the merge; `todo`, `chore`, `typo`,") != std::string_view::npos && shipped.find("necessary") == std::string_view::npos,
			   "only issue blocks on its own; a todo, chore or typo does not ([[BPR-2276]])");
		expect(application::writing_hook_purpose(core::writing_hook::comment_labels) == "feature.comment-labels"
				   && application::writing_hook_of_purpose("feature.comment-labels") == core::writing_hook::comment_labels
				   && application::writing_hook_version_of(core::writing_hook::comment_labels) == application::comment_labels_hook_version,
			   "it travels under its own purpose ID and version");
		const auto* const feature = core::find_prompt_feature(core::comment_labels_feature);
		expect(feature != nullptr && feature->Editable && feature->BlankTurnsOff && feature->Hook == core::writing_hook::comment_labels, "comment-labels is an editable Feature a blank turns off");
		const auto defaults = core::writing_hook_settings{ };
		const auto labels   = application::comment_labels_instructions(defaults);
		expect(labels.starts_with(application::comment_labels_preamble()) && labels.ends_with(shipped), "it is sent as the fixed preamble, then the text");
		const auto style  = application::writing_style_for(defaults, true);
		const auto posted = application::prompt_features_for(defaults, { .HumanAudience = true, .PostedComments = true });
		expect(posted == labels + "\n\n" + style, "a request that posts comments carries Comment labels, then the Writing style last");
		expect(style.find("Comment labels (the") == std::string::npos, "a request that writes no comment carries no Comment labels");
		expect(application::prompt_features_for(defaults, { .PostedComments = true }) == labels, "with no reader, only Comment labels");
		const auto own = std::array{ std::string_view{ "Draft it. /bpr:comment-labels" }, std::string_view{ "again /bpr:comment-labels" } };
		expect(application::writing_style_for(defaults, false, own) == labels, "a prompt that names /bpr:comment-labels gets it");
		const auto named = application::prompt_features_for(defaults, { .HumanAudience = true, .PostedComments = true }, own);
		expect(named == posted && named.find(application::comment_labels_preamble(), 1U) == std::string::npos, "named and attached, it is still sent once");
		auto hooks                  = core::writing_hook_settings{ };
		hooks.Overrides[labelsHook] = "Start each comment with one of: blocker, idea.";
		const auto overridden       = application::prompt_features_for(hooks, { .PostedComments = true });
		expect(overridden == std::string{ application::comment_labels_preamble() } + "Start each comment with one of: blocker, idea." && overridden.find("`nitpick`") == std::string::npos,
			   "an override replaces the shipped convention in what is sent");
		hooks.Overrides[labelsHook] = std::string{ core::blank_feature_override };
		expect(core::valid(hooks) && application::comment_labels_instructions(hooks).empty() && application::prompt_features_for(hooks, { .PostedComments = true }).empty()
				   && application::prompt_features_for(hooks, { .PostedComments = true }, own).empty()
				   && application::prompt_features_for(hooks, { .HumanAudience = true, .PostedComments = true }) == style,
			   "a blank override (the owner's plain comments) sends no label guidance, even where it is named");
		hooks.Overrides[labelsHook] = " \n\t";
		expect(application::comment_labels_instructions(hooks).empty(), "any blank override is no labels");
		// The suggested-comment format asks for a label only when Comment
		// labels is sent, so a blanked convention leaves comments plain.
		const auto format = application::default_writing_hook(core::writing_hook::suggested_comment);
		expect(format.find("When the request carries Comment labels (/bpr:comment-labels), open with the label it asks for") != std::string_view::npos
				   && format.find("Without it, start with the problem and no label") != std::string_view::npos && format.find("chooses a label in the comment editor") == std::string_view::npos,
			   "the suggested-comment format defers the label to Comment labels");
	}
	std::cout << "writing-delivery-tests:pass\n";
}
