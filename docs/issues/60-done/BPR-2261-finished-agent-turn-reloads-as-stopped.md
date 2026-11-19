---
schema: "implement-rfc.card.v1"
id: "BPR-2261"
title: "A finished Agent-tab review reloads as \"Stopped after 16:46\", because a helper's report-back holds the turn open with no timeout and quitting saves it as cancelled"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2261--BPR-2261--349c7b58c335.json","found_by":"owner, 2026-11-18, build 68 on macOS: an Agent-tab review showed its full report, the owner quit and reopened the app, and the same turn came back as a red error block, 'Stopped after 16:46.', with the report glued onto the narration and no report card. A later Check of the agent also took about four minutes making and removing a workspace, which looked like every message setting up the workspace again","related_cards":["BPR-2004","BPR-2152","BPR-2246","BPR-2051"],"related_rfcs":["RFC-BPR-0103"]}
acceptance: []
---

# BPR-2261: A finished Agent-tab review reloads as "Stopped after 16:46", because a helper's report-back holds the turn open with no timeout and quitting saves it as cancelled

## Observed

The owner, 2026-11-18, build 68 on macOS, with a custom agent of the Claude
stream JSON protocol, on a large repository.

1. They asked the Agent tab to **Review this pull request**. The run used
   three helpers. The tab rendered the full report as a report card, with
   suggested comments (Draft / Dismiss), and showed no error.
2. They quit the app and reopened it. The same turn now showed:
   - a red error block ending `Stopped after 16:46.`, with a **Delete**
     button;
   - the progress narration with the report appended to it and no
     separator, as in `…carries state over between them.# Review of #N`
     followed by `**Verdict:** Ready with comments`;
   - no report card, though the suggested comments still showed.
3. They sent a follow-up message in the same tab. The tab said the agent was
   setting up the workspace, which looked like a new workspace for every
   message, although one workspace is shared per pull request.

The session log before the restart:

| Event | `utc_ms` | Detail |
|---|---|---|
| `agent.workspace.started` (`workspace_materialize`) | 1795038636586 | |
| `agent.workspace.succeeded` | 1795038748402 | 111815 ms |
| `agent.run.started` | 1795038748412 | `protocol=claude_stream_json` |
| `agent.run.cancelled` | 1795039642874 | `outcome=cancelled stdout_bytes=630479 helpers_started=3 helpers_finished=3 helpers_failed=0 longest_silence_ms=15076 read_past=system/thinking_tokens,system/permission_denied` |
| `application.shutdown` | 1795039662144 | 19 s after the cancel |

The run lasted 894 s. With the 112 s of workspace setup that makes 16:46,
the time shown. The run was recorded as cancelled when the app quit, after
it had already shown a finished report.

The session log after the restart:

| Event | `utc_ms` | Detail |
|---|---|---|
| `agent.setup_check.started` (`agent_custom_check`) | 1795039751078 | |
| `agent.workspace.started` (`workspace_materialize`) | 1795039751081 | 3 ms after the check |
| `agent.workspace.succeeded` | 1795039872436 | 121356 ms |
| `agent.workspace.succeeded` (`workspace_cleanup`) | 1795039998866 | 126429 ms |
| `agent.run.started` | 1795040115866 | 117 s after the cleanup, nothing logged between them |
| `agent.run.succeeded` | 1795040127901 | 12 s |

The session before the restart has the same materialize → cleanup pair
beside its setup check.

## Cause

### A. The turn never ends after a helper reports back

1. A native Agent-tab connection stays open between turns. Its run ends only
   when the decoder says the turn is complete
   (`src/adapters/agent/src/agent_protocol_loop.cpp`, around line 70). Process
   exit ends only non-native runs (lines 78–96).
2. On the `result` frame the decoder keeps the turn open while a helper is
   running, or while `NotifiedSinceInit` is set
   (`src/application/src/agent_launch.cpp`, around line 1819:
   `open = !aHelpers.Running.empty() || aHelpers.NotifiedSinceInit`, so
   `Phase = 3`). A `task_notification` sets `NotifiedSinceInit` (around line
   848). Only the next `init` of the same session clears it (around line
   1579).
