---
schema: "implement-rfc.card.v1"
id: "BPR-1900"
title: "Tour authoring runs as its own background agent session, with the workspace and read-only tools"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat","related_cards":["BPR-1375","BPR-1873","BPR-1878","BPR-1881","BPR-1892","BPR-1899"]}
acceptance: [{"criterion": "AC1 A drafting request runs in the review's private workspace with read-only tools. A test shows the agent can search the repository, and that a write or edit tool use is refused", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "application_agent_launch: read-only argv for Claude and Codex; Grep, rg and Read decoded as tool activity; Edit, Write, rm -rf, WebFetch, an init with Edit or an MCP server end as tool_refused. core_tour_agent_session covers the allowlist. The offscreen look's stub ran in the agent workspace and searched it.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC2 Tour runs never appear in the Agent tab's chat thread, and chat runs never appear in the tour session", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "observation", "detail": "The tour session is its own agent_invocation_host with its own projection, launcher and lane; AgentHost->dispatch( keeps one call site (application_agent_chat_composition_contract passes). Offscreen look after a tour run: ATTENDED_AGENT_TAB_TURNS:0.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#implementation-2026-11-06"}}, {"criterion": "AC3 The author can keep editing the tour while a drafting run goes, and cancel it from its control", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: Draft the tour says Stop drafting while running and stops it; the Problem takes an edit meanwhile; a field's drafting run shows Stop drafting on its own control and stops from it. The offscreen look wrote the Problem while it drafted.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC4 The tour session's transcript can be opened from the Tour tab", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: Transcript (1) opens it, never by itself; it says Searched, Read and Answered. Offscreen look printed the transcript lines.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC5 BPR-1892's failure notice links to the tour session's run", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_failure (updated): the notice keeps its words; Show the run opens the tour session's transcript at the failed run and does not ask for the Agent tab.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC6 Owner look: an attended Accept/Reject on a frozen build that shows a background drafting run, its transcript, and a whole-tour proposal", "outcome": "pending", "revision": "739dcf4e4", "evidence": {"kind": "observation", "detail": "v001 (8055ab061) rejected by the owner on 2026-11-06 in session 874076eee1b3626c3770e0fcf890cdd9: the first click on Accept highlighted a box instead of accepting, and the box cramped the text. v002 (9025a78f6) rejected on 2026-11-06 in session 099b512a7f255d217c6eed51ba487328 (\"What's up with this?\" on a flagged stop outside the pull request). v003 (739dcf4e4) in new session 286839f31dbf8cbea8b2ca8a93efeeb5, candidate BPR-1900-tour-session (revision 739dcf4e4-v003), ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#attended-review-ac6"}}, {"criterion": "AC7 The RFC amendment records the decisions above, and the documentation gate passes", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1900 records T1 fresh runs, T2 the transcript on the Tour tab, T3 the allowlist per agent, and the drafting/rewriting split; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#implementation-2026-11-06"}}, {"criterion": "AC8 A whole-tour draft arrives as a proposal of the Problem, the Solution and ordered stops, each accepted, edited or discarded on its own; accepting never replaces author text without that gesture. The core validates every proposed stop's anchor against the pull request's files and lines, and a stop that does not anchor is flagged and cannot be accepted", "outcome": "pass", "revision": "8055ab061", "evidence": {"kind": "command", "detail": "core_tour_agent_session: propose_tour anchors against the host's head line counts (not in the pull request, deleted, past the end, line zero, uncounted all flagged), decide_tour_proposal refuses accepting a flagged stop. qt_tour_agent_session_view: edits survive, Accept sends the edit, a flagged stop's Accept is disabled. Offscreen look: 4 proposed stops, the one outside the pull request flagged.", "path": "docs/issues/40-parked/BPR-1900-tour-authoring-agent-session.md#verification-focused-owner-dont-wait-for-tests"}}]
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

## Installed Claude check (2026-11-06, coordinator request before the look)

- `/opt/homebrew/bin/claude` 2.1.310, with the exact read-only drafting
  arguments and environment, in an empty scratch folder outside the
  repository: the init frame's `tools` were `["Bash","Glob","Grep","Read"]`,
  `mcp_servers` `[]`, `plugins` `[]`. Init passes the app's check.
- The offscreen course against that Claude (`--attended-tour-authoring
  /opt/homebrew/bin/claude --attended-tour-drafting`) on v001 ended as
  `tool_refused` every time: Claude chains read-only commands
  (`git diff --stat c58160c b20cb44; git diff c58160c b20cb44 | head -400`),
  writes grep alternations as `"a\|b"`, runs `cd` into the workspace and
  `sed -n` itself, and tries `for` loops that its own allowlist then denies.
  v001's check refused `;`, `&&`, a backslash in double quotes, `cd` and
  `sed`, and ended the run at the first tool use it refused, even one Claude
  was about to deny.
- Fixed on branch `bpr1900-installed-claude-course`: the check reads a
  command as plain commands joined by `|`, `&&`, `||` or `;` (redirection only
  to `/dev/null`), keeps a backslash's character literal, admits the
  read-only programs Claude runs on its own (`cd`, `pwd`, `echo`, `printf`,
  `cut`, `tr`, `diff`, `file`, `stat`, `du`, `tree`, `basename`, `dirname`,
  `realpath`, print-only `sed`, and `git status|rev-parse|shortlog|describe|cat-file|ls-tree|merge-base|name-rev`),
  and holds a shell command it refuses until its result: denied by Claude, the
  transcript says "(not run, outside the read-only list)" and the run goes on;
  ran, the run ends as `tool_refused`. A tool outside the four still ends the
  run at once. The drafting look waits up to ten minutes for a real agent.
- After the fix the same course ran twice to `ATTENDED_TOUR_DRAFTING_SHOWN`
  with a Problem, a Solution and 12 stops on the fixture's own files, all
  anchored, and `ATTENDED_AGENT_TAB_TURNS:0`.

### v001 rejected (2026-11-06)

The owner, verbatim: "Some weird focus thing happens the first click on accept
highlights a box in the pane and doesnt click accept and that box makes the
text feel cramped". Session `874076eee1b3626c3770e0fcf890cdd9` recorded the
Reject; its host is stopped and its candidate released.

### v002 (`9025a78f6`, branch `bpr1900-installed-claude-course`)

`frontend-design:frontend-design` was loaded for the UI work.

- **The first click acts.** Accept and Discard take the keyboard only by Tab,
  not on a press, so a field losing focus mid-click cannot move the button out
  from under the pointer. The proposal and transcript panes never take the
  keyboard, so no box is drawn around the pane.
- **No cramping box.** A proposed field draws no native focus ring inside
  itself. It sits in a frame that paints the base colour inside a quiet
  rounded one-pixel edge, which turns the agent indigo while the field has the
  keyboard, at the same width, so nothing moves or shrinks. The text has 8 px
  of padding, and the field grows to fit its text (three to twelve lines)
  instead of showing a scroll bar. Drawn from the palette, no style sheet, so
  the dark appearance keeps its text colour.
- **Test.** `qt_tour_agent_session_view` now sends one real press and release
  to Accept while a proposed field holds the keyboard: it accepts at once, the
  press leaves the keyboard where it was, focusing a field keeps its room, no
  native focus ring is drawn, and the pane has no focus.
- Also on this branch: the installed-Claude fix above, and the composition
  pin resealed for `main.cpp`'s look (a focused-field picture).
- Focused checks pass: `qt_tour_agent_session_view`,
  `qt_review_tour_agent_failure`, `qt_review_tour_agent_help`,
  `core_tour_agent_session`, `application_agent_launch`,
  `application_review_tour_writing`,
  `application_review_round_trip_composition_contract`; the format check and
  the documentation gate.
- Frozen build `/Users/jaeger/bpr1900-tour-session-v002.f0vneb5e` (executable
  SHA-256 `9bda5ba32cf45d5e559624ac97fa29cd073736dbfc257fb017a58011198d4977`);
  the launcher's own offscreen check reached `ATTENDED_TOUR_DRAFTING_SHOWN`,
  and the frozen executable's offscreen look did too, light and dark, with
  `ATTENDED_AGENT_TAB_TURNS:0`. Launch:
  `python3 /Users/jaeger/bpr1900-tour-session-v002.f0vneb5e/launch.py`.
- New short session `099b512a7f255d217c6eed51ba487328`, state
  `/Users/jaeger/.codex/attended-review/bpr1900-v002-bLsLZy/state`, host
  started with `serve --tailscale`. One candidate, `BPR-1900-tour-session`
  (revision `9025a78f6-v002`), one Accept/Reject question, leading with
  pictures of the frozen build. Status: ready, awaiting the owner.

### v002 rejected (2026-11-06)

The owner attached a picture of the proposal's Stop 4, "A file it imagined"
(`src/not_in_this_pull_request.cpp`, lines 3 to 9): red text "This file is not
in the pull request, so this stop can't be added.", an editable box, a
disabled Accept and Discard. Their note, verbatim: "What's up with this?"
Session `099b512a7f255d217c6eed51ba487328` recorded the Reject; its host is
stopped and its candidate released.

Owner direction for v003 (relayed by the coordinator, 2026-11-06), verbatim:
"It should strongly prefer on hunk. I'm okay with high value off-hunk that will
work on live or i can relocate to on a hunk or in the PR".

### v003 (`739dcf4e4`, branch `bpr1900-installed-claude-course`)

`frontend-design:frontend-design` was loaded for the UI work.

- **Left out, quietly.** `core::propose_tour` leaves out every stop that does
  not anchor to the pull request's files and lines before the proposal is
  shown (`tour_proposal::LeftOut`). The proposal says only "1 suggested stop
  pointed outside this pull request and was left out." (the plural for more,
  nothing when none), in the quiet colour. `core::left_out_entries` keeps each
  one, its path, lines and why, in the run's transcript ("Left out").
  `tour_proposal_item` no longer has a state that disables Accept; nothing in
  the proposal is red or disabled.
