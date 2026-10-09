---
schema: "implement-rfc.card.v1"
id: "BPR-1091"
title: "Wire optional convention-aware hooks through the existing workshop"
status: "In Progress"
rfcs: ["RFC-BPR-0051"]
depends_on: []
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-17","milestone":"hooks-cli","acceptance":["R19","R34","R35","R36","R37"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"sequence_after":["BPR-1090"],"coordinator_amendment":{"date":"2026-10-08","decision":"BPR-1090's landed Review feedback hook and workshop_feedback_guidance() satisfy the dependency; its parked owner Settings look does not block this wiring"}}
acceptance: [{"criterion":"R19","outcome":"pass","revision":"a6ac530d3","evidence":{"kind":"command","path":"tests/end_to_end/agent_finding_composition_public_path.cpp","detail":"Tour hooks keep their purposes: the guidance rides only on a workshop_comment gesture with an operation (core refuses it elsewhere: core_agent_context_supplement, application_agent_launch); a launch-panel turn with the helper on carries none (application_agent_finding_workshop_guidance_default_contract); application_review_tour_writing and application_writing_delivery pass unchanged."}},{"criterion":"R34","outcome":"pass","revision":"a6ac530d3","evidence":{"kind":"command","path":"tests/end_to_end/agent_finding_composition_public_path.cpp","detail":"Helper on: a press carries the shipped review-feedback-v1 text (default course). No agent installed: the press is told no revision, the draft is untouched, nothing is published, and a typed label is still read (no-agent course). No model call anywhere."}},{"criterion":"R35","outcome":"pass","revision":"a6ac530d3","evidence":{"kind":"command","path":"tests/application/agent_launch_tests.cpp","detail":"A customized hook saved on the Settings row is sent as written with source profile_override and the shipped version named (custom course, application_agent_launch); never merged with the default; a tour hook override is not guidance; helper off sends none and the press wire is byte-identical (pinned digest)."}},{"criterion":"R36","outcome":"pass","revision":"a6ac530d3","evidence":{"kind":"command","path":"tests/end_to_end/agent_finding_composition_public_path.cpp","detail":"Turning the helper on and saving the hook start no process and record no press; the guidance adds no argument, environment variable, executable or tool to any protocol's launch; zero GitHub submissions, target observations or stored comments in every course."}},{"criterion":"R37","outcome":"pass","revision":"a6ac530d3","evidence":{"kind":"command","path":"tests/core/feedback_workshop_candidate_tests.cpp","detail":"core_feedback_workshop_candidate, application_agent_comment_workshop_host, qt_review_draft_editor and the default/custom courses: the shared recognizer reads both texts, suggestion bytes and the question are compared, the change is shown under the before and after (unknown or malformed headers named), and Accept is one local edit with the candidate's bytes. Provenance: the runtime supplies no model identity on this path, so none is claimed."}},{"criterion":"BPR-1091:owner-look","outcome":"pending","revision":"a6ac530d3","evidence":{"kind":"freeform","detail":"The candidate line under the workshop's before and after is new visible UI; it was built from offscreen tests without the frontend-design skill, which is not installed on bpr-main-linux. The owner's look is left open for the coordinator."}}]
---

# BPR-1091: Wire optional convention-aware hooks through the existing workshop

Pack alias: **PACK-CC-17** (live `BPR-1091`, allocated at import).

## Outcome and work

Compose the default fragment only into relevant explicitly invoked tasks. Preserve user overrides and tour purposes. Validate candidate prefixes through the shared parser and display semantic/patch changes. Keep runtime-supplied provenance and permitted context boundaries intact.

## Coordinator amendment (2026-10-08)

The coordinator ruled that [[BPR-1090]]'s landed Review feedback hook and
`workshop_feedback_guidance()` satisfy this card's dependency. BPR-1090 stays
parked only for the owner's Settings look, which does not block this wiring.
BPR-1090 moved from `depends_on` to `metadata.sequence_after`, as
[[BPR-1089]] did, because the board keeps a card with an unfinished dependency
in Backlog. Coordinator direction for this card:
- Send the guidance with comment-workshop requests only while the label
  helper is on.
- No model calls in tests; use the fake agent harness.
- Leave the owner look open.

## Acceptance

- Default, customized, disabled and no-agent fixtures dispatch the expected content with no automatic launch.
- Prose-only enhancements preserve uncertainty and patch bytes unless a visible candidate proposes a change.
- Candidate acceptance is local only; prompts cannot bypass publication or native review authority.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation (2026-10-08, Linux)

