---
schema: "implement-rfc.card.v1"
id: "BPR-1876"
title: "A failed agent answer offers Ask again, and says when its cause is fixed"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: confused by a failed answer's Check again button after the agent setup check passed","related_cards":["BPR-1847","BPR-1753","BPR-1817","BPR-1859"],"frontend_design":"loaded (frontend-design:frontend-design) before v001 and again before v002; no new visual language: a fixed error drops the danger look for the plain reply card and opens with the setup's own passed check mark and a bold line; Delete is a flat footer button beside Resend, in the footer's existing style","attended_session":{"id":"4c9a03488091c0f8aa684e74294ab271","state":"/Users/jaeger/.codex/attended-review/bpr1876-v002-aqvxcy/state","candidate":"BPR-1876-failed-answer-v002","transport":"serve --tailscale","lifecycle":"v002 ready, awaiting the owner"},"previous_sessions":["42d77107a2dc91f52a120fa3063b0ebc (v001, rejected 2026-11-06 with a note; host stopped)"],"delete_scope":"Delete is offered on any turn whose message never reached the agent (failed_to_start, workspace_unavailable, history_unavailable) while nothing runs; a message the agent received keeps no Delete, Clear session still clears everything"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 Pressing a failed answer's action starts a new run of the same question; covered by a Qt or public-path test","outcome":"pass","revision":"4ea7b6578","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_agent_turn|qt_workbench_agent_send_refusal|qt_agent_launch_panel)$'","detail":"core: decide_agent_turn_actions offers resend (renamed from ask_again in v002) for every ending but an unreadable answer (Agent settings…), and Delete only for a message that never reached the agent (agent_turn_never_sent), on any turn while nothing runs. Qt test_a_failed_answer_asks_again_and_says_when_fixed: a typed message whose run fails to start offers Resend (the stopped banner too, never Check again); pressing it dispatches the same text in the same session and scope again, opens no setup, and the admitted run is a new turn of the same message. Delete on the latest and on an earlier unsent turn removes each, the thread observer keeps the rest, and the stopped banner goes with the latest; a stopped or unreadable turn offers no Delete."}},{"criterion":"AC2 After the cause clears, the failed answer reads as fixed-since with Ask again; while the cause holds, it shows the cause","outcome":"pass","revision":"4ea7b6578","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_agent_turn|qt_workbench_agent_send_refusal)$'","detail":"core test_fixed_since: only failed_to_start (agent check) and workspace_unavailable (workspace check) can be fixed since, on any turn, and fixed since changes nothing else. Qt: a pass from before the failure, and a running check, leave the cause shown (danger, 'Error'); a pass after the failure shows 'Fixed since: my-agent passed its check. Resend to try it now.' on the latest (Resend kept, 'Error, fixed since', stopped banner hidden) and the line without the sentence on an earlier failure; a check failing again shows the cause again; a workspace failure clears only on the workspace check."}},{"criterion":"AC3 The owner accepts the look (attended review)","outcome":"pending","revision":"4ea7b6578","evidence":{"kind":"observation","detail":"v001 (session 42d77107a2dc91f52a120fa3063b0ebc, candidate BPR-1876-failed-answer-v001 at 9a33d8dea) rejected 2026-11-06, decision a9194b41-fab0-4dd0-bc1b-8b1cd52d375b: \"We should also be able to delete a message that was never sent to the agent instead of just resend. Also I think ask again is probably too specific because it wont always be a question so maybe just resend.\" v002 (session 4c9a03488091c0f8aa684e74294ab271, candidate BPR-1876-failed-answer-v002 at 4ea7b6578): one Accept/Reject question on offscreen renders of the Agent tab, light and dark."}},{"criterion":"AC4 python3 scripts/verify_documentation.py --root . passes","outcome":"pass","revision":"4ea7b6578","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .","detail":"DOCUMENTATION_OK after merging origin/main."}}]
---

# BPR-1876: A failed agent answer offers Ask again, and says when its cause is fixed

## Observed

The owner, 2026-11-05. An agent question failed ("Stopped after 0:19"). The
failed answer card, saved in the agent tab, carries a **Check again** button.
Pressing it re-runs the agent setup check (the green "<agent> is ready" box
refreshes and passes) but does not re-send the question, so the red failed
answer stays, still showing its old cause. The owner read the red box as the
check result and concluded the check still failed.

## Change

- A failed answer's action is **Ask again**: it re-sends the same question
  (same prompt, same context) as a new run. Keep any setup re-check as a
  step that run performs, not as the button's whole effect.
