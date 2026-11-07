---
schema: "implement-rfc.card.v1"
id: "BPR-1912"
title: "A tour drafting run reports a shell command Claude approved outside the read-only list instead of ending"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1900"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06: Draft the tour (0.4.2 build 28) worked for a while, then stopped with tool_refused after ~200 KB of output","related_cards":["BPR-1891","BPR-1896","BPR-1900"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: []
---

# BPR-1912: A tour drafting run reports a shell command Claude approved outside the read-only list instead of ending


## Observed

The owner, 2026-11-06, BetterPullReview 0.4.2 build 28 (includes BPR-1900).
Claude Code 2.1.314, run through a wrapper program. A tour drafting run
searched and read for a while, then stopped:

- Card: "It tried a tool a drafting run may not use, so the run was stopped."
- Session log: `agent.run.failed protocol=claude_stream_json
  outcome=protocol_failure failure=tool_refused stdout_bytes=199847
  stderr_bytes=0`. The log does not say which command.

A probe ran the wrapper with `claude_read_only_restrictions` verbatim, in a
scratch folder holding one text file, and asked for three shell commands:

```text
init tools: ['Bash', 'Glob', 'Grep', 'Read'] permissionMode: default
tool_use  Bash: ls
  result: is_error=False 'notes.txt'
tool_use  Bash: awk 'NR<3' notes.txt
  result: is_error=False 'one\ntwo'
tool_use  Bash: python3 -c 'print(42)'
  result: is_error=True 'This command requires approval'
result: success is_error: False
```

`awk` is not in `--allowedTools` and Claude ran it without asking. `python3`
was denied, as it should be.

## Cause

1. Claude Code approves, without asking and in `default` permission mode, the
   shell commands it classifies as read-only, whatever `--allowedTools`
   lists. BPR-1900 already met this on 2.1.310 (`cd`, `wc`, `sort`, `uniq`,
   `cut`, `sed -n`) and added those programs to `read_only_programs`
   (`src/core/include/bpr/core/tour_agent_session.hpp`). On 2.1.314 `awk` is
   one more, and Claude's set will keep changing between releases. A wrapper's
   or policy's own permission rules can widen it further.
2. `read_only_tool_use` (`src/application/src/agent_launch.cpp`) holds a
   command `core::decide_read_only_tool` refuses. `read_only_tool_result`
   then ends the run as `tool_refused` when the command's result does not say
   it was denied (`is_error: true`).
3. That check acts after the fact. When it fires, the command has already
   run, so ending the run prevents nothing. It only throws away the draft. And
   each read-only command Claude newly approves breaks drafting until BPR
   ships that program's name.

What actually acts before a command runs is Claude's own permission check
(`--allowedTools`, `--disallowedTools`, `--permission-mode default`, print
mode denying anything that would ask), plus the run starting in the review's
private, disposable workspace. Agent chat already relies on exactly that.

## Decision (owner, 2026-11-06)

Option 1 of three considered:

- **Accept what Claude's permission check let run, and report it.** A shell
  command outside BPR's read-only list whose result says it ran no longer
  ends the run. The transcript shows it as run outside the list, and the
  `agent.run` diagnostic counts such commands.
- **Unchanged:** a `tool_use` of any tool other than Read, Grep, Glob and
  Bash ends the run at once (it was never loaded), and so does an init frame
  that lists another tool or any MCP server. A held command that Claude denied
  still shows "(not run, outside the read-only list)" and the run goes on.

Rejected: add `awk` to `read_only_programs` with argument checks (fixes today
only), and drop Bash from drafting (loses `git log`/`show`/`diff`).

To settle at pickup: whether a command that clearly writes or deletes (one the
core refuses for `argument`, or a program such as `rm`, `mv`, `touch`) and
that Claude ran should still end the run. Ending it still prevents nothing,
but it says loudly that the read-only premise broke (for example, a wrapper
that allows everything). Recommended: do not end the run. Mark the run's
result, so the proposal view says a command outside the read-only list ran,
and name the commands in the transcript.

## Change

### 1. Decoder

- `read_only_tool_result`: a held command whose result does not say it was
  denied is no longer `tool_refused`. Release it from `aHeld` and emit a
  `tool_activity` event (or amend the held one) whose text says it ran
  outside the list, for example "(ran, outside the read-only list) awk 'NR<3'
  notes.txt". A denied held command keeps its "(not run, …)" text.
- `read_only_tool_use`: unchanged for a tool outside the four. The cap on
  held commands (`maximum_agent_tool_activity`) must not end a long run: once
  results release held entries this rarely binds, but if it does, stop
  holding (report the command as unchecked) rather than refusing.
- Update the [[BPR-1900]] comments above both functions: Claude's permission
  check is the gate; this app reads what it decided and names what ran outside
  its own list.

### 2. Transcript and proposal

- `tour_transcript_of` / `transcript_kind_of`
  (`src/core/src/tour_agent_session.cpp`): a command that ran outside the
  list appears under its usual kind (Searched, Read or Ran) with the "outside
  the read-only list" note, bounded like every entry.
- The finished run records that it ran N commands outside the list. The Tour
  tab's proposal shows one quiet line ("Claude ran 1 command outside the
  read-only list. See the transcript."), linking to the transcript entry. Not
  an error, not a dialog. Load `frontend-design:frontend-design` for the
  wording and placement.

### 3. Diagnostics

Add `outside_list=N` to the `agent.run` line of a read-only run, next to
`plugins=` and `memory=`. Never log the command text: it can contain paths
and search terms from the repository.

### 4. Tests

`tests/application/agent_launch_tests.cpp`, the BPR-1900 block:

- "a write that ran is refused" (`touch made.txt` + `outcome(false)`) and "a
  result that does not say it was denied is refused" (`rm -rf src` without
  `is_error`): both become accepted runs whose activity says the command ran
  outside the list. Whether `rm`/`touch` stay refused depends on the decision
  above.