3. This run had 3 helpers start and 3 finish, so `Running` was empty. Only
   `NotifiedSinceInit` could have held the turn open. Most likely a helper
   reported back and the report was answered within the same part of the
   turn, so no further `init` came. The raw stream was not kept, so this
   step is the best-supported hypothesis, not a proven trace.
4. Phase 3 has a bounded wait only when a steering correction is awaited:
   `awaiting_successor()` requires `SuccessorAwaited`
   (`agent_launch.cpp`, around line 4465), and
   `abandon_overdue_successor()` (`agent_protocol_loop.cpp` lines 233–249)
   does nothing otherwise. A turn held open by a report-back therefore waits
   for ever, showing "is finishing" (`agent_run_status.cpp`, around line 91).
   `longest_silence_ms=15076` does not contradict this: silence is measured
   only when bytes arrive (`agent_protocol_loop.cpp`, around lines 420–425),
   so the quiet stretch after the last frame was never counted.
5. While a run is open (`Ended=false`), its streamed text renders as a
   report. The card is withheld only once a turn has ended without an answer
   (`src/core/src/agent_report.cpp`, around line 150;
   `agent_tab_view.cpp`, around line 1966). That is why the owner saw a
   finished report.

### B. Quitting saves an answered turn as cancelled

1. `destroy()` calls `cancel()` (`agent_invocation_host.cpp`, around lines
   1704–1712), so the open run's outcome becomes `cancelled`.
2. The conversation record sets `Outcome` from `result.Outcome` alone:
   anything but `success` is `ended_without_answer` (around line 1222). A
   successful `result` frame that had already arrived is lost.
3. `turn.Seconds` is measured from admission (around line 1223), so the
   stopped time includes the workspace setup.
4. On reload, `decide_agent_turn_actions` gives every outcome other than
   `answered` the error style, Delete and the "Stopped after" line
   (`src/core/src/agent_turn.cpp`, lines 91–96; `agent_run_status.cpp`,
   around lines 176–184). The report card is withheld ([[BPR-2246]]).
5. The saved answer joins `AssistantItems` with no separator
   (`agent_invocation_host.cpp`, around lines 1205–1210), so the narration
   and the report run together (`…between them.# Review`).

RFC-BPR-0103 ("A turn cannot end silently") says every turn ends as an
answer, a stop by the reviewer, or an error that says what happened, and is
never left as a clock. Both A and B break that.

### C. The workspace check looks like per-message workspace setup

1. The retained workspace is shared per pull request: it is keyed by the
   pull request's authority and its base and head (`agent_invocation_host.cpp`,
   around lines 866–871). It is reused across Agent-tab turns (around lines
   932–956 and 1097), as `docs/architecture/system-architecture.md` (around
   line 134) says. The follow-up turn logged no materialize. The workspace
   on disk was at the tab's head.
2. The materialize → cleanup pair is the agent setup check, not a turn. It
   starts 3 ms after `agent_custom_check`. **Check it works** and
   Settings → **Check agent** (`window_agent_setup.cpp`, around lines
   1407–1470 and 1806) call `probe_workspace` (`agent_invocation_host.cpp`,
   around lines 2045–2087). That builds a full throwaway workspace and
   removes it, as the settings text says: "It also makes the open pull
   request's workspace once, then removes it."
   (`agent_setup_settings.cpp`, around line 892). On a large repository
   that is about four minutes.
3. The agent's own check is kept across restarts ([[BPR-2051]],
   `agent_setup_host.cpp`, around lines 318–320), but the workspace check is
   not. So setup asks for Check again after every restart, and each Check
   pays the full clone again.
4. Reattaching to the retained workspace runs an unlogged verification
   (`epoch_application_host.cpp`, around lines 209–218). While it runs, the
   turn says "%1 is making its workspace at the head, %2."
   (`agent_run_status.cpp`, around line 152), although nothing is being made.
   No log line accounts for the 117 s between the cleanup and
   `agent.run.started`.
5. A workspace directory's name (`betterpullreview-agent-<32 hex>`) is a
   random token (`installed_git_process.cpp`, around lines 8482–8492), not a
   commit. Here it happened to start with the head's first seven characters.
   The agent read it as the head and said so in its narration. Nothing breaks
   because of it, but it misleads anyone who reads the folder name.