- **On the change first.** `github_file_record::HeadHunks` keeps the head lines
  of each hunk of a file's patch from GitHub's own `files` answer (both read
  paths; `core::parse_patch_head_hunks`). The request gives the agent each
  file's `changed_lines` and asks for stops inside them, an off-hunk stop only
  when it adds high value and its first sentence says why. `core::place_tour_stop`
  places each anchored stop as publication would: on the change (inside one
  hunk, or hunks unknown), near it (one line off every hunk: a line comment
  where GitHub takes one, else a file comment with a permalink, so it works on
  GitHub), or off it (several lines not inside one hunk, which a range cannot
  publish). Stops on the change are listed first, then near, then off.
- **Shown.** Near: one small quiet "Outside the change". Off: "Outside the
  change. It won't appear when the tour is published until it's on changed
  lines." and a one-click **Move to nearest change** (`core::relocate_tour_stop`,
  onto the closest hunk of its file, keeping as many lines as fit). Accepted as
  it is, it says it won't appear when published until it's on changed lines,
  and Use selected lines still lets the author choose them.
- A proposed field now fits its text again whenever its width changes (the
  installed Claude's long Problem was clipped).
- Tests: `core_tour_agent_session` (patch hunks, placement on/near/off,
  ordering, relocation onto the nearest hunk and to the last hunk's end,
  refusals, left-out stops and their transcript lines),
  `qt_tour_agent_session_view` (no flag, no disabled button, the quiet line in
  singular, plural and absent, the near label, the off line and Move dispatch,
  the accepted-as-is words), `application_agent_launch` (the request carries
  `changed_lines` and asks for stops on them), and `github_read_contract`,
  `github_paged_read`, `application_github_contract`,
  `application_review_tour_writing` (+ wrapper),
  `application_review_tour_composition_public_path`,
  `application_agent_chat_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `architecture_boundaries`: pass after a full dev build. Format check and
  documentation gate pass.
- Frozen build `/Users/jaeger/bpr1900-tour-session-v003.s5tpfw2_` (executable
  SHA-256 `c8b51526de155a9a9523638293e14aea43a44c6c931bf3c86fc0eb106bf0af8a`):
  the launcher's own check and the offscreen look reach
  `ATTENDED_TOUR_DRAFTING_SHOWN` in light and dark; the stub proposes stops on,
  near and off the change and one outside the pull request, which the
  transcript keeps as left out. Launch:
  `python3 /Users/jaeger/bpr1900-tour-session-v003.s5tpfw2_/launch.py`.
- Installed Claude Code (`/opt/homebrew/bin/claude` 2.1.310) against this same
  frozen executable, offscreen: the whole-tour draft finished end to end,
  "Proposed a Problem, a Solution and 12 stops.", all on the change, with
  `ATTENDED_AGENT_TAB_TURNS:0`; its one `for` loop was denied by Claude and
  shown "(not run, outside the read-only list)".
- New short session `286839f31dbf8cbea8b2ca8a93efeeb5`, state
  `/Users/jaeger/.codex/attended-review/bpr1900-v003-7fONsD/state`, host
  started with `serve --tailscale`. One candidate, `BPR-1900-tour-session`
  (revision `739dcf4e4-v003`), one Accept/Reject question, leading with
  pictures of the frozen build. Status: ready, awaiting the owner.

## Accepted

The owner accepted v003 on 2026-11-06 (attended review, no note).

## Follow-up: drafted stops and the head-changed state (2026-11-07)

The owner, 2026-11-07:
- "Why did my tour draft have this warning at every stop: 'the head
  changed reaffirm or revise before publishing'"
- "there's this button 'Reaffirm explanation for current head' which
  doesnt seem to do anything"
- "I think it's very weird for any of that to be included in the text
  that would get posted to the PR. ... Turn it into a badge/chip on the
  stop outside of the main stop comment content box ... Then click it I
  guess to ask to clear it in a second click maybe."

**Causes.**
- A stop accepted from a proposal (`tour_stop_of`), a split's pieces and
  outline skeletons never recorded the head they were made at
  (`ReaffirmedHead`). The editor compares that head with the tour's
  observed head, so every drafted stop said the head changed when it had
  not.
- The Reaffirm button sent `reaffirm` for the open stop. Core refuses it
  for a stop with no confirmed lines (no `Original`, not published), which
  is what a drafted stop is until Use selected lines. The refusal showed
  only in the tour's status line, worded for marking a stop Ready, so the
  click seemed to do nothing. The button did act on its own card's stop.
  Every per-card action dispatches the stop the card shows, `Selected`;
  only the route's Move, Duplicate, Merge and Remove use the route
  selection.
- The warning was never published. It lived only in the editor's source
  line and its tooltip, and the publication renderers never read it.
  `ReaffirmedHead` is local SQLite state with no GitHub channel.

**Fix.**
- **Core.** Each proposal part carries `Head`, the head its run read: the
  workspace's `PinnedHead`, or else the draft's observed head.
  `tour_stop_of` affirms the stop at that head, and its destination is at
  that head too. Splits use the part's head. Outline skeletons are affirmed
  at the captured tour's head. `reaffirm_need` names what confirming still
  needs: a title, an explanation, or confirmed lines.
- **Host.** The tour run's `PinnedHead` is kept with its workspace path and
  passed to `propose_tour` and `propose_tour_changes`.
- **Qt** (`frontend-design:frontend-design` loaded first). The separate
  button and the source-line warning are gone. A stale stop wears one
  small amber "Head changed" chip by its title, outside the explanation.
  Clicking the chip opens, in place under the title, "The head moved since
  you wrote this. Still accurate?" with Confirm and Edit. Confirm reaffirms
  that card's stop, and Edit opens its explanation. Where confirming can't
  apply yet, the chip names what is missing (for a drafted stop, its lines:
  the card's own Show in code and Use selected lines) and offers no Confirm.
  A refusal is said there in plain words.
- Ready still means the author confirmed the lines. A drafted stop is
  never made Ready by this change.

**Evidence.**
- `core_tour_agent_session`: parts carry the run's head or the observed
  head; the accepted stop is at and affirmed at the run's head, is not
  Ready, and needs its lines before it can be confirmed; split pieces use
  the part's head.
- `qt_review_tour_reaffirm` (new):
  - the chip shows with no stop chosen first, and on a card other than the
    route's selection;
  - Confirm clears exactly that card's stop;
  - a drafted stop from an old head asks for its lines, with no Confirm;
  - a refusal is said on the card;
  - Edit opens the explanation;
  - a stop drafted at the observed head has no chip;
  - after a real head change it has one again;
  - neither the source line nor the text carries the state.
- `application_review_tour_publication_*`: the cold-profile course now
  confirms through the chip, and still makes zero remote writes and keeps
  the imported subject and comment.

### Attended review (follow-up v001)

- Commit `6454480df` on branch `worktree-agent-ad49329075ef77ea5`, not
  landed: the owner asked to look before landing.
- Frozen build
  `/Users/jaeger/.codex/attended-builds/bpr1931-1900-head-chip-v001.zopy_1jf`
  (executable SHA-256
  `c815cf8626d3bd900e05b6ec8a0321d7d6bba45bc07162df77efa35368004698`). The
  launcher's own check reached `ATTENDED_TOUR_REVISION_SHOWN`, and the
  pictures are that build's own offscreen renders.
- Launch:
  `python3 /Users/jaeger/.codex/attended-builds/bpr1931-1900-head-chip-v001.zopy_1jf/launch.py`.
  The chip look is that build's
  `src/build/app-release-tests/tests/adapters_qt/review_tour_reaffirm --look`.
- New short session `7600c02d09fb1415cb1b7f2ac35695a4`, state
  `/Users/jaeger/.codex/attended-review/bpr1931-1900-chip-v001-Hq7c2T/state`,
  host started with `serve --tailscale`.
- Candidates, revision `6454480df-v001`, one Accept/Reject question each:
  - `BPR-1900-head-changed-chip-v001` (the chip UX);
  - `BPR-1931-BPR-1900-fixes-v001` (the reset and drafted-head fixes).
- Status: ready, awaiting the owner.
