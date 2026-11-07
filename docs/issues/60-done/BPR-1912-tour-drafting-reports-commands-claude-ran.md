---
schema: "implement-rfc.card.v1"
id: "BPR-1912"
title: "A tour drafting run reports a shell command Claude approved outside the read-only list instead of ending"
status: "Done"
rfcs: []
depends_on: ["BPR-1900"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06: Draft the tour (0.4.2 build 28) worked for a while, then stopped with tool_refused after ~200 KB of output","related_cards":["BPR-1891","BPR-1896","BPR-1900"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: [{"criterion": "AC1 A read-only drafting run in which Claude runs a shell command outside the app's list (awk 'NR<3' notes.txt, is_error false) finishes with its proposal, and the transcript names the command as run outside the read-only list", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "application_agent_launch: the probe's awk turn decodes to the answer with one tool_activity '(ran, outside the read-only list) awk ...' and one outside_list_ran; tour_transcript_of keeps kind Read with note ran_outside_list. The stub's drafting run now emits the same turn and the offscreen look finishes with the proposal.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}, {"criterion": "AC2 A held command Claude denies still reads '(not run, outside the read-only list)' and the run goes on", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "application_agent_launch: touch made.txt with is_error true lets the run answer and is named not run, not counted.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}, {"criterion": "AC3 A tool_use of a tool other than Read, Grep, Glob or Bash, and an init frame listing another tool or any MCP server, still end the run as tool_refused", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "application_agent_launch: Edit, Write and WebFetch tool uses and both init refusals are unchanged and still refused.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}, {"criterion": "AC4 The proposal says how many commands ran outside the list and links to them in the transcript; a run with none shows nothing extra", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: 'Claude ran 1 command outside the read-only list. See the transcript.' in the quiet colour; its link opens the transcript window at that run with the command and its note; hidden for a run with none. Offscreen look prints ATTENDED_TOUR_OUTSIDE_LIST.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}, {"criterion": "AC5 The agent.run diagnostic of a read-only run carries outside_list=N, and never the command text", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "application_agent_launch: describe and parse round-trip outside_list=2 and outside_list=0; non-count values are refused. diagnosed_agent_process sets it only for a read-only Claude run, from a count; no command text is carried.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}, {"criterion": "AC6 Draft the tour finishes against the installed Claude Code (agent check: offscreen course plus a stub that emits the probe's awk turn; owner look through their own wrapper)", "outcome": "pending", "revision": "63718408a", "evidence": {"kind": "observation", "detail": "Agent half passes: the offscreen --attended-tour-authoring /opt/homebrew/bin/claude --attended-tour-drafting course on 2.1.310 finished with 'Proposed a Problem, a Solution and 12 stops.' and ATTENDED_AGENT_TAB_TURNS:0; the stub's awk turn finishes with its proposal. Owner half pending: it runs through the owner's own wrapper and can be checked only on a prerelease; not asked in the attended review by coordinator direction.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#ac6-owner-half-pending"}}, {"criterion": "AC7 Agent chat (not read-only) is unchanged", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "application_agent_launch: the Agent tab's own runs are not held to the tour's list (Edit decodes); the chat decoder is constructed without read-only tools and never emits outside_list_ran.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}, {"criterion": "AC8 The documentation and RFC amendment are updated, and verify_documentation.py passes", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "ai-agent-boundary.md (Claude's check is the gate, outside list reported), security-threat-model.md (Read-only tour drafting residual), review-tours.md, RFC-BPR-0092 T3 records the supersession; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#implementation-2026-11-06"}}, {"criterion": "AC9 application_agent_launch, core_tour_agent_session, qt_tour_agent_session_view and architecture_boundaries pass", "outcome": "pass", "revision": "63718408a", "evidence": {"kind": "command", "detail": "ctest --preset dev: all four pass, plus qt_review_tour_agent_failure and application_agent_finding_protocol; coding-format-check passes.", "path": "docs/issues/60-done/BPR-1912-tour-drafting-reports-commands-claude-ran.md#verification"}}]
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

## Implementation (2026-11-06)

Decision at pickup, as the card recommended (coordinator, 2026-11-06): a
command that writes or deletes and that Claude ran does not end the run. It
is marked the same way as any command outside the list and named in the
transcript.

- **Decoder** (`src/application/src/agent_launch.cpp`). `read_only_tool_use`
  ends the run only for a tool outside the four. A shell command
  `core::decide_read_only_tool` refuses is held with what it was asked and
  emits nothing yet; `read_only_tool_result` then emits its `tool_activity`
  as `(not run, outside the read-only list) ...` when the result says
  `is_error`, or `(ran, outside the read-only list) ...` plus a new
  `outside_list_ran` event otherwise. Past `maximum_agent_tool_activity`
  held commands a command is named at once as `(unchecked, ...)`, and the run
  goes on. The comments say Claude's permission check is the gate.
