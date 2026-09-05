---
schema: "implement-rfc.card.v1"
id: "BPR-0620"
title: "Stop agent process groups after application death"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0619"]
---

# Stop agent process groups after application death

RFC-0013's recovery matrix requires forced death during an AI session. The
current Unix launcher creates an isolated process group and kills it during
cooperative cancellation/timeout, but those paths require the application to
remain alive. Windows already uses a kill-on-close Job Object. Existing
`qt_agent_launch_canary` exercises timeout and descendants, not parent death.

## Acceptance

- Use the concrete production agent launcher and the local, stubborn agent
  canary. Force-kill its owning application process after the agent and its
  descendant are observably running. A child that voluntarily exits on stdin
  EOF is insufficient proof.
- The agent and its descendant must stop within a bounded local observation
  window without relying on application destructors, timers or the test's
  cleanup. Detect failure before cleaning up exact fixture-created processes.
- A fresh launcher process must complete an ordinary canary invocation after
  the killed run. The normal protocol, environment filtering, cancellation,
  timeout and descendant-cleanup contract must remain passing.
- If a Unix lifetime guard is needed, prepare it before agent execution, keep
  its process-group authority exact, close unrelated inherited descriptors,
  and make setup failure refuse launch. It must not expand agent argv,
  environment or credential authority or require an independently installed
  executable. Windows Job Object behavior remains unchanged.
- Verify focused local process/protocol checks and a tests-off adapter build.
  Run network-isolated fake-agent fixtures on the Mac. No GitHub Actions,
  pushes, credentials, paid agent invocation or remote mutation.

## Scope and ownership

Own the agent process adapter and narrow Unix lifetime support, the existing
canary/test executable and its CMake registration, a bounded parent-death
runner, network-isolation registration if needed, and the owning architecture
paragraph. Reuse existing process-kill and contained-temp support when their
contracts fit; do not falsely treat heartbeat stability alone as termination.

This card covers child lifetime across forced application death. Complete
Git/SQLite/workspace restart during a real agent-host session remains a
separate RFC-0013 requirement to scope next, along with target-native evidence
outside this Mac. Do not close the parent RFC on this card's evidence.