## Change

### 1. End a turn whose report-back was answered

- When the `result` frame arrives with no helper running, end the turn
  unless a helper's report-back is still unanswered. An assistant message
  after the last `task_notification` answers it, so clear
  `NotifiedSinceInit` there and not only on `init`.
- Give every phase-3 wait a bound, not just the steering successor's. If the
  follow-on turn's first frame does not come within the bound (the same
  `SteerSuccessorMilliseconds`, or its own setting), end the turn on the
  answer it already gave, as `abandon_successor()` does.
- Log the wait and its bound, so a held turn shows in diagnostics as held
  rather than as silence.

### 2. Never downgrade a received answer

- Record on the run that a successful `result` frame arrived. A later
  cancel or stop (quit, Stop, a lost connection) keeps outcome `success`
  and the turn is saved as `answered`, as long as no helper is still running.
- With a helper still running, keep today's cancelled outcome, but keep the
  answer text, and make the saved record say the turn was stopped at quit.
- On quit with a run open, save the turn as stopped by the app closing, not
  as an error the reviewer must explain.

### 3. Keep narration and report apart

- Join saved `AssistantItems` with a blank line (`\n\n`). Better still, keep
  the final report as its own item, so a reload renders the report card the
  same way the live view did.

### 4. Make the workspace check cheap and say what it does

- Let **Check it works** / **Check agent** verify the pull request's retained
  workspace when one exists (the same check reattaching uses) instead of
  making and removing a throwaway clone. Make a throwaway one only when no
  retained workspace exists, and keep it as the retained one afterwards
  rather than removing it.
- Keep the workspace check's result across restarts, beside the agent's own
  check ([[BPR-2051]]), keyed by the same authority and base/head binding.
- Log the reattach verification as its own operation (for example
  `workspace_verify`) with its duration.
- While it runs, say "%1 is checking its workspace at the head, %2." and
  keep "making" for an actual materialize.

### 5. Tests

- Application decoder: a Claude stream JSON fixture where one helper
  reports back, the assistant answers, and `result` arrives with no further
  `init`. The turn ends as answered. A fixture where `result` arrives while
  the report-back is unanswered and no frame follows ends after the bound,
  on the answer given.
- Invocation host: cancel after a successful `result` with no helper
  running saves `answered`. Cancel with a helper still running saves the
  stopped outcome with its text. `destroy()` follows the same rules.
- Core: a reloaded answered turn has no error, no Delete and no "Stopped
  after". The saved answer keeps the narration and the report as separate
  paragraphs or items.
- App composition (fixture course): run a review whose helper reports back,
  quit while it would have been held, reopen. The turn shows as answered
  with its report card.
- Setup: with a retained workspace, **Check it works** does not
  materialize or clean up. Its result survives a restart. The reattach
  status line says "checking".

## Acceptance

- AC1. A Claude stream JSON turn whose helpers have all finished and whose
  report-backs are answered ends when its `result` arrives, and never waits
  without a bound.
- AC2. A turn that received a successful `result` is saved and reloaded as
  answered, with its report card, whether the app later quits, the reviewer
  stops it, or the connection closes, as long as no helper was still running.
- AC3. A reloaded turn shows its narration and its report separately, the
  way the live view showed them.
- AC4. Checking an agent with a retained workspace for the open pull
  request does not clone the repository again. The workspace check's
  result survives a restart.
- AC5. Status text says "making its workspace" only while a workspace is
  being made. Reattach verification is logged with its duration.
- AC6. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Keeping the raw agent stream for diagnosis. It would have turned step A.3
  from a hypothesis into a trace, and may deserve its own card.
- Making a first workspace faster on a large repository.
- Workspace directory naming. The random token is fine. Only the misleading
  match in this one case is noted here.

## Built

- **Decoder** (`agent_launch.cpp`). The card's rule (an assistant message
  after the last `task_notification` answers it) is wrong against the
  recorded Claude Code 2.1.319 traces: in
  `claude-2.1.319-helpers-stdin-closed.jsonl` the last report (line 51)
  is followed by the rest of a message already being written (line 53,
  the same message id), and its answer comes only in the next part (init
  at line 58). With the card's rule that run would end one part early.
  The rule kept is narrower: a report is answered inside its part when a
  tool result goes back to the agent after it and the agent then writes a
  message with a new id (the report is drained into the next request at a
  tool-result boundary). The traces still pass unchanged.
