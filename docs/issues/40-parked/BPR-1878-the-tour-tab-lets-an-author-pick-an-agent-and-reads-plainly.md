---
schema: "implement-rfc.card.v1"
id: "BPR-1878"
title: "The Tour tab lets an author pick an agent and says plainly what each control does"
status: "Parked"
parked_on: "human-review"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: 'I see absolutely no way to pick an agent on the tour tab … This whole tab is a disaster.'"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "769c87f57", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_help (tests/adapters_qt/review_tour_agent_help_tests.cpp, v002): the Tour tab's set_agent_picker holds an agent_run_settings_picker (objectName agentRunSettings) on the same agent_run_settings_source as an Agent-tab picker; it sits at the bottom of the tab, above the draft's state, showing the same 'Claude Code \u00b7 ...' words; choosing Codex in the Tour tab's menu changes the launch panel's selected_profile and the Agent-tab pill, and choosing in the Agent-tab menu shows in the Tour tab; the Tour tab's picker carries the Model and Effort menus, an effort chosen there reaches the shared run-settings dispatch and shows as 'high effort' in both pickers; Draft from PR context asks for Problem help; with no agent both pickers and the tour's agent row are hidden, as in the Agent tab, and manual writing stays open. The host's refusal notice says why on a click (no agent set up, none chosen, not this window's draft, or busy). Passes; qt_workbench_window_scale_100 (Agent tab pill, unavailable agent, placement in the question field) passes on the refactored picker."}}, {"criterion": "AC2", "outcome": "pass", "revision": "769c87f57", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_help asserts in the agent and no-agent states that no visible button reads Next, Recover explanation, Use selected description text or Enhance text, that the Problem helpers and Next carry tooltips, and that no notice says 'Choose an available agent'. qt_review_tour_steps, qt_scoped_prose_editor, qt_review_tour_publication_working, qt_review_tour_stop_view, qt_drawer_parts_scale_{100,125,200}, qt_drawer_surfaces_fit_scale_{100,125,200}, application_review_tour_composition_public_path, application_review_tour_writing and application_review_tour_writing_wrapper pass."}}, {"criterion": "AC4", "outcome": "pass", "revision": "769c87f57", "evidence": {"kind": "command", "detail": "python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK; coding-format-check passed (clang-format 22.1.8)."}}]
---

# BPR-1878: The Tour tab lets an author pick an agent and says plainly what each control does

## Observed

The owner, 2026-11-05, authoring a tour for their own pull request. The Tour
tab shows:

- A banner: "Choose an available agent and wait for its current request.
  Manual editing is available." There is no agent picker anywhere on the tab,
  so the instruction cannot be followed.
- Step 1 "Problem" with a text box "What problem does this change solve?",
  then buttons **Recover explanation**, **Use selected description text** and
  **Next**, then steps 2 "Solution" and 3 "Stops", and "Saved locally. Nothing
  published." with **Preview and publish**.
- The owner could not tell what "Recover explanation" or "Use selected
  description text" do, and "Next" made no sense in that position.

## Change

Load the frontend-design skill and redesign the authoring surface so a first
time author understands it without reading docs:

- An agent picker on the tab (or a clear link to the one in the Agent tab)
  wherever agent help is offered; if no agent is available, say why and how to
  get one, in one line. Never instruct the author to choose something the tab
  does not show.
- Rename or replace "Recover explanation" and "Use selected description text"
  with labels that say the effect (e.g. draft from the PR description, draft
  with the agent), or drop them if redundant. Each control's tooltip says what
  it does.
- Step navigation that reads as a guided flow, or no "Next" at all if every
  step is visible; the current step and what is left are obvious.
- Keep manual editing first-class.

## Acceptance

- AC1. With an agent available, the author can choose it from the Tour tab's
  flow and start agent help; with none, the tab says why. Qt test.
- AC2. No control on the tab names an action the tab does not offer; every
  button label states its effect.
- AC3. The owner accepts the redesigned look (attended review, one question).
- AC4. `python3 scripts/verify_documentation.py --root .` passes.

## Diagnosis

- There was no agent picker on the Tour tab. Writing asked
  `writing_agent()`: the Agent tab's chosen agent if it can write tour text,
  else the first added agent that speaks Claude Code's stream JSON. The only
  menu was in the Agent tab (and Settings mirrored it). The banner "Choose an
  available agent and wait for its current request" covered three different
  refusals: no agent, a draft this window does not own, and an agent busy
  with another request.
- **Recover explanation** (the helper on an empty Problem or Solution)
  copied the PR description's own `Problem`/`Solution` section verbatim when
  there was exactly one, and otherwise asked the agent for a draft. With text
  in the field it read "Enhance text"; with a selection, "Enhance selection".
  RFC-BPR-0043 already named these "Draft from PR context" and "Enhance
  problem statement/solution/comment"; the code had drifted from it, so the
  RFC needed no amendment.
- **Use selected description text** copied the selection in Overview's
  description Source view into the field, keeping its source span as the
  field's baseline for updating the description in place later.
- **Next** went to the next step. It sat in the helpers' row.

## Implementation

v001, commit `d1ea88005` (frontend-design skill loaded): a Drafting agent
menu at the top of the tab sharing the Agent tab's menu model, a help line
and Set up an agent; RFC-BPR-0043 helper labels and tooltips; Copy selected
description text; Next: Solution / Next: Stops on their own row;
`--attended-tour-authoring AGENT` for the owner look.

v002, commit `ebcfad9ba` (frontend-design skill loaded again), after the
owner's v001 reject:

- The Agent tab's agent · model · effort pill moved out of
  `agent_tab_view` into `agent_run_settings_picker`
  (`src/adapters/qt/src/agent_run_settings_picker.cpp`) over an
  `agent_run_settings_source` that the Agent tab owns: the settings, the
  model each agent last reported, and where a choice, a model or effort, the
  catalogue request and Agent settings… go. The Agent tab's question field
  shows one picker on it; `workbench_window` gives the Tour tab a second one,
  which `review_tour_editor::set_agent_picker` places at the bottom,
  right-aligned above the draft's state as in the question field. Same
  menu, same choice; hidden while no agent is chosen, "Unavailable: …" when
  the chosen agent went, an icon alone when narrow.
- v001's top agent help, its hint line, Set up an agent and the host's
  `tour_agent_help()` are gone. The Agent tab's picker shows no
  "set up" state, so the Tour tab follows it; a helper clicked without a
  usable agent says why in the tab's notice.
- Owner note on v002 ("i need the ability to pick effort level. It needs
  the bottom setting picker from agents tab too"), commit `769c87f57`: the
  pill is that settings picker, with its Model and Effort menus; the test
  chooses an effort in the Tour tab and sees it reach the shared dispatch
  and show in both pills. Tour drafting requests already run with the
  chosen agent's model and effort (`dispatch_agent_invocation` with
  writing-only still applies Claude's and added agents' model and effort;
  `agent_launch` passes `--model`/`--effort`). The look chooses high effort
  so the pill shows one. The Agent tab bar's other controls (add context,
  suggested prompts, read scope, Send) are about chat questions and do not
  apply to drafting, so the Tour tab does not carry them.