- Once the cause the failed answer names is no longer true (the setup check
  passes, or the workspace cause is gone), the failed answer says so, e.g.
  "Fixed since. Ask again", instead of presenting the old cause as current.
- The setup check's own control keeps its own wording and is visually
  distinct from a failed answer.

## Acceptance

- AC1. Pressing a failed answer's action starts a new run of the same
  question; covered by a Qt or public-path test.
- AC2. After the cause clears, the failed answer reads as fixed-since with
  Ask again; while the cause holds, it shows the cause.
- AC3. The owner accepts the look (attended review).
- AC4. `python3 scripts/verify_documentation.py --root .` passes.

## Fix

- Core `bpr/core/agent_turn.hpp`: `agent_turn_recovery::check_setup` is
  removed; a start or workspace failure asks again like every other ending
  but an unreadable answer (still **Agent settings…**). The run that Ask
  again starts makes the workspace and starts the agent, so it performs the
  check of both. `agent_turn_cause_of` names the setup check whose pass
  clears a failure (agent check for `failed_to_start`, workspace check for
  `workspace_unavailable`); `agent_turn_facts::CausePassedSince` in,
  `agent_turn_actions::FixedSince` out.
- Qt `agent_tab_view`: a pass counts as made since a turn ended only once the
  check was seen not passed (running, failed or not run) after it ended, so a
  pass from before the failure says nothing. A fixed error drops the danger
  look for the plain card, is described "Error, fixed since", and opens with
  the setup's passed check mark and "Fixed since: <agent> passed its check."
  (or "the workspace check passed."), plus "Ask again to send this question
  now." on the latest. The stopped banner's Check again is gone (it asks
  again), and the banner stands down while the latest turn is fixed since.
  `adopt_setup` no longer takes the check callback it no longer uses
  (one call site in `github_application.cpp`).
- `docs/ux/workbench-specification.md` describes both.

## Attended review (AC3)

- v001: session `42d77107a2dc91f52a120fa3063b0ebc` (state
  `/Users/jaeger/.codex/attended-review/bpr1876-v001-2vrITi/state`),
  candidate `BPR-1876-failed-answer-v001` at `9a33d8dea`. **Rejected**
  2026-11-06: "We should also be able to delete a message that was never
  sent to the agent instead of just resend. Also I think ask again is
  probably too specific because it wont always be a question so maybe just
  resend." Host stopped.
- v002: session `4c9a03488091c0f8aa684e74294ab271` (state
  `/Users/jaeger/.codex/attended-review/bpr1876-v002-aqvxcy/state`),
  candidate `BPR-1876-failed-answer-v002` at `4ea7b6578`, one Accept/Reject
  question. The material is offscreen renders of the Agent tab from
  `test_a_failed_answer_asks_again_and_says_when_fixed`, light and dark.

## v002 (owner look v001)

- **Resend** replaces Ask again everywhere: the failed answer, the stopped
  banner ("Your message is kept."), the Fixed since line ("Resend to try it
  now."), the tooltip, and the workspace stop reasons that said "ask again"
  (`agent_launch_panel.cpp`). Core `agent_turn_recovery::resend`.
- **Delete** on a turn whose message never reached the agent: core
  `agent_turn_never_sent` (did not start, no workspace, private history could
  not record it) and `agent_turn_actions::Delete`. Chosen scope: any such
  turn, not only the latest, while nothing runs; it leaves the thread and the
  kept thread (the thread observer records the rest), finding cards under
  later turns keep their turn, and the stopped banner goes with the stopped
  run's turn until the next run. A message the agent received (stopped,
  timed out, an error exit, unreadable, set aside) keeps no Delete: it is part
  of that agent's conversation, and Clear session still clears all.

No frozen build: a fixture build opens the agent setup at its first step,
which covers the conversation, and once an agent is chosen in it a review
whose program is missing ends `failed_to_start` without a turn being drawn
(only the stopped banner), so a launcher cannot reach a failed answer card
without new test-only hooks. That second point is also a product gap (open
below).

## Open

- A run that fails before its workspace phase (a missing program once the
  setup has a chosen agent) leaves no turn in the thread, only the stopped
  banner: `agent_tab_view::progress` begins a turn on the idle-to-preparing
  phase change, which such a run never shows. Not changed here.
- Resend re-sends the message with the composer's current context (the
  path the earlier Ask again already used, now named Resend), not a snapshot of the failed
  turn's chips.
- `tests/workbench_case_inventory.json`'s `source_sha256` (and the command
  registry's) were already stale against `workbench_window_tests.cpp` on the
  base; the coordinator's reseal covers them.
