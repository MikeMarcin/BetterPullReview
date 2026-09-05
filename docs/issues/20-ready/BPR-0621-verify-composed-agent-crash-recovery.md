---
schema: "implement-rfc.card.v1"
id: "BPR-0621"
title: "Verify composed application recovery after an agent-session kill"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0620"]
---

# Verify composed application recovery after an agent-session kill

RFC-0013 requires forced death during an AI session. BPR-0620 proves the
concrete launcher stops its children, but does not open the real application,
Git cache, SQLite session catalog or disposable workspace on restart.

## Acceptance

- Extend the existing ordinary application composition fixture with a real
  local canary that reports its PID and descendant PID only while running in
  the materialized exact-head workspace. Kill the application with SIGKILL
  after session admission and observable descendant activity, without normal
  shutdown. Assert both processes stop before fixture cleanup.
- Independently inspect all fixture SQLite databases for integrity/foreign-key
  consistency and the exact committed agent-session/sequence identity. Inspect
  the retained bare Git cache with fsck and exact refs. Reopen the application
  twice without starting an agent: both opens retain session #1/next #2 and
  start zero agent processes. No remote publication or automatic replay.
- A new explicit invocation through the real UI/host/process composition uses
  session #2, succeeds in a fresh workspace and cleans that new workspace at
  shutdown. Independent persisted-state checks retain both exact session
  bindings and next sequence, without duplicate epochs or changed cache refs.
- Observe the killed workspace through each restart. Preserve its exact native
  identity/content if it remains; record any missing reclamation as a separate
  bounded recovery card, not as full resource recovery. Do not acquire deletion
  authority from a matching directory name. Test-owned artifacts are removed
  only after all assertions and exact fixture process cleanup.
- Run the new network-isolated composed kill lane, existing ordinary composed
  seed/reopen lane and isolation manifest on the Mac, plus the documentation
  gate and changed-source format/diff checks. No GitHub Actions, push, live
  credentials, paid agent, network service or remote mutation.

## Ownership and closeout

Own the existing agent composition fixture, local agent canary, end-to-end
CMake registration, bounded Python kill/restart runner and narrow shared kill
runner argument/timeout support. Production fixes require an observed failure
and explicit ownership refinement. Anchor: BPR-0617, RFC base
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`. RFC-0013 remains open for any workspace
reclamation gap and the remaining performance/resource criteria.
