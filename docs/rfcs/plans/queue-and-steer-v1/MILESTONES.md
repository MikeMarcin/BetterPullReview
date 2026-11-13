# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **queue-and-steer-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## queue-and-steer

In the Agent tab and tour chats the reviewer queues follow-ups that BPR releases one by one after each run truly ends, and steers a running Claude or Codex App Server turn natively, at no extra inference cost.

**Work:** `BPR-2151`, `BPR-2152`, `BPR-2153`, `BPR-2154`, `BPR-2155`

**Prerequisites:** None

**Completion:**

- Queue and Steer exist only on resumed paths; fresh one-offs and Ask more behave as RFC-BPR-0101's amendment of 2026-11-12 says.
- A queued entry is released only after durable completion and host retirement, with its captured context, and pauses rather than retargets or retries.
- No input is lost or resent automatically across crash, write and save races; uncertain delivery stays visible.
- Installed Claude and Codex App Server each accept a native correction on the running conversation; unsupported drivers keep Queue without a paid probe.
- Queue maintenance adds no model call, and a steered run's native parts count once.
- The composed Qt flow shows pending, paused, sent and uncertain input with Stop reachable and keyboard and accessibility actions.
- Stop all in the agent footer stops every active turn and pauses every queue; a queue belongs to its session, not to a window, and a message being edited is held.

**Boundary:** Agent tab, tour chats and the local draft's discussion (owner, Q2). Enhance, comment workshops, Review Cycle Send to Agent, external terminals and fresh first drafts are outside; the BPR-1930 feedback buffer is folded into the queue (owner, Q1). No model API client, SDK runtime or second agent loop.
