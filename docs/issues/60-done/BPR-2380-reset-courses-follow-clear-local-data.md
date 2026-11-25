---
schema: "implement-rfc.card.v1"
id: "BPR-2380"
title: "The wrapper-setup and prompt-macro courses reset agents through Clear local data's Agent setup, as the product does since BPR-2377"
status: "Done"
rfcs: []
depends_on: ["BPR-2377"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"coordinator, 2026-11-24: application_agent_invocation_composition_contract fails at wrapper-setup-durable-reset","reservation":"docs/issues/_reservations/BPR-2380--BPR-2380--c3d09a31df8c.json","revision":"d1999f485","related":["BPR-2377","BPR-1769","BPR-2146","BPR-1755"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2380: The courses reset agents through Clear local data

## Report

`application_agent_invocation_composition_contract` failed at
`wrapper-setup-durable-reset` on main `15a9fffb2`. The `setup-wrapper` phase
triggered History's **Reset agents…** (`agentResetAgentsAction`) and
answered a `QMessageBox` carrying a Reset button. It then waited for the
agent setup store to be empty, the legacy Claude wrapper and model to be
cleared (with Mark read kept), and the setup to say "No agent found".

## Cause

The product is right; the course was stale. BPR-2377 made Reset agents… a
category of Clear local data. The action now opens Settings › Storage &
privacy with Agent setup checked
(`github_application::implementation::open_clear_local_data`), Clear… asks
one `profile_reset_confirmation`, and the confirmed request goes to the
panel's dispatch. The profile desktop closes the profile's windows and
reopens it with the request, and `app::run_profile_data_reset` performs the
reset as the profile starts. No `QMessageBox` appears and nothing is reset
in place, so the course's wait could never succeed.

`application_prompt_macros_public_path` (`ask_menu_public_path.cpp`,
`macros`) had the same dependency but passed vacuously. Its
`tab->reset_agents()` only opened Settings, so "Reset agents changes no
prompt" was checked without anything having been reset.

## Change

Tests only; no product source, manifest or digest changes.

- `tests/end_to_end/agent_invocation_composition_public_path.cpp`
  (`setup-wrapper`): triggers the real `agentResetAgentsAction`, waits for
  Clear local data to show in Settings with exactly Agent setup checked and
  Clear… enabled, clicks Clear…, confirms the `profileResetConfirmation`
  (copy off), and takes the request at `set_profile_data_reset_dispatch`
  where the desktop would. The course was started without the desktop, so
  after checking that nothing was published it shuts the application down
  and restarts it on the same root with `DataReset` set to that request,
  the way the desktop reopens a profile. The startup report must show Agent
  setup cleared with no failure. The course then reconnects, opens the same
  pull request, opens the setup and runs the original durable check
  unchanged: the agent setup store is empty with `AgentsMoved`, the legacy
  ClaudeWrapper and ClaudeModel are empty, Mark read is kept, the setup
  says "No agent found", and nothing was published.
- `tests/end_to_end/ask_menu_public_path.cpp` (`macros`): the same route
  through Clear local data. The request runs at the restart the course
  already makes. The startup report and the empty agent store are checked,
  and then the existing check that the chip turned off and the new order
  survive (now "survive Reset agents and a restart") is proved after a real
  reset.

Search for other dependants: `QMessageBox::Reset` and "Reset agents?" have
no other answerers in `tests/` or `scripts/`. The only object name BPR-2377
removed is the per-row `profileReset%1Name`, and nothing references it.
`qt_workbench_window_*` still drives `agent_tab_view::reset_agents` through
its own reset dispatch with no route set. That is the widget's remaining
fallback path, so it is left alone.

## Acceptance

- AC1. The `setup-wrapper` phase drives Reset agents… → Clear local data
  (Agent setup checked) → Clear… → confirmation, and the reset runs as the
  profile reopens.
- AC2. The phase still proves the durable reset: agents gone, legacy
  wrapper and model cleared, other preferences kept, the setup back at "No
  agent found", nothing published.
- AC3. The prompt-macros course proves a real Agent setup clear changes no
  prompt.
- AC4. The relevant application_agent_*, setup, ask and prompt-macro
  courses pass. Format and tidy are clean on the touched units.

## Evidence

Revision `d1999f485` (commit `d1999f485` on main `15a9fffb2`), macOS arm64
Debug, `ctest --test-dir build/dev`. These rows ran on `d1999f485`'s tree.

- AC1, AC2: `application_agent_invocation_composition_contract` passed
  (`agent-invocation-setup-wrapper:pass`).
- AC3: `application_prompt_macros_public_path` passed (`ask-menu-macros:pass`)
  with the startup report and empty agent store checked after the clear.
- AC4: `ctest -R '^application_(agent_|setup|native_agent|prompt_macros|ask_|profile_data_reset)'`
  ran 76 tests, and all 76 passed. That covers every
  application_agent_invocation_* (positive, construction-cut,
  launcher-construction-cut, gesture-cut, cancellation, staleness,
  cleanup-retry, workspace-diagnostic, native-pinned),
  application_agent_finding_*, application_agent_chat_*,
  application_agent_queue_*, application_agent_setup_host(_policy_public_path),
  application_native_agent_sessions, application_setup_destination(_public_path),
  application_profile_data_reset_public_path, application_ask_* and
  application_prompt_macros_public_path.
- AC4: `_coding-standard/scripts/format_check.py` passed on both files.
  `_coding-standard/scripts/tidy.py` passed on both units, and no warning is
  on a changed line (the remaining findings are on lines this card did not
  change).
