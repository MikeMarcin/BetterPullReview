---
schema: "implement-rfc.card.v1"
id: "BPR-2264"
title: "Writing style: one built-in, editable Feature, /bpr:writing-style, that every request whose answer a person reads carries"
status: "Done"
rfcs: ["RFC-BPR-0129"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","release":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner, 2026-11-18: suggested comments are hard to read; one built-in but editable writing prompt rather than editing every prompt; it should be in 0.5.0; then: build it as a Feature, immutable by default, this the one editable Feature, edited in the Prompts library","reservation":"docs/issues/_reservations/BPR-2264--BPR-2264--c77bb219696e.json","related_rfcs":["RFC-BPR-0129","RFC-BPR-0107","RFC-BPR-0128"],"acceptance":["BPR-2264:every-flow","BPR-2264:override","BPR-2264:once","BPR-2264:formats-win","BPR-2264:exchange","BPR-2264:size","BPR-2264:decorations"]}
---

# BPR-2264: Writing style, one built-in, editable Feature

## Request

The owner, 2026-11-18: "I'm finding the suggested comments difficult to
read. I feel like we need a bit more of our technical humanizer in the
writing or maybe it's better to have a hook where we can plug in a writing
skill for the prompts like a feature that is built-in but editable. Because
putting that everywhere we write for human audience is too many places to
change." Then: "It should definitely be in the 0.5.0 release." Then: "build
the writing style hook at a Feature I think. That way all the skills can
reference it easily. And then realistically I would probably just point that
hook at my existing skills. But there should be mostly immutable features
but we can have some editable features using the prompt library to edit
them. This being the only one I can think of right now".

This card ships ahead of RFC-BPR-0129, which adopts its Feature registry.

## Scope

- A Feature registry (`bpr:` names). Every Feature is immutable unless the
  registry marks it editable; `writing-style` is the one editable Feature.
- **Writing style** is edited in Settings › Prompts like the built-in
  prompts (override, compare, use built-in), and travels in the settings
  and prompt-pack exchange under the purpose ID `feature.writing-style`.
- One helper appends it to every agent request whose answer a person reads.
  A prompt or skill naming `/bpr:writing-style` gets the same text, once.
- Formats win: the style follows the request's own instructions and says
  that any format, schema or block rule wins, and that in a JSON answer it
  governs only the text inside string values.
- The text is sent as written. It may simply name a skill installed for the
  agent. Values substituted from pull request data never bring in a Feature.
- `suggested-comment.md` is tightened: one problem, its consequence and the
  fix, in plain words.
- Owner, the same day: "I think we way over use blocking and non-blocking in
  our suggested comments. They're just noise with how they are currently
  used. They should only appear when they add value over the tag." A
  `(blocking)` or `(non-blocking)` appears only where it says something the
  label does not.

## Acceptance

- **every-flow**: each flow family's sent request carries the style text.
- **override**: an override changes what every flow sends.
- **once**: an explicit `/bpr:writing-style` reference sends the text once.
- **formats-win**: JSON-answer flows still parse with the fixture agent.
- **exchange**: export and import carry the override.
- **size**: the default is under 2 KiB; the override's bound holds and a
  launch with the largest override is still admitted.
- **decorations**: a drafted finding gets no weight decoration its label
  already implies, and the guidance says when one is warranted.

## Delivered

**Feature registry.** `core/prompt_features.hpp`: `prompt_feature` (name,
editable, the override that keeps its text), `find_prompt_feature`,
`feature_of_hook`, `referenced_features`. `writing-style` is the only entry
and the only editable one. `bpr:` is a reserved qualifier
(`core/skill_references.hpp`): `/bpr:<name>` is never a skill reference and is
never noted as unknown; `find_feature_references` reads it.

**Storage and editing.** `core::writing_hook::writing_style` (17 hooks),
purpose ID `feature.writing-style`, version `writing-style-v1`, shipped text
`resources/review-preparation/writing-style.md` (1,025 bytes). Workbench
settings schema 34 adds the `writing_style` column (33 migrates); the agent
setup store's hook tables check 0 to 16 (15 rebuilds). Settings › Prompts
lists it under a new Features heading after the Agent tab group (the review
skill stays in the Agent tab group); its help line says it can point at a
skill installed for the agent. `bpr tour hooks` lists, edits and resets it as
`writing-style`. The settings exchange carries it like every prompt.

