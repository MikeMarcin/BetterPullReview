---
id: BPR-0067
title: Expose confirmed public epoch maintenance commands
status: Done
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

## Result

The core now projects six epoch commands and owns their general legality,
including exact authority/catalog availability, request exhaustion, ingestion
and maintenance conflicts, accepted-pair presence and epoch-anchor kind for
pruning, and both cancellation lanes. Exact maintenance transitions continue
to revalidate the captured catalog, cache, epoch IDs, revision, and starting
availability. BPR-0070 retains the nonblocking refinement that will disable
prune before confirmation when selected availability is already ineligible.

The reusable Qt control exposes one secondary `Local history maintenance`
menu rather than three new primary buttons. Prune derives only epoch anchors
from the accepted comparison pair, sorts and deduplicates their stable IDs,
and captures the catalog before confirmation. Whole-cache deletion captures
the accepted catalog and repository-cache scope. Both real `QMessageBox`
warnings make Cancel the default and Escape action, name the destructive
consequence and retained tombstones, and state that GitHub and developer
worktrees are untouched. Stable object names, accessible descriptions, and
keyboard actions cover the menu, actions, dialogs, and buttons.

The selected test-enabled executable adds a four-process maintenance probe:
seed, public prune, restart/public delete, and final durable verification. It
uses the existing recorded GitHub fixture, real Qt actions and confirmations,
core reducer, application host, installed Git, SQLite catalog, incremental
projection, and shutdown path. The probe exposed and corrected one production
catalog inconsistency: whole-cache finalization now supersedes earlier
`pruned` rows with `cache_deleted`, so durable state and the core/Qt bulk
projection agree. Its final proof reopens the durable retired catalog from the
captured smart authority rather than sampling a transient replacement
projection during shutdown.

## Direct verification

- The focused developer batch passed the core policy, SQLite behavior, all
  three 1.0x/1.25x/2.0x Qt cases, real Git/SQLite maintenance integration,
  and public maintenance path. After correcting the final durable-evidence
  selection, the four-process public contract passed three consecutive runs
  (12 selected-application processes).
- A refreshed ASan/UBSan build passed 4/4:
  `core_epoch_policy`, `qt_epoch_comparison_scale_100`,
  `epoch_maintenance_integration`, and
  `application_epoch_maintenance_public_path`.
- Core/dependency, architecture, documentation structure/issue graph,
  tests-disabled application-composition seam exclusion, and binary dependency
  surfaces passed in the combined closeout batch. The documentation validator
  reported `DOCUMENTATION_OK`; tracked and on-disk GitHub Actions workflow
  counts were 0/0; `git diff --check` passed.
- The current 1.0x and 2.0x loaded-window rasters were inspected. The single
  secondary maintenance menu remains visible without clipping or new primary
  action clutter. These forced offscreen rasters are layout evidence only;
  BPR-0059 remains the native System-theme evidence.

## Documentation ledger

- README, system/Git architecture, security model, workbench UX, and
  appearance/accessibility guidance now describe the exact public command,
  confirmation, target, authority, consequence, and theme boundaries.
- The build guide and fixture catalog record the focused command and
  four-process selected-executable evidence.
- The RFC-BPR-0003 verification ledger now marks available-target public
  maintenance evidence precisely while retaining Windows and unrelated
  closeout work as open.

## Simplification disposition

The required simplification pass consolidated the destructive commands into
one secondary menu, removed a speculative recovery-directory scan, and made
the final restart proof derive durable identity from the already captured
smart authority. The explicit four-phase harness and typed failure evidence
remain because they distinguish public dispatch, physical effects, durable
catalog state, restart, and extra-provider-call failures.

## Critical review disposition

The independent capability-critical review found no production authorization
or destructive-scope defect. It found one High evidence gap: public pruning
proved only that the selected epoch's refs disappeared and could have missed a
broad ref deletion. The fixture now carries ordinal-two base/head canaries and
requires them before prune, after live prune, and after restart, then requires
their absence only after intentional whole-cache deletion. The strengthened
regular and sanitized public contracts pass.

The review also corrected one exact marker name in the fixture catalog and
identified one Medium fail-closed UX issue: an already ineligible selected
epoch may still offer prune before the exact transition/host rejects it.
BPR-0070 records that command-projection refinement. It is not an authority
bypass and is intentionally deferred by product-owner direction.

## Deferred scope

BPR-0068 retains epoch-host job/FIFO/ingress capacity hardening. BPR-0069
retains final provenance, claim-wording, and immutable-base path
reconciliation. BPR-0053 retains real Windows execution. These tasks do not
block RFC-BPR-0004 implementation by product-owner direction, and this slice
makes no RFC-BPR-0005 presentation-polish or RFC-BPR-0006 selectable-theme
claim.
