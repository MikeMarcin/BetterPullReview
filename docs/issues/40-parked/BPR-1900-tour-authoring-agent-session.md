---
schema: "implement-rfc.card.v1"
id: "BPR-1900"
title: "Tour authoring runs as its own background agent session, with the workspace and read-only tools"
status: "Parked"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat","related_cards":["BPR-1375","BPR-1873","BPR-1878","BPR-1881","BPR-1892","BPR-1899"]}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 A drafting request runs in the review's private workspace with read-only tools. A test shows the agent can search the repository, and that a write or edit tool use is refused", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "application_agent_launch: read-only argv for Claude and Codex; Grep, rg and Read decoded as tool activity; Edit, Write, rm -rf, WebFetch, an init with Edit or an MCP server end as tool_refused. core_tour_agent_session covers the allowlist. The offscreen look's stub ran in the agent workspace and searched it.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC2 Tour runs never appear in the Agent tab's chat thread, and chat runs never appear in the tour session", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "observation", "detail": "The tour session is its own agent_invocation_host with its own projection, launcher and lane; AgentHost->dispatch( keeps one call site (application_agent_chat_composition_contract passes). Offscreen look after a tour run: ATTENDED_AGENT_TAB_TURNS:0.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#implementation-2026-11-06"}}, {"criterion": "AC3 The author can keep editing the tour while a drafting run goes, and cancel it from its control", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: Draft the tour says Stop drafting while running and stops it; the Problem takes an edit meanwhile; a field's drafting run shows Stop drafting on its own control and stops from it. The offscreen look wrote the Problem while it drafted.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC4 The tour session's transcript can be opened from the Tour tab", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: Transcript (1) opens it, never by itself; it says Searched, Read and Answered. Offscreen look printed the transcript lines.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC5 BPR-1892's failure notice links to the tour session's run", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_failure (updated): the notice keeps its words; Show the run opens the tour session's transcript at the failed run and does not ask for the Agent tab.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC6 Owner look: an attended Accept/Reject on a frozen build that shows a background drafting run, its transcript, and a whole-tour proposal", "outcome": "pending", "revision": "8055ab061", "evidence": {"kind": "observation", "detail": "Session 874076eee1b3626c3770e0fcf890cdd9, candidate BPR-1900-tour-session (revision 8055ab061-v001), ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#attended-review-ac6"}}, {"criterion": "AC7 The RFC amendment records the decisions above, and the documentation gate passes", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1900 records T1 fresh runs, T2 the transcript on the Tour tab, T3 the allowlist per agent, and the drafting/rewriting split; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#implementation-2026-11-06"}}, {"criterion": "AC8 A whole-tour draft arrives as a proposal of the Problem, the Solution and ordered stops, each accepted, edited or discarded on its own; accepting never replaces author text without that gesture. The core validates every proposed stop's anchor against the pull request's files and lines, and a stop that does not anchor is flagged and cannot be accepted", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "core_tour_agent_session: propose_tour anchors against the host's head line counts (not in the pull request, deleted, past the end, line zero, uncounted all flagged), decide_tour_proposal refuses accepting a flagged stop. qt_tour_agent_session_view: edits survive, Accept sends the edit, a flagged stop's Accept is disabled. Offscreen look: 4 proposed stops, the one outside the pull request flagged.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}]
---

# BPR-1900: Tour authoring runs as its own background agent session, with the workspace and read-only tools

## Owner direction (2026-11-06)

"Feels like something like `rg` might be useful for a tour writer to gather
context." And: "to me the tour itself is much more of a second agent session,
separate and mostly background from my main agent chat session."

Owner addition (2026-11-06, relayed by the coordinator): "And then it can
fill its tour stops and problem and solution from all that context."

## Today

Tour drafting ("Draft from PR context", "Suggest an outline") uses BPR-1375's
tool-free writing launch: an empty `bpr-writing-XXXXXX` folder, no tools, and
only the context the app puts in the prompt. Tour runs also show up in the
Agent tab's chat thread as turns titled "Review this pull request", mixed in
with the reviewer's own conversation.

## Change

1. **A tour agent session per review.** Tour authoring gets its own agent
   session, separate from the Agent tab's chat thread. It uses the same agent,
   model and effort the reviewer picked (the BPR-1878 bottom bar). Tour runs no
   longer appear as turns in the Agent tab's chat.
2. **The workspace and read-only tools for drafting.** "Draft from PR context"
   and "Suggest an outline" run in the BPR-1873 private workspace (from the
   linked or kept clone), with read-only tools: read, search (`rg`/grep/glob),
   and read-only git (log, show, blame, diff). No edits, no writes, no
   network, no MCP. Use the agent's own sandbox and the existing
   workspace-run path. Small rewrites (Enhance, Generate comment, tour
   message) may stay tool-free and fast. Record the split.
3. **Mostly in the background.** The author keeps working while a drafting run
   goes. Progress shows on the control that started it, in the BPR-1881 agent
   accent. The result lands in the tour where it was asked for. The session's
   transcript (what it read, what it answered) can be opened from the Tour tab,
   but is never pushed in front of the author.
4. **Failures.** BPR-1892's notice links to the tour session's run instead of
   the Agent tab's chat. Its wording stays the same.
5. **Setup.** The BPR-1881 call to action stays the gate. Without a clone,
   drafting falls back to the pull request's files (BPR-1873's "This pull
   request only"), and says so.

6. **Draft the whole tour** (owner addition). The tour session can draft the
   whole tour, not just one field: the Problem and Solution statements and the
   tour stops (which files and lines to visit, in what order, and each stop's
   explanation), from the workspace context it gathered with its read-only
   tools. It runs in the background like the other drafting. The result
   arrives as a proposal the author reviews and accepts, edits or discards,
   per field and per stop; it never silently overwrites text the author
   already wrote. It reuses "Suggest an outline" or "Draft from PR context"
   if they fit, or adds one clear "Draft the tour" action in the agent accent.
   Stops anchor to real lines in the pull request's diff or files: the core
   validates them, and drops or flags a stop that does not anchor.

Settle and record in an RFC-BPR-0092 amendment:
- Does the tour session keep its conversation across drafting requests
  (`--resume`), or is each request a fresh run with the tour so far as
  context? Recommend one.
- Where its transcript opens: a panel in the Tour tab, or the Agent tab with a
  session switcher.
- Which tool allowlist each agent kind (Claude, Codex) uses for read-only work.

## Acceptance

- AC1. A drafting request runs in the review's private workspace with
  read-only tools. A test shows the agent can search the repository, and that
  a write or edit tool use is refused.
- AC2. Tour runs never appear in the Agent tab's chat thread, and chat runs
  never appear in the tour session.
- AC3. The author can keep editing the tour while a drafting run goes, and
  cancel it from its control.
- AC4. The tour session's transcript can be opened from the Tour tab.
- AC5. BPR-1892's failure notice links to the tour session's run.
- AC6. Owner look: an attended Accept/Reject on a frozen build that shows a
  background drafting run, its transcript, and a whole-tour proposal.
- AC7. The RFC amendment records the decisions above, and the documentation
  gate passes.
- AC8. A whole-tour draft arrives as a proposal of the Problem, the Solution
  and ordered stops, each accepted, edited or discarded on its own; accepting
  never replaces author text without that gesture. The core validates every
  proposed stop's anchor against the pull request's files and lines, and a
  stop that does not anchor is flagged and cannot be accepted.

## Implementation (2026-11-06)

`frontend-design:frontend-design` was loaded before the UI work.

- **Core** (`bpr/core/tour_agent_session.hpp`): `tools_for` and
  `tour_request_of` record the split (Draft the tour, Draft from PR context on
  an empty field and Suggest an outline read the repository; Enhance, Enhance
  selection, Generate comment stay tool-free); `decide_read_only_tool` is the
  read-only allowlist check (four tools; one plain pipeline of `rg`, `grep`,
  `ls`, `cat`, `head`, `tail`, `wc`, `nl`, `find` and read-only `git`, with
  writing or running arguments refused); the session's bounded transcript
  (`begin_tour_run`, `record_tour_activity`, `finish_tour_run`, one run at a
  time); and the whole-tour proposal (`propose_tour`, `anchor_tour_stop`,
  `decide_tour_proposal`, `tour_stop_of`).
- **Application**: `agent_launch_options::ReadOnlyTools` adds
  `claude_read_only_restrictions` (Claude) or `codex_read_only_arguments`
  (Codex `exec`); the decoder refuses an init frame with another tool or an
  MCP server and any refused tool use (`agent_protocol_failure::tool_refused`)
  and reports the rest as `tool_activity`, collected into
  `agent_run_result::ToolActivity`. `agent_invocation_request::ReadOnlyTools`
  and `MeasurePaths`: the host counts the head lines of the pull request's
  files in the new workspace before the agent starts
  (`measure_workspace_lines`). `tour_agent_drafting.hpp` builds the
  whole-tour request (`bpr.tour-draft.v1`) and reads its answer with the
  core's strict JSON reader. The BPR-1375 writing launch itself is unchanged.
- **Composition**: the tour session is its own `agent_invocation_host` with
  its own launcher, lane (`TourAgentEffects`) and projection, on the same
  Claude wrapper, program and added-agent ports. Every tour writing run goes
  there (`AgentHost->dispatch(` keeps its single call site); the Agent tab's
  chat never sees a tour run, and a tour Stop stops only the tour's run.
- **Qt** (`tour_agent_panel`): **Draft the tour** in the agent accent at the top
  of the Tour tab (**Stop drafting** while it runs), the proposal under it (each
  part's text editable in place, Accept or Discard, a field the author wrote
  says accepting replaces it, an unanchored stop is flagged in red with Accept
  disabled), and **Transcript (n)**, which opens the session's runs only when
  asked. Draft from PR context and Suggest an outline say **Stop drafting** on
  their own control while they run; leaving the window no longer stops a
  drafting run. BPR-1892's notice keeps its words; its button is **Show the
  run** and opens the transcript at that run.
- **Look**: `--attended-tour-drafting` with `--attended-tour-authoring
  {build}/tests/end_to_end/attended-drafting-claude/claude`: the stub
  (`test_scoped_writing_agent` in that folder) really walks the workspace,
  reports a Grep, an `rg`, a Read and a `git log` as Claude Code does, and
  answers with a tour on the pull request's own files plus one stop on a file
  outside it; the look presses Draft the tour, writes a Problem while it runs,
  waits for the proposal and opens the transcript.
- RFC-BPR-0092 amendment T1 to T3 (fresh runs, the transcript on the Tour tab,
  the allowlist per agent);
  `docs/architecture/ai-agent-boundary.md#the-tour-sessions-read-only-drafting-bpr-1900`
  and `docs/architecture/review-tours.md`.

### Verification (focused, owner: "dont wait for tests")

- `core_tour_agent_session` (new): tools split, allowlist (searches and git
  reads pass; rm, redirection, sequence, substitution, globs, `find -delete`,
  `sort -o`, `rg --pre`, `git commit|push|-c|--output`, env assignments,
  network refused), transcript bounds, proposal anchoring and decisions.
- `application_agent_launch` (extended), AC1: exact read-only argv for Claude
  and Codex, App Server and plain text refused; a Grep, an `rg` and a Read
  decoded as tool activity and shown inside the workspace; Edit, Write,
  `rm -rf`, WebFetch, an init with Edit and an init with an MCP server all end
  as `tool_refused`; the Agent tab's own runs are not held to it;
  `measure_workspace_lines` skips a link, a folder, an escape and an absolute
  path; the tour answer decoder refuses extra keys, another schema and prose.
- `application_agent_finding_protocol` (failure count),
  `qt_tour_agent_session_view` (new, AC3/AC4/AC8), `qt_review_tour_agent_failure`
  (updated, AC5), `qt_review_tour_agent_help`, `qt_review_tour_agent_setup_call`,
  `application_review_tour_writing`, `application_review_tour_writing_wrapper`,
  `application_review_tour_composition_public_path`,
  `application_agent_chat_composition_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`: pass.
- Offscreen look on the dev build, light and dark:
  `ATTENDED_TOUR_DRAFTING_RUNNING`, then
  `ATTENDED_TOUR_TRANSCRIPT:Run 1, Draft the tour: answered | Asked Draft the tour | Searched draw in . | Searched rg -n "clear|present" src/engine/renderer.cpp | Read src/engine/renderer.cpp | Ran git log --oneline -3 | Answered Proposed a Problem, a Solution and 4 stops.`,
  `ATTENDED_AGENT_TAB_TURNS:0` (AC2), `ATTENDED_TOUR_DRAFTING_SHOWN`.
- `coding-format-check` passes; `verify_documentation.py` prints
  `DOCUMENTATION_OK`.

## Attended review (AC6)

- v001: frozen build of `8055ab061` at
  `/Users/jaeger/bpr1900-tour-session-v001.ckwcyqwa` (executable SHA-256
  `697ed5fd26dc935b8be71fe9b963e6dbb8cdea532a2a91eabb01e3bfe76a95ea`),
  fixture `conventional-comments-v1` on `src/engine/renderer.cpp`, ready line
  `ATTENDED_TOUR_DRAFTING_SHOWN`; the launcher's own offscreen check reached
  it. Launch: `python3 /Users/jaeger/bpr1900-tour-session-v001.ckwcyqwa/launch.py`.
  Pictures from that executable, offscreen: the window while Draft the tour
  runs, the Tour tab with the proposal, every proposed part, the transcript,
  and the proposal in the dark appearance.
- New short session `874076eee1b3626c3770e0fcf890cdd9`, state
  `/Users/jaeger/.codex/attended-review/bpr1900-v001-VbjQNf/state`, repo root
  the implementing worktree, host started with `serve --tailscale`. One
  candidate, `BPR-1900-tour-session` (revision `8055ab061-v001`), one
  Accept/Reject question. Status: ready, awaiting the owner.
- The frozen commit `8055ab061` stays reachable on the local branch
  `bpr1900-v001-frozen-history`. The card's branch was then squashed and
  rebased onto `main` past BPR-1899 (its thinking-block handling sits beside
  the read-only tool check in `decode_claude_frame`); the focused tests, the
  format check, the documentation gate and the offscreen look pass again on
  the rebased commit.
