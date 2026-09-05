---
schema: "implement-rfc.card.v1"
id: "BPR-0622"
title: "Recover interrupted agent workspaces with durable exact ownership"
status: "Backlog"
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
