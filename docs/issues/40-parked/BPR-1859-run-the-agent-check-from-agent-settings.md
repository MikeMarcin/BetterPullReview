---
schema: "implement-rfc.card.v1"
id: "BPR-1859"
title: "Run the agent check from Settings › Agents & context"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-04: \"I want to be able to run agent check from agent settings page\"","reservation":"docs/issues/_reservations/BPR-1859--BPR-1859--ee97c25d4868.json","related_cards":["BPR-1749","BPR-1751","BPR-1753","BPR-1828","BPR-1847"],"frontend_design":"loaded (frontend-design:frontend-design) before the visual change; the page reuses Check it works' marks, colours, progress and Stop and inherits the palette, with no style sheet"}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 The core decides Settings' Check agent: offered, running (not offered again), or disabled with its reason (no agent added, several found and none chosen, the saved agent forbidden by the open review's host); before the agents were looked for, or with a saved agent no longer found, it is offered; the workspace row runs only with a review open", "outcome": "pass", "revision": "6668516c0", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_agent_setup$'", "detail": "agent_setup_tests.cpp test_check_offer: every offer, its order, ChecksWorkspace, stable names."}}, {"criterion": "AC2 The host runs the same check: asked before detection it looks first (shown running, Stop ends it and the later search runs nothing); a second ask while running starts nothing; a saved agent that is gone fails as agent_not_found with no process; checking again after a fix passes and moves the setup exactly as Check it works", "outcome": "pass", "revision": "6668516c0", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_agent_setup_host$'", "detail": "agent_setup_host_tests.cpp test_check_from_settings, beside the unchanged status-check, stop and diagnostics cases."}}, {"criterion": "AC3 Settings › Agents & context shows Check agent under the agent list: disabled with its reason, running with progress and Stop, then its rows and, on failure, the specific failure, its remedy (sign-in command with Copy) and Show details; Check again after a run; marks follow the palette", "outcome": "pass", "revision": "6668516c0", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_agent_setup_view|qt_settings_window.*|qt_workbench_agent_send_refusal)$'", "detail": "agent_setup_view_tests.cpp test_settings_check_agent, with light and dark renders under BPR_SETUP_RENDER_DIR; the setup's own Check it works tests pass on the shared words and marks."}}, {"criterion": "AC4 Composed course: in the real composition, Settings' Check agent is not offered again while the wrapper's status run is held, passes inline, keeps the finished setup finished, and with the program removed fails inline", "outcome": "pass", "revision": "6668516c0", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_agent_invocation_composition_contract$'", "detail": "agent_invocation_composition_public_path.cpp setup-wrapper course, settings-check-* stages, before the existing wizard diagnostics and reset stages."}}, {"criterion": "AC5 Gates: clang-format 22, clang-tidy on the changed translation units, the documentation gate", "outcome": "pass", "revision": "6668516c0", "evidence": {"kind": "command", "command": "cmake --build --preset dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors (9 TUs); python3 scripts/verify_documentation.py --root .", "detail": "Formatting check passed (clang-format 22.1.8, 1926 files). Tidy clean except the pre-existing go_on_to_reviewers naming error in github_application.cpp (recorded on BPR-1847). architecture_boundaries (QToolTip::showText in workbench_window.cpp, from 8d448697b) and command_correctness_registry_contract (BPR-1856's selector case) fail on the base 4605d6461 as well; this card touches neither file."}}, {"criterion": "AC6 The owner judges the look and placement of Check agent on Settings › Agents & context: Accept or Reject", "outcome": "pending", "revision": "6668516c0", "evidence": {"kind": "observation", "detail": "Pending attended review."}}]
---

# BPR-1859: Run the agent check from Settings › Agents & context

Owner, 2026-11-04: "I want to be able to run agent check from agent settings
page".

## What "agent check" was

The agent setup's third step, **Check it works** ([[BPR-1749]],
[[BPR-1751]], RFC-BPR-0092). One press of Check runs the chosen agent's own
status and version commands (or, for a custom agent, launches its program and
its own check) on the setup host's check lane (`agent_setup_host::check`),
and makes, then removes, the open review's workspace
(`check_agent_workspace` in the composition). It was offered only inside the
Agent tab's setup, which shows only while a step is missing or after
**Finish setup** or a turn's **Check again** opens it. Settings › Agents &
context had the agent list, the run limit and Reset agents…, and no way to
run the check: once the setup had finished, re-checking after a fix (signing
in, a new program) meant going back through the setup.

## Change

- Core `bpr/core/agent_setup.hpp`: `agent_check_offer` (available, running,
  no_agent, not_chosen, forbidden), `agent_check_facts` and
  `decide_agent_check`, which also says whether the workspace row runs: only
  with a review open. Without one the row is left not run rather than failed,
  which does not hold the setup back ([[BPR-1847]]).
- Application: `agent_check_facts_of(projection, review open)`. The host's
  `check()` now looks for the agents first when the Agent tab never did
  (the check is shown running throughout and Stop ends it), and a saved
  agent that is no longer found fails as `agent_not_found` with its remedy
  instead of doing nothing. The check itself is unchanged.
- Composition: Settings' Check agent and Stop call the same
  `check_agent_setup` and Stop as the setup's buttons; the offer is presented
  whenever the setup publishes and when a review opens or closes.
- Qt: `agent_programs_panel` has Check agent under the agent list, the line
  beside it (what runs, or why it cannot), progress and Stop while it runs,
  and the rows, a failure's words and remedy (with the sign-in command and
  Copy) and Show details on the page. The setup's words, marks and agent name
  are shared functions now (`setup_agent_name`, `agent_check_words`,
  `setup_check_mark`), used by both.
- `docs/ux/workbench-specification.md` describes it.

Because the check updates the same setup state, a pass after a fix moves the
setup on exactly as Check it works does: the setup finishes, and the Agent
tab's suggested prompts ([[BPR-1847]]) show once it has.

## Review (AC6)

Attended review session `ee11f630d0bdb0f2e3a34e289413d051`, state
`/Users/jaeger/.codex/attended-review/bpr1859-v001-jPKCNC/state`, one
Accept/Reject question (`BPR-1859-check-agent-look`) on the look and
placement, against the frozen build BPR-1859-v001 at 6668516c0
(`/Users/jaeger/BPR-attended/BPR-1859-v001.fpj7_d0e`, executable SHA-256
fde0580d…, launcher check `ATTENDED_FILE_SHOWN`). Lifecycle: prepared and
served (Tailscale), awaiting the owner.

## Open

- `architecture_boundaries` and `command_correctness_registry_contract`
  fail on the base as well (see AC5); neither is this card's.
- A check run from Settings with no pull request open leaves the workspace
  row not run. The setup counts that as finished ([[BPR-1847]]), as it does
  for a reopened application, so the workspace is first made at the next Ask.
