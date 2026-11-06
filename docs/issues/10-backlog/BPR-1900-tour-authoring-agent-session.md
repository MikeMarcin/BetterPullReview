---
schema: "implement-rfc.card.v1"
id: "BPR-1900"
title: "Tour authoring runs as its own background agent session, with the workspace and read-only tools"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat","related_cards":["BPR-1375","BPR-1873","BPR-1878","BPR-1881","BPR-1892","BPR-1899"]}
acceptance: []
---

# BPR-1900: Tour authoring runs as its own background agent session, with the workspace and read-only tools

## Owner direction (2026-11-06)

"Feels like something like `rg` might be useful for a tour writer to gather
context." And: "to me the tour itself is much more of a second agent session,
separate and mostly background from my main agent chat session."

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
  background drafting run and its transcript.
- AC7. The RFC amendment records the decisions above, and the documentation
  gate passes.
