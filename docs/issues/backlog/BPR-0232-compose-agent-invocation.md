---
id: BPR-0232
title: Compose explicit agent invocation into the shipped workbench
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0226]
---

# Compose explicit agent invocation into the shipped workbench

The policy, context, process, workspace, and Qt panel components exist. The
application constructs none of them, so a reviewer cannot invoke an agent.

## Required final state

- The shipped workbench presents the explicit invocation surface and the
  selected profile, scope, revision binding, workspace mode, and dirty-worktree
  risk.
- A user gesture enters core legality and host policy before any process is
  resolved or launched.
- The host constructs the exact stdin/structured context, environment
  allowlist, and revision-pinned workspace; no BetterPullReview credential can
  cross the boundary.
- Streaming/result lifecycle, cancellation, staleness, and shutdown are
  presented through typed events rather than direct widget/process coupling.
- A public canary trace proves the composed surface while retaining the RFC's
  optional status for a smoke run against an actually installed agent.
