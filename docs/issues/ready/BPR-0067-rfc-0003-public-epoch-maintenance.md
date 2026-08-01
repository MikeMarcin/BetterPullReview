---
id: BPR-0067
title: Expose confirmed public epoch maintenance commands
status: Ready
rfc: RFC-BPR-0003
depends_on: [BPR-0046, BPR-0047, BPR-0055]
---

# Expose confirmed public epoch maintenance commands

RFC-BPR-0003 requires repository cache deletion and retained-epoch pruning to
be explicit user or administrator operations. The deterministic core intents,
application-host orchestration, installed-Git mutations, SQLite persistence,
recovery, and incremental projections already exist, but the selected public
application exposes no command that can authorize either operation.

Add a secondary local-history maintenance surface to the reusable Qt epoch
control. Core command projection must decide general availability; Qt may
derive exact targets only from the accepted comparison projection, never from
visible page rows, OIDs, paths, or adapter state. Capture the complete catalog,
epoch, and cache target before opening a Qt-owned destructive confirmation,
default the dialog to Cancel, and dispatch the existing typed intent only
after confirmation. A later authority/catalog change must fail closed through
the existing core identity checks.

Pruning must accept only selected base/head epoch anchors, sort and deduplicate
their stable epoch IDs, preserve visible `pruned` tombstones, and explain that
private retention refs may become unrecoverable after remote history changes.
Whole-cache deletion must clearly state that it affects every retained pull
request in the current repository cache, removes private retained repository
bytes, and leaves `cache_deleted` tombstones. Both dialogs must state that the
operation does not modify GitHub or a developer worktree. Provide stable
object names, accessible names/descriptions, keyboard access, and an explicit
maintenance-cancel command where core policy permits it.

## Required direct evidence

- Core tests cover command projection and exact catalog/authority/target
  rejection, including duplicate selections and stale confirmation targets.
- Qt tests exercise the real actions and confirmation dialogs, prove Cancel
  dispatches nothing, confirm exact typed prune/delete intents, and cover one
  selected epoch, two selected epochs, non-epoch review anchors,
  accessibility, and action enablement.
- A selected-executable fixture drives QAction -> confirmation -> core ->
  application host -> real installed-Git/SQLite -> incremental projection.
  It proves exact retained-ref absence and durable `pruned` state after
  restart, then exact repository-cache deletion and durable `cache_deleted`
  state after restart.
- The real-composition proof must detect widget-only dispatch, host-direct
  fixture shortcuts, broad deletion, model reset, projection failure,
  credential retention, remote mutation, or extra GitHub traffic.
- Run the focused core, Qt, maintenance-integration, public-path persistence,
  architecture, documentation, workflow-absence, and diff-hygiene gates.

No GitHub Actions workflow, GitHub/GHES mutation, final RFC-BPR-0005 visual
polish, RFC-BPR-0006 selectable theme, or Windows-runtime claim belongs to
this slice.