**One helper.** `application::writing_style_for(settings, human_audience,
own_texts)` returns a fixed preamble (formats win; in JSON only string
values) plus the effective text, once however often it is named.
`build_agent_launch` places `agent_launch_options::WritingStyle` after every
other instruction: last in Claude's system prompt or Codex exec's
`developer_instructions`, or, where instructions are pasted (App Server, the
agent's switch, past the Windows bound), as the request's last field
`writing_style`. Three call sites give it: the PR window's
`launch_agent_invocation` (every Agent tab turn, comment workshop press,
returning question, Explain this file, Suggest the change, and every tour run
and tour chat), the local draft runner (description, discussion, commit
message, a check's plan and run), and `bpr tour assist context`.

**Decorations.** The shipped Review this pull request / file / commits
prompt no longer asks for `(blocking)` or `(non-blocking)` after every issue
and suggestion: the label carries the weight, and a decoration is added only
where it differs. `core::implied_expectation` gives each label's own weight;
`read_agent_comment_suggestion` (every Draft comment from a finding, a report
item or an answer) leaves out a weight the label implies and keeps one that
differs. `review-feedback.md` (now `review-feedback-v2`) says the same. The
reviewer's label toggles are unchanged.

## Evidence

Lane `claude/v0-5-0-prerelease-targets-52265d`, dev preset, macOS.

- **every-flow**: `application_agent_invocation_composition_contract` (Agent
  tab: the request's `writing_style` field); the scoped writing stub refuses
  any tour or tool-free run without the style, so
  `application_review_tour_composition_public_path`,
  `application_review_tour_writing`, `application_review_tour_writing_wrapper`,
  `application_enhance_comment_note_public_path`,
  `application_returning_review_latest_public_path`,
  `application_returning_review_addressed_public_path`,
  `application_explain_file_public_path` and
  `application_suggest_change_public_path` prove those families;
  `tour_cli_composition` (tour assist prompt, between format and evidence);
  `application_agent_launch_policy` (every route, the local runner's
  tool-free shape).
- **override**: `application_agent_launch_policy` (Claude, Codex exec, App
  Server send the override, never the shipped text), `tour_cli_composition`
  (an edited style is what the next prompt sends), `application_writing_delivery`.
- **once**: `application_writing_delivery`, `application_agent_launch_policy`
  (a question naming `/bpr:writing-style` sends the text once),
  `core_skill_references`.
- **formats-win**: with the Fixture agent,
  `application_tour_chat_queue_local_draft_public_path`,
  `application_tour_chat_queue_whole_tour_public_path`,
  `application_tour_chat_queue_stop_public_path`,
  `application_local_draft_check_desktop_public_path`,
  `application_fixture_agent_every_ask`, `fixture_agent_answers`;
  `application_review_report_public_path`, `application_ask_report_public_path`,
  `application_ask_addressed_public_path`, `qt_agent_report_view`.
- **exchange**: `sqlite_agent_configuration_storage` (export under
  `feature.writing-style`, preview, apply, sent text);
  `sqlite_workbench_settings_storage` (schema 33 to 34);
  `application_profile_tour_handoff_public_path`.
- **size**: `application_writing_delivery` (default under 2 KiB; the 8,192-byte
  override bound), `application_agent_launch_policy` (the largest override
  launches).
- **decorations**: `core_agent_comment_suggestion`, `core_agent_finding_promotion`,
  `core_feedback_convention`, `application_writing_delivery` (the guidance and
  the shipped review prompt).
- Also passed: `core_prompt_macro`, `core_agent_report`, `core_agent_draft_all`,
  `core_agent_configuration`, `application_agent_launch`,
  `application_prompt_macro_host`, `qt_settings_window` (and scale 125),
  `qt_agent_configuration_dialogs`, `architecture_boundaries`; format check
  passed on the changed C++; clang-tidy reported nothing on changed lines;
  `verify_documentation.py` DOCUMENTATION_OK.

## Left out

- A skill the style text names is sent as written and not inlined; a
  tool-free run cannot load it. Inlining `/skill` references from the style
  is a follow-up.
- Settings › Prompts has no Compare for built-in prompts; RFC-BPR-0129 M3
  ([[BPR-2225]]) brings Compare to them.
- The `/` completion picker does not list Features yet (RFC-BPR-0129 M1).
- Review Cycle rounds (`build_round_launch`) do not carry the style: the
  round is the reviewer's own document for their harness.
- `report_summary_comment` (Keep summary) lists items with their labels as
  written.
