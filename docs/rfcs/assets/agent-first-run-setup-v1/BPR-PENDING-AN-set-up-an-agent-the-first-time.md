---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AN"
title: "Set up an agent the first time the Agent tab opens"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"kind":"design","origin":"Owner, 2026-10-22, relayed by the coordinator","owner_note":"I think we need some ux for setting up your agent the first time you go to the agent tab. Maybe a little wizard I'm not sure.","phase":"1 (investigation and design); no product code changed; awaits the owner's decisions","draft_rfc":"docs/rfcs/assets/agent-first-run-setup-v1/RFC-DRAFT-0026-agent-first-run-setup.md","design_frames":"docs/design/BPR-PENDING-AN-agent-setup-v1/index.html","frontend_design":"frontend-design:frontend-design loaded before the design frames","requested_skill":"frontend-design","related_cards":["BPR-1594","BPR-1684","BPR-1301","BPR-1375","BPR-1179"],"related_rfcs":["RFC-BPR-0011","RFC-BPR-0012","RFC-BPR-0060","RFC-BPR-0085"]}
---

# BPR-PENDING-AN: Set up an agent the first time the Agent tab opens

## Owner request

The owner, 2026-10-22, verbatim:

> I think we need some ux for setting up your agent the first time you go to
> the agent tab. Maybe a little wizard I'm not sure.

## Phase 1 finding

The request is right, and there is a defect under it. Observed on
`b69b82c9b`, rendered offscreen with a fresh profile, light and dark:

- The Agent tab opens on a conversation. Its "Set up an agent" empty state
  can only show when no profile is offered, which never happens on
  github.com or GHES.
- The agent menu is the four built-in profiles by protocol name, with
  "Codex App Server" selected first. Nothing is looked for until Ask.
- The first Ask ended "Codex App Server stopped before answering. Finished:
  failed_to_start." The Codex and external terminal profiles name a bare
  program, and only Claude's launch is resolved to an absolute path, so a
  shipped build can never start Codex (the Unix supervisor refuses a
  relative program; Windows passes it to `CreateProcessW` unsearched).
- The owner's own first run (2026-10-22) chose **External terminal
  harness** for a Claude wrapper and ended "Finished: workspace_unavailable"
  with a raw Git log: the review workspace could not be made from the cache
  (a missing parent commit, fixed separately), and no agent ran. The words
  blamed the harness, and the only actions were Ask again and the log.
- The MCP helper needs no user setup, and `bpr` ships in no release package.

Details, sources and the proposal are in
[RFC-DRAFT-0026](RFC-DRAFT-0026-agent-first-run-setup.md);
the frames, including the real renders, are in
[the design page](../../../design/BPR-PENDING-AN-agent-setup-v1/index.html).

## Proposal in one paragraph

The first time, the Agent tab is a three-step setup: **Agent** (found by
file metadata, shown with its path, chosen for you when there is one),
**Check it works** (one bounded run of the agent's status command, and one
private copy of this pull request made and removed, each with its own result
in plain words and a recovery action) and **Ready** (one green line, then the
question field). The choice is saved for the profile; History and Settings
run setup again, it returns when the saved agent is gone, and a later Ask
that fails for a setup reason goes back to that check. Codex is found as
Claude is; the plain-text harness leaves the menu.

## Decisions for the owner

Q1 to Q6 in the RFC's "Decisions for the owner", asked one per question in a
fresh attended-review session. The session ID, state directory and status
are recorded here when it opens; its URL is not.

## Plan after the answers

Estimated size: medium, five cards, about two to three agent days.

1. Core setup model (state, rows, policy filtering, check decoding) and a
   Codex locator with a Codex program path, under Q4 and Q5.
2. Application setup host: metadata search on show (Q2), the bounded check
   (Q3), the saved choice, Send to Agent's default.
3. Qt setup in the Agent tab (Q1) with every state in the frames, light and
   dark, the History and Settings entries and the partly set up callout.
4. The AI agent boundary, Settings and workbench specification amendments.
5. A frozen build and the owner's attended look (RFC AC09).

## Acceptance

- The owner's answers to Q1 to Q6 are recorded here and in the RFC, and the
  RFC is adopted, revised or set aside to match.