Station bpr-main-linux (WSL2 x86_64, GCC 15.2.0, Debug `-Werror`). Landed
as product commit `14f1e37ef`, documentation `173ecb6dd`, pins resealed in
`a6ac530d3` and the MSVC binding fix `2f040b0fa`, over `6fb13f1bb`. The rows
and controls were run at the first rebase over `6752a7136` (product
`4d69b888d`, reseal `3a6756185`, the same `src/`, `tests/` and `cmake/` as
the landed commits except the binding fix). The second rebase brought only
BPR-1324's Qt title-row change, no pinned `src/app` source, and the round-trip
contract, the network-isolation manifest, `architecture_boundaries`,
`type_read_binding_contract`, `qt_review_draft_editor`, `qt_settings_window`,
`application_agent_launch` and the four workshop courses were rerun there
(11 of 11). The very first candidate was `06a483a90` over `3c7c56620`. Engineering owner `rfc-implementer`,
model `claude-opus-5-5`, effort `high`.

- **Core**: `core::supplement_feedback_guidance` (source `shipped` or
  `profile_override`, the shipped version, the text) is an optional field of
  the agent supplement request and supplement. `build_agent_supplement`
  admits it only beside an admitted workshop operation and refuses it whole
  otherwise, or when empty, over 8,192 bytes, not UTF-8, or unnamed.
  `core::compare_workshop_candidate` reads the original and the candidate
  with `recognize_feedback_prefix` (label, decorations, expectation, the raw
  token), every suggestion block's exact bytes (`core::suggestion_blocks`, new
  beside `suggested_change_of`), and whether the prose asks a question (code,
  fences and query strings excluded).
- **Application**: `workshop_feedback_supplement(hooks, label helper)` builds
  the guidance from `workshop_feedback_guidance`; `agent_context_observation`
  carries it, `build_agent_context_supplement` refuses it under any gesture
  but a workshop press, and `encode_agent_supplement` writes
  `feedback_guidance.source`, `.version` and `.text` after
  `workshop_operation` only when present. `agent_comment_workshop_host`
  attaches the comparison to every proposal; it still refuses for the same
  four reasons only.
- **Composition**: `dispatch_agent_invocation` fills the guidance from the
  profile's accepted preferences for a `workshop_comment` gesture with an
  operation. That is every press of the six and the agent-assisted
  composition (dispatched as a workshop clarify); launch-panel, chat, tour
  writing and AI-review turns carry none.
- **Qt**: `review_draft_editor` shows one line under the two texts,
  `reviewWorkshopCandidate`, computed from the texts it shows, and withdrawn
  with the other workshop surfaces in an agent-free window.

### Acceptance evidence

