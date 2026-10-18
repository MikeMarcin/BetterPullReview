# BPR-1594 handoff: Agent is a drawer tab (in progress)

- Worktree: `/Users/jaeger/.codex/worktrees/bpr1594-agent-tab/BetterPullReview`,
  branch `bpr-1594-agent-tab` (pushed to origin), branched from
  `bpr-1593-tours` at `202e6b3d3`, because it builds on BPR-1592/1593's
  provisional Agent tab. BPR-1593 was accepted; Codex lands it (item 7c of
  `docs/issues/_checkpoints/codex-handoff-2026-10-17.md`) from
  `/Users/jaeger/.codex/worktrees/bpr1574-drawer-redesign`: do not touch that
  worktree, its server or its frozen build. Merge `origin/main` here once
  BPR-1593 is on `main`.
- Build tree: `build/dev` in this worktree (configured with the shared CPM
  cache symlinked from the main checkout).
- Scope (RFC-BPR-0085 D5, AC07): the Agent tab after Tour; ⌥⇧A opens the
  drawer on it; ⌘I and the code's context menu "Ask about these lines" open
  it with the selected lines attached; context row (profile, head, state,
  scope); conversation with finding cards (promote reads Draft comment, Ask
  follow-up, Add to notes, More: Draft a defect, Dismiss); question field with
  context chips; History menu (sessions, earlier heads, private runs); four
  states: not set up, working (Stop), head moved (Start a new session,
  Continue on the old head), stopped (what happened, Ask again, Show the log).
  Read-only authority unchanged.
- Done: worktree and branch; this checkpoint.
- Next: configure and build (`cmake --preset dev` after
  `mkdir -p build && ln -s /Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache build/.cpm-cache`,
  then `cmake --build build/dev`); render the mockup `docs/design/BPR-1574-drawers-and-agent-v1/agent.html?still=agent-B`;
  survey `agent_launch_panel`, `side_chat_panel`, `finding_queue_view`,
  `private_review_panel` and the window's Agent tab (`tray_findings_tab`).
- Open design questions: none yet.
- Running PIDs: none. Review session: none yet.