- **Core** (`tour_agent_session`). `tour_transcript_note` (none, not_run,
  ran_outside_list, unchecked) on each entry; `tour_agent_run::OutsideList`
  counts commands that ran outside the list, past the entry bound too.
  `transcript_kind_of` shows a command by what it does: reading programs
  (`cat`, `head`, `tail`, `nl`, `sed`, `awk`, `less`, `od`, `column`, `jq`)
  as Read, finding ones as Searched, anything else as Ran.
- **Application.** `tour_transcript_of` turns the decoder's note into the
  entry's note; the agent loop counts `outside_list_ran` into
  `agent_run_result::OutsideListCommands`; `agent_run_diagnosis` writes
  `outside_list=N` for a read-only Claude run, never the command.
- **Qt.** Under the proposal, one quiet line: "Claude ran 1 command outside
  the read-only list. See the transcript." The link opens the transcript
  ([[BPR-1913]]'s window) at that run, scrolled to the first such entry.
  `frontend-design:frontend-design` was loaded for the wording and placement:
  the same quiet colour and size as the left-out line above it, not an
  error, no dialog.
- **Stub.** `scoped_writing_agent`'s drafting run emits the probe's turn,
  `awk 'NR<3' <file>` with `is_error: false`, before it answers.
- **Docs.** `ai-agent-boundary.md`, `security-threat-model.md` (new section
  "Read-only tour drafting (BPR-1900, BPR-1912)" with the residual),
  `review-tours.md`, RFC-BPR-0092 Amendment BPR-1900 T3.

## Evidence (installed Claude, 2026-11-06)

`/opt/homebrew/bin/claude` 2.1.310, no wrapper, the exact
`claude_read_only_restrictions` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`, in a
scratch folder outside the repository holding small text files. No token was
printed.

1. The probe's three commands: init tools `['Bash', 'Glob', 'Grep', 'Read']`,
   `permissionMode: default`, no MCP server. `ls` ran; `awk 'NR<3' notes.txt`
   was **denied** ("This command requires approval"); `python3 -c 'print(42)'`
   was denied. So on 2.1.310 without a wrapper `awk` is not approved: the
   owner's `awk` came from Claude 2.1.314's own classifier, the wrapper's
   permission rules, or both. This Mac cannot tell which without that release
   and wrapper; the threat model records this.
2. Other common read-only programs, the same arguments: ran without asking
   `jq .a data.json`, `ls | xargs wc -l`, `column -t`, `comm`, `od -c`,
   `sed -n 1p`, `cd . && pwd`; denied `awk '{print}'`, `less`, `touch`
   ("needs approval"), `rm`. The transcript's notes for these stay rare:
   Claude runs them only when it chooses them, and the app's list already
   covers the commands it uses most (`rg`, `grep`, `git log|show|diff`).
3. The offscreen course against that Claude
   (`--attended-tour-authoring /opt/homebrew/bin/claude --attended-tour-drafting`,
   commit `63718408a`): it ran `git diff ... --stat; git diff ...`,
   `git ls-files; git grep -n ...; git diff -M ... --name-status`, and
   finished: "Proposed a Problem, a Solution and 12 stops.",
   `ATTENDED_AGENT_TAB_TURNS:0`. No command ran outside the list in that run.

## Verification

Focused, owner: "dont wait for tests". On `63718408a` in the worktree's own
dev build:

- `core_tour_agent_session`: a ran-outside entry is kept under its kind with
  its note and counted, also past the bound; a denied one is not counted;
  `awk` reads, `rm` runs, `ls` searches.
- `application_agent_launch`: AC1 (the probe's `awk`), AC2 (`touch` denied),
  the writes that ran (`touch`, `rm -rf src` without `is_error`) reported and
  not refused, AC3 refusals unchanged, AC5 `outside_list` round trip, AC7.
- `qt_tour_agent_session_view`: AC4 line, link and absence.
- `architecture_boundaries`, `qt_review_tour_agent_failure`,
  `application_agent_finding_protocol`: pass. A full dev build compiled.
- `coding-format-check` passes (clang-format 22.1.8);
  `verify_documentation.py` prints `DOCUMENTATION_OK`.
- Offscreen look with the stub (`attended-drafting-claude`):
  `ATTENDED_TOUR_OUTSIDE_LIST:Claude ran 1 command outside the read-only list. <a href="#outside-list">See the transcript</a>.`,
  transcript line `Read awk 'NR<3' src/engine/renderer.cpp (ran, outside the read-only list)`,
  then `ATTENDED_TOUR_DRAFTING_SHOWN`.

## AC6 owner half (pending)

The owner's half of AC6 runs Draft the tour through the owner's own Claude
wrapper on a frozen build, and can be checked only once a prerelease carries
this change. Pending; by coordinator direction it is not asked in
[[BPR-1913]]'s attended review.