| Criterion | Evidence |
|---|---|
| Default, customized, disabled and no-agent fixtures dispatch the expected content with no automatic launch | Composed courses through the real application and the fake agent (`agent_finding_canary`, which reports the guidance it received): `application_agent_finding_workshop_guidance_default_contract` (helper on by the Settings path; the press carries `source=shipped version=review-feedback-v1`, 3,193 wire bytes, the shipped text's head), `..._custom_contract` (an override saved on the Settings row is carried as `profile_override`, as written), `application_agent_finding_workshop_press_contract` (helper off, the default: `source=absent`) and `..._no_agent_contract` (no agent installed: one attempt, "No revision", draft untouched, a typed `question (blocking)` still read by the chip). In each, turning the helper on and saving the hook started no process and recorded no press. `application_agent_launch` pins the helper-off press wire's SHA-256 as captured before the change. |
| Prose-only enhancements preserve uncertainty and patch bytes unless a visible candidate proposes a change | `core_feedback_workshop_candidate`: a reworded labelled question with a CRLF patch is kept on all three; decoration, custom decoration, label, unknown and malformed headers, a dropped or added question, and a patch changed (a line ending included), added or removed are each named. `qt_review_draft_editor`: the line reads "Label kept: question (non-blocking) · Still asks a question · Suggested change kept byte for byte" or names each change. Courses: the shipped-guidance candidate shows "Label added: question · Still asks a question", the override candidate "Label added: issue (blocking) · No longer asks a question". |
| Candidate acceptance is local only; prompts cannot bypass publication or native review authority | `qt_review_draft_editor`: a candidate saying "Publish this now and submit Request changes" is accepted as exactly one `edit` carrying its bytes, no receipt, no publication. `application_agent_launch`: the same arguments, environment, executable and working directory with and without the guidance, for Codex, Claude and the external terminal. Courses: zero submissions, target observations and stored comments; the accepted draft is the candidate. |
| R19 | Tour hooks are untouched (`application_writing_delivery`, `application_review_tour_writing`); the core refuses guidance without a workshop operation; a launch-panel turn with the helper on carries none (default course). |
| R34 | The shipped default reaches the press when the helper is on; with no agent installed, labels and drafts work and nothing is sent. |
| R35 | The override is sent as written, named `profile_override` with the shipped version; reset and migration stay BPR-1090's evidence. |
| R36 | No automatic launch, no new argument, tool, environment, MCP server, provider, network or publication path. |
| R37 | The candidate is a previewable proposal; its label is re-read by the shared recognizer and its changes shown; uncertainty and patch bytes are compared. The workshop runtime supplies no model identity, so the proposal claims none. |

### Failing-before controls (each applied, run, reverted)

| # | Mutation | Fails at |
|---|---|---|
| C1 | `valid_feedback_guidance` ignores the operation | `core_agent_context_supplement`: "guidance on a request that is not a workshop press is refused whole, not dropped" |
| C2 | Encoder writes the three guidance lines even when absent | `application_agent_launch`: "a press without guidance writes exactly the bytes it wrote before the hook was sent" |
| C3 | `build_agent_context_supplement` drops the guidance | `application_agent_launch`: "the composed press carries the guidance into the supplement" |
| C4 | `workshop_feedback_supplement` always says `shipped` | `application_agent_launch`: "a customized hook is sent as written, never merged with or appended to the default" |
| C5 | Composition sends no guidance | default and custom courses: `press-carried-effective-guidance`, `proposal-is-guided-candidate`, `candidate-change-shown`, `accepted-draft-is-candidate` |
| C6 | Composition ignores the label helper | `application_agent_finding_workshop_press_contract`: `helper-off-press-carries-no-guidance` (and the proposal changed) |
| C7 | Composition sends guidance on every gesture | default course: `launch-panel-turn-carries-no-guidance`, `two-gestures-two-launches` (the launch-panel turn is refused) |
| C8 | Candidate comparison ignores decorations | `core_feedback_workshop_candidate`: "replacing a custom decoration is a label change" |
| C9 | Patch comparison ignores `\r` | `core_feedback_workshop_candidate`: "a patch whose line ending changed is changed: the bytes are the patch" |
| C10 | Inline code read as prose | `core_feedback_workshop_candidate`: "inline code asks nothing" |
| C11 | Editor does not show the candidate line | `qt_review_draft_editor`: "a prose-only candidate says it keeps the label, the question and the suggested change"; default course `candidate-change-shown` |
| C12 | Accept emits `begin_publish` instead of `edit` | `qt_review_draft_editor`: "accepting emits exactly one local draft edit with the candidate's bytes; no publication, receipt or review decision" |
| C13 | A label-helper or hook change starts a workshop press | custom course `settings-launched-nothing` (and `launch-panel-turn-carries-no-guidance`); no-agent course `settings-launched-nothing`, `one-press-one-attempt` |
| C14 | An unknown label read as plain | `core_feedback_workshop_candidate`: "a misspelled label is a change the recognizer cannot read"; `qt_review_draft_editor`: "a label the shared recognizer cannot read is said to be unknown, not passed off as plain" |

### Gates

- Dev rows at `3a6756185` (first rebase; same sources as `a6ac530d3`): 88 of 88 (`ctest -R "agent|workshop|writing|feedback|review_draft_editor|suggestion|tour_cli|settings_window|dif_|architecture_boundaries$|test_network_isolation_manifest|round_trip_composition_contract"`), including `architecture_boundaries`, `test_network_isolation_manifest`, `application_review_round_trip_composition_contract`, every `application_agent_*` row and the new `core_feedback_workshop_candidate` and three guidance courses. `architecture_boundary_contract` passed before the rebase.
- `core-only`: 253 of 254 at `3a6756185`; the one failure was
  `type_read_binding_contract` on this card's own new structured binding in
  `agent_context_supplement_tests.cpp` (MSVC C4189 without `[[maybe_unused]]`),
  fixed in `2f040b0fa`, after which it, `core_agent_context_supplement`,
  `core_feedback_workshop_candidate` and both architecture rows pass. The 27
  script contract rows of `tests/CMakeLists.txt` pass in dev.
- Optimized tree (`app-release`, `BUILD_TESTING=ON`, GCC 15.2.0 `-O3 -DNDEBUG
  -Werror`): every target built with no warning; the nine rows this card added
  or extended pass there; the tree was deleted.
- `coding-format-check` with `/usr/bin/clang-format` 22.1.8.
- Pins: origin/main's own pins matched at `6752a7136` (detached worktree); full-application and production-manifest resealed; mutation-branch pin unchanged.

### Limits

- **Owner look** of the candidate line is open (`BPR-1091:owner-look`); the frontend-design skill is not installed on this station, so the line was not built with it. Wording and placement are offscreen-test only.
- **Composition candidate**: the agent-assisted composition press also carries the guidance, but its candidate surface (private composition) does not show the candidate line; only the draft workshop's before and after does.
- **Question reading** is a deterministic reading of `?` in prose, not a judgment of uncertainty; prose changes stay visible as the before and after.
- **Provenance**: no model or effort identity is supplied by this runtime path, so none is recorded or shown.
- **Platforms**: Linux only; macOS and Windows runs are owed (the three courses are pinned uncontained on Windows like the workshop press). No live GitHub, no model call.
