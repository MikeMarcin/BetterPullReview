---
schema: "implement-rfc.card.v1"
id: "BPR-0622"
title: "Recover interrupted agent workspaces with durable exact ownership"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0621"]
---

# Recover interrupted agent workspaces with durable exact ownership

BPR-0621's composed SIGKILL course retained the interrupted workspace through
two ordinary application reopens. Native identities and content stayed equal;
Git/SQLite recovery and zero agent replay passed. The process guard stops the
agent but cannot reclaim the application's disposable checkout. Current
`github_application.cpp` creates a random sibling QTemporaryDir, disables its
generic removal, and retains the exact parent/workspace capabilities only in
memory. A new application creates a different root and cannot recover those
capabilities from the session catalog, which intentionally stores no paths.

## Required outcome

- Give disposable workspace lifetime a durable, bounded ownership/recovery
  path in the application host and installed-Git adapter. Keep original
  parent/child native identity, exact scope and ownership authority distinct
  from session metadata. Reuse the existing exact quarantine/removal machinery.
- Reclaim provably owned abandoned workspaces after application death, before
  reporting complete resource recovery. Preserve workspaces owned by another
  live application; use actual lifetime/locking evidence, not PID existence
  alone. Do not run a directory-name sweep or recursively delete a same-path
  substitute, symlink target, user checkout or retained Git cache.
- Crash points must include durable ownership preparation, materialization,
  running agent and interrupted cleanup. Run ordinary recovery twice to prove
  idempotence and retain exact Git refs/SQLite session identity. New invocation
  must use a fresh workspace and monotonic session ID with no automatic replay.
- Treat missing/untrusted old ownership evidence as a visible repair/refusal
  case. Bound admission and retained ownership records so crashes cannot create
  unbounded new workspaces. Do not infer removal authority for historical
  unrecorded roots. Document any unavoidable pre-ownership crash window and its
  bounded admission/repair behavior.
- Replace BPR-0621's explicit orphan-preservation observation with the stronger
  reclamation assertion where durable ownership exists. Add substitution and
  simultaneous-live-owner negatives through the real selected effect path.
  Keep safe preservation evidence for roots without durable authority.

## Scope before claim

Inspect `installed_git_process.hpp/.cpp` workspace identity and quarantine
operations, `agent_workspace` ports/host lifecycle, and application construction
and shutdown. Establish exact owned files and persistence format before source
changes. Use Glaze if JSON is chosen; do not add workspace paths to the immutable
agent-session catalog. Keep decisions in core/host and filesystem effects in
adapters. No credentials, live agent, paid service, GitHub Actions or push.

BPR-0617 remains the closeout anchor at RFC base
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`. This card stays open until the complete
recovery/resource outcome is demonstrated; BPR-0621 alone cannot close it.

## Implementation plan and ownership

First establish lifetime exclusion at the process boundary. A bounded optional
host value names an exact regular guard-file identity; the Unix launcher opens
it read-only without following links, checks native identity/private ownership
and retains a shared file lock. The bundled supervisor receives a duplicate of
that same locked open file description and keeps it through group teardown;
the external agent receives no guard descriptor. Recovery will need an
exclusive lock, so application death cannot release exclusion before the
supervisor stops the child. Unsupported platforms must refuse a requested
guard until their lifetime transfer is implemented, never silently discard it.
No production caller requests this guard until the durable workspace owner is
wired; that later integration is required for this card to finish.

Own `agent_launch.hpp`, the agent launcher/supervisor and their focused canary,
parent-death runner and architecture allowance. Then own the workspace host/port,
installed-Git workspace operations, application composition and focused Git/
composed recovery tests for durable records and recovery. Use existing native
identity/quarantine operations. Bound registry records and use a versioned
Glaze record if JSON is selected; reserve ownership before directory creation,
persist identity before materialization, and record a chosen quarantine path
before rename. Keep all scans and recursive cleanup on the effect lane. Root
construction, materialization and cleanup need crash checkpoints. This plan
is not acceptance of an unwired lifetime guard or incomplete journal.

## Lifetime prerequisite — implemented, card still In Progress

Source `e50edf77cc0af477bdfdc7d0ef269108e7f61722` adds the optional host-owned guard reference and Unix
launcher/supervisor retention described above. The adapter checks canonical
path, exact regular-file identity, private ownership and single link, and
retains a shared lock through a read-only descriptor. The supervisor inherits
only the designated duplicate and closes it in the external child before exec.
A requested guard on other platforms refuses; no production workspace owner
supplies this reference yet. Durable registration, restart reclamation and
non-Unix guard transfer remain unimplemented, so this card is not complete.

The real parent-death test freezes the supervisor with SIGSTOP, then kills the
application by PID or isolated process group. An independent exclusive lock
must still fail while the paused supervisor owns the running agent. Resuming
it stops the agent/descendant; exclusive access and a fresh launch then succeed.
This test initially failed: application death orphaned the stopped supervisor's
process group, causing a hangup/continue and supervisor exit. The launcher now
starts the supervisor in its own session, avoiding that orphan transition.
The fixture's failure cleanup was hardened to tolerate an already-dead
supervisor; the two exact leaked fixture processes from the first attempt were
identified, killed and independently confirmed absent.

### Local verification — 2026-09-05

- Dev canary target builds. `agent_parent_death` passes in 25.15s (25.16s total),
  including live and dead-parent lock exclusion, both PID/group kill courses,
  post-teardown release and fresh launches. Its preparatory public-launcher
  course also passes busy-lock, same-path substitution and symlink refusals
  before the external child side effect; successful guarded execution exposes
  no guard descriptor to the agent.
- Final `qt_agent_launch_canary` and `architecture_boundaries` pass 2/2 in
  79.46s (71.46s and 7.99s). The isolation manifest passed in 3.38s before the
  session-only repair; its registration inputs are unchanged.
- Tests-off `app-release` application builds after the session repair. Built
  and bundled supervisor SHA-256 both equal
  `58a20eee115f95505844123074c8178a9f02ff7a334f2e1202d5ee99ec650fb3`.
  `otool -L` lists only libc++ and libSystem. The application was not launched.
- Documentation gate, changed C++ clang-format checks and `git diff --check`
  pass. No GitHub Actions, push, live credentials, live agent or paid service.
  Task build logs are removed after recording results; test roots are consumed
  by their contained runners. No release application sign-out was performed.

## Next implementation step

Implement the durable registry and integrate its lifecycle on the effect lane.
Carry its exact guard reference from the owned workspace into the process
request; the native primitive above is ready for that handoff. Retain the
owner's shared lock during preparation, materialization and normal cleanup;
restart needs exclusive access plus durable original ownership and scope.

Use the installed-Git native identity/quarantine operations. The current
`quarantine_and_remove_exact` chooses a random destination and changes only an
in-memory path; a durable cleanup record must publish its chosen quarantine
path before rename and recognize both original and quarantine identities on
restart. Do not substitute a post-crash pathname capture for original ownership.
Reserve bounded journal capacity before creating a parent/root, persist exact
identity before materialization, and leave unknown pre-ownership artifacts
preserved with bounded refusal/repair. Recovery scans and recursive removal
must not run on the Qt/decision lane. Preserve the immutable SQLite session
catalog format; use a separate versioned Glaze record if choosing JSON.

Add preparation/materialization/cleanup kill cases, live-owner and substitution
negatives, then upgrade the composed BPR-0621 orphan observation to prove actual
reclamation of a durably owned workspace. Run repeated ordinary recovery and
session #2 with unchanged Git/SQLite identities. Keep safe preservation for
historical roots without ownership receipts. Only then consider Done.
