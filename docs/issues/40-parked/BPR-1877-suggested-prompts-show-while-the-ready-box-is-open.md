---
schema: "implement-rfc.card.v1"
id: "BPR-1877"
title: "Suggested prompts show while the agent-ready box is open"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind": "defect", "found_by": "owner, 2026-11-05: suggested prompts don't come up until the green '<agent> is ready' box is closed, which is confusing", "frontend_design": "frontend-design:frontend-design loaded before the change; no new visual language: the existing Ready card and prompt buttons, with the prompts placed straight under the card in the empty tab's line's place. Offscreen renders inspected light and dark."}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 With a passing check and the ready box open, suggested prompts are visible and clickable; covered by a Qt test", "outcome": "pass", "revision": "4856202af", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_agent_turn|qt_workbench_window_scale_100|qt_workbench_agent_send_refusal)$'", "detail": "core: decide_agent_suggestions no longer takes a Ready fact (agent_turn_tests). Qt test_an_agent_error_is_an_error_and_suggestions_wait_for_setup: setup adopted, check passed, present_ready: the Ready card, the suggestions and their toggle are shown, Finish setup is not, every prompt button is visible and enabled, and clicking the first sends exactly its prompt as a chat message while the card stays open. test_the_agent_is_a_drawer_tab: with the Ready card open the suggestions show, enabled, below the card. A failing check still shows 'Suggested prompts appear once the agent is set up.' with Finish setup (unchanged assertion), now whatever else is on screen."}}, {"criterion": "AC2 The owner accepts the look (attended review)", "outcome": "pending", "revision": "4856202af", "evidence": {"kind": "observation", "detail": "Pending: attended session 02b279f80e5efa9486ed9be5d25ee4d5 (state /Users/jaeger/.codex/attended-review/bpr1877-v001-3DK7bz), candidate BPR-1877-prompts-beside-ready-v001, frozen root /Users/jaeger/BPR-attended/BPR-1877-v001.ccivjr31 (launcher --check passed)."}}, {"criterion": "AC3 python3 scripts/verify_documentation.py --root . passes", "outcome": "pass", "revision": "4856202af", "evidence": {"kind": "command", "command": "python3 scripts/verify_documentation.py --root .", "detail": "DOCUMENTATION_OK."}}]
---

# BPR-1877: Suggested prompts show while the agent-ready box is open

## Observed

The owner, 2026-11-05. In the agent tab, the suggested prompts do not appear
until the green "<agent> is ready" setup-check box is dismissed. Nothing says
the prompts are waiting behind it, so the tab looks like it has none.

## Change

- A passing ready check never hides the suggested prompts: show them together
  (the ready box compact, the prompts usable at once), or let a passing check
  collapse itself.
- A failing check may still lead, but must not leave the prompt area blank
  without saying why.

## Acceptance

- AC1. With a passing check and the ready box open, suggested prompts are
  visible and clickable; covered by a Qt test.
- AC2. The owner accepts the look (attended review).
- AC3. `python3 scripts/verify_documentation.py --root .` passes.

## Root cause

[[BPR-1847]] read the setup's Ready line (its fourth step) as still part of
the setup: `core::decide_agent_suggestions` returned `setup_unfinished`
while `ReadyShowing` was set, so the prompts and their toggle stayed hidden
until the card was dismissed, and Finish setup was hidden too because the
Ready card was up. The owner accepted that reading in BPR-1847's session
(2026-11-04) and reversed it here.

## Built

- Core (`bpr/core/agent_turn.hpp`): `agent_suggestion_facts` loses
  `ReadyShowing`; a passed check's Ready line never holds the prompts back.
  An agent or repository still to choose, a failed or running check and the
  setup on screen still do.
- Qt (`agent_tab_view`): the prompts and their toggle show with the Ready
  card open; Finish setup shows for an unfinished setup whatever else is on
  screen, so the prompt area is never blank without saying why.
  `present_ready` and the card's dismiss refresh the whole tab, so the Ready
  line takes the empty tab's line's place at once and the prompts follow
  straight under it, and closing it brings the line back.
- `docs/ux/workbench-specification.md` says so.

## Verification

- `ctest --preset dev -R '^(core_agent_turn|qt_workbench_window_scale_100|qt_workbench_agent_send_refusal|command_correctness_registry_contract)$'`: pass.
- clang-format 22 on the five changed C++ files. clang-tidy on
  `agent_turn.cpp`, `agent_turn_tests.cpp`, `agent_tab_view.cpp`: one
  finding, `to_end` at `agent_tab_view.cpp` line 1761, from bdd2e68cfa,
  outside this change.
- Offscreen renders `agent-ready.png` and `agent-ready-dark.png` (with
  `BPR_AGENT_RENDER_ROOT`) inspected and embedded in the attended candidate.

## Attended review

Session `02b279f80e5efa9486ed9be5d25ee4d5`, state
`/Users/jaeger/.codex/attended-review/bpr1877-v001-3DK7bz`, one candidate
`BPR-1877-prompts-beside-ready-v001` (Accept or Reject), frozen build
`/Users/jaeger/BPR-attended/BPR-1877-v001.ccivjr31` at 4856202af, launched with
`python3 /Users/jaeger/BPR-attended/BPR-1877-v001.ccivjr31/launch.py`.
Status: open, waiting on the owner.