- **Bound** (`agent_protocol_loop.cpp`). A native Claude turn held after
  its successful result with every helper ended (and no steering
  successor awaited) waits at most `HeldTurnMilliseconds` (30 s, the same
  as `SteerSuccessorMilliseconds`), then ends on the answer it gave; the
  connection is not reused. A wait for helpers still running stays
  unbounded: a helper may legitimately run for many minutes, and Stop or
  its own end ends it. The run's line logs `held_ms`, `held_bound_ms`,
  `held_out=on` and `stopped_after_answer=on`.
- **Never downgrade** (`agent_protocol_loop.cpp`, `agent_invocation_host.cpp`).
  A cancel or time-out after a successful result with no helper out keeps
  outcome `success`, so the turn is saved answered (quit, Stop, time-out).
  With a helper out the run is still cancelled, keeps its text, and on quit
  is saved with the new outcome `closed_with_app`: not an error, Resend
  and Delete, no keep action, footer "Stopped when the app closed, after
  16:46". The legacy outcomes table's CHECK is rebuilt to allow it.
- **Paragraphs**. Saved `AssistantItems` are joined with a blank line, so a
  reload's report starts at the start of a line and gets its card.
- **Workspace check**. Check it works / Check agent verify the pull
  request's retained workspace (the reattach check) when one exists, and
  clone nothing; a workspace a turn holds now passes as it is. With none,
  one is made and removed as before. A passed check is kept under the
  retained workspace's key (`agent_workspace_key`: authority, base, head;
  at most 16, latest last) in the setup preferences and restored for that
  pull request after a restart; a failure forgets it. Reattach
  verification logs `workspace_verify` with its duration; while it runs
  the turn says "%1 is checking its workspace at the head, %2." (a
  `workspace_check` stream event), and the tour chat says the same.

## Hypotheses

- A.1, A.2, A.4, A.5 and B.1–B.5 are confirmed by the
  `application_agent_chat_held_turn_public_path` fixture course: a scripted
  Claude replays the recorded trace through the part whose result arrives
  with all three helpers ended and the last report unanswered, then sends
  nothing. Without this change the turn is held ("is finishing") until the
  app quits and reloads as an error with no report card.
- A.3 is not confirmed. No raw stream of the owner's run exists, and the
  recorded traces show the opposite ordering (a message after the
  notification that does not answer it). The bound ends the turn whatever
  held it, so A.3 is no longer needed for the fix.

## Decisions for the owner

- The held bound is 30 s (`HeldTurnMilliseconds`), the steering successor's
  value. The answering part's init normally follows the result at once.
- Quitting while a helper still runs saves the turn as "Stopped when the
  app closed, after M:SS", presented plainly (not red), with Resend and
  Delete and no Draft/Keep.
- Check does not keep a throwaway workspace as the retained one. A
  retained workspace belongs to a native conversation record
  (`native_agent_workspaces.conversation` references a session), which owns
  its lifetime guard and cleanup; a check has no conversation to give it.
  So the first Check on a pull request still clones once and removes it;
  every later Check, and every Check after a turn has made the shared
  workspace, verifies without cloning, and its result survives restarts.

## Verification

Lane `claude/turn-reload-fix`, dev preset, macOS: 122 focused tests
matching `agent|operation_diagnos|sqlite_agent_setup|architecture_boundar|core_boundary_contract`,
including `application_agent_helper_turn`, `application_agent_invocation_host`,
`application_agent_setup_host`, `core_agent_turn`, `core_operation_diagnosis`,
`sqlite_agent_setup_storage`, `qt_agent_launch_panel`, `qt_agent_activity_view`,
`application_agent_chat_held_turn_public_path`, the helpers courses and
`architecture_boundaries`. Shown failing without the fix: the decoder test
(rule disabled) and the held-turn course (stop-after-answer disabled:
restored `closed_with_app`, no card). Format check passes on the changed
files; clang-tidy reports nothing on changed lines. Documentation gate
passes. Not run: the whole suite.