- The helper labels, copy helper and Next from v001 stay.
- `docs/architecture/review-tours.md` describes the shared picker.

## Attended review (AC3)

- v001: frozen build of `d1ea88005` (root removed with
  `scripts/freeze_attended_build.py remove`), session
  `3c2a6241b7bf337b8ac749c89d496abd` (state
  `/Users/jaeger/.codex/attended-review/bpr1878-v001-99RcVJ/state`, host
  stopped). Owner verdict 2026-11-06 on `BPR-1878-tour-authoring`
  (`d1ea88005-v001`): **reject**, "That agent picker is no where near good
  enough, we should just have the bottom agent picker the same one from the
  agent tab."
- v002: frozen build of `769c87f57` at `/Users/jaeger/.codex/attended-builds/bpr1878-tour-authoring-v002.9doad7pi` (executable SHA-256
  `183a651e0cb86fc372a0a4f204ca56fe7fb965ceae191ad5dded6160d441ba94`), fixture `conventional-comments-v1`, `--attended-open-file
  src/engine/renderer.cpp --attended-tour-authoring
  {build}/tests/end_to_end/scoped_writing_agent`. The launcher's own
  offscreen check reached `ATTENDED_TOUR_SHOWN`. Launch:
  `python3 /Users/jaeger/.codex/attended-builds/bpr1878-tour-authoring-v002.9doad7pi/launch.py`.
- New short session `252f6102a3bb6fdd18933a745a5e1a43`, state `/Users/jaeger/.codex/attended-review/bpr1878-v002-v11vbl/state`, repo root the
  implementing worktree, host started with `serve --tailscale`. One
  candidate, `BPR-1878-tour-authoring-v002` (revision `769c87f57-v002`),
  with one Accept/Reject question. Status: ready, awaiting the owner.