- Add the probe's case: `awk 'NR<3' notes.txt` with `is_error: false`, then
  an answer. The run finishes with the answer and one "ran, outside" entry.
- Keep "an edit is refused", "a write is refused" (the Write tool), "the
  network is refused" (WebFetch) and both init refusals unchanged.
- `core_tour_agent_session`: a transcript entry for a command that ran
  outside the list.
- `qt_tour_agent_session_view`: the proposal's line and its link to the
  transcript entry.

### 5. Documentation

- `docs/architecture/ai-agent-boundary.md`, the read-only Claude launch
  paragraph (from "The decoder then holds the run to the list itself"):
  Claude's permission check is the gate. Claude approves its own read-only
  commands beyond `--allowedTools` (2.1.310: `cd`, `sed -n`; 2.1.314: `awk`).
  A command outside this app's list that ran is reported, not refused.
  The admitted-programs list stays as the list of commands the transcript
  shows without a note.
- `docs/architecture/security-threat-model.md`: add the residual. A read-only
  run is only as read-only as Claude's permission check, plus whatever a
  wrapper's or policy's settings add to it, and it is bounded by the private
  workspace. This app sees what ran and names it. It cannot undo it.
- RFC-BPR-0092 Amendment BPR-1900: record this decision and that it
  supersedes the "ran, the run ends as `tool_refused`" rule.

## Acceptance

- AC1. A read-only drafting run in which Claude runs a shell command outside
  the app's list (fixture: `awk 'NR<3' notes.txt`, result `is_error: false`)
  finishes with its proposal. The transcript names the command as run outside
  the read-only list.
- AC2. A held command Claude denies still reads "(not run, outside the
  read-only list)" and the run goes on.
- AC3. A `tool_use` of a tool other than Read, Grep, Glob or Bash, and an init
  frame listing another tool or any MCP server, still end the run as
  `tool_refused`.
- AC4. The proposal says how many commands ran outside the list and links to
  them in the transcript. A run with none shows nothing extra.
- AC5. The `agent.run` diagnostic of a read-only run carries
  `outside_list=N`, and never the command text.
- AC6. Draft the tour finishes against the installed Claude Code. Agent check:
  the offscreen `--attended-tour-authoring <claude> --attended-tour-drafting`
  course, plus a stub that emits the probe's `awk` turn. Owner look: an
  attended Accept/Reject on a frozen build, run through their own wrapper.
- AC7. Agent chat (not read-only) is unchanged.
- AC8. The documentation and RFC amendment above are updated.
  `python3 scripts/verify_documentation.py --root .` passes.
- AC9. `application_agent_launch`, `core_tour_agent_session`,
  `qt_tour_agent_session_view` and `architecture_boundaries` pass.

## Evidence to gather first

1. With the installed Claude Code and no wrapper, rerun the probe's three
   commands with `claude_read_only_restrictions`. If `awk` also runs there,
   this is Claude's own classifier and not the wrapper. Record which in the
   threat model.
2. Note which other common read-only programs the installed Claude approves
   on its own (`awk`, `jq`, `xargs`, `less`, `column`, `comm`, `od`), so the
   transcript's "outside the list" notes stay rare.

## Out of scope

- Logging which command ended a run in the earlier, refusing builds.
- Codex read-only runs: their commands are shown and not re-checked already.
