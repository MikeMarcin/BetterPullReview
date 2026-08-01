---
id: BPR-0051
title: Document RFC-BPR-0003 cache and epoch behavior
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0047, BPR-0049, BPR-0050, BPR-0055, BPR-0065]
---

# Document RFC-BPR-0003 cache and epoch behavior

Update agent and product documentation for architecture ownership, exact
typed flow, installed-Git requirements, native smart transport, cache
location/identity/privacy/permissions, private refs, persistence, retention,
quotas, restart recovery, explicit pruning/deletion, force-push and
never-observed limits, epoch causes/aliases/OIDs, failure states, safe fixture
verification, platform evidence, and operator troubleshooting.

Record the affected documentation ledger and direct no-impact evidence for
GitHub mutations, local review receipts, RFC-BPR-0004 diff/line mapping,
RFC-BPR-0005 final workbench, RFC-BPR-0006 selectable appearance/syntax,
queue/AI/notifications, package formats/signing/installers, positive CLI
enablement, and every hosted workflow surface.

## Outcome

- Classified the complete immutable-base change set from
  `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`: 186 paths at closeout, comprising
  121 additions and 65 modifications. The review covered product entrypoints,
  architecture/authority, security, developer/operator guidance, UX,
  fixtures/tests, dependency/build ownership, RFC/tracker state, and explicit
  downstream boundaries.
- Updated the product README and product contract so the selected public
  RFC-BPR-0003 path, private retained-revision authority, isolated
  installed-Git/cache/catalog composition, force-push retention, and
  never-observed limit are visible without conflating them with the future
  private workflow ledger.
- Added operator guidance for installed-Git admission, opaque app-owned
  storage, no manual mutation, hard quota refusal without automatic pruning or
  Git GC, restart and `repair_required` behavior, exact prune/delete
  consequences, and the current absence of a public maintenance command.
- Normalized completed-card evidence language across the Git/epoch, GitHub
  host, and security architecture documents. Windows remains BPR-0053 and the
  owner-activated macOS focus transition remains BPR-0059.
- Reconciled the RFC documentation ledger with direct anchors and preserved
  the existing UX, fixture-catalog, dependency, contribution, and agent-guide
  dispositions because this documentation slice adds no behavior, fixture,
  dependency, packaging, or workflow surface.

## No-impact disposition

- The canonical RFC-BPR-0004, RFC-BPR-0005, and RFC-BPR-0006 documents and
  their issue plans are unchanged. RFC-BPR-0003 claims no semantic diff/line
  mapping, final workbench/presentation polish, selectable theme/font/syntax,
  local review receipt/actionability, mutation, queue/AI/notification, positive
  CLI, package/signing/installer, or hosted-workflow implementation.
- The immutable-base path audit found no package or `.github/workflows` change
  and no source target for those deferred regions. The only RFC-index change
  is tracker/status reconciliation.
- GitHub remains the authority for current shared facts. Retained epochs are
  private records of exact observed revisions and remain selectable only while
  retained; they do not invent never-observed history or make raw cache
  topology authoritative.

## Direct verification

- `python3 scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK` after the final edits.
- `architecture_boundaries` and `architecture_boundary_contract` passed 2/2
  in 37.27 seconds.
- `core_epoch_policy`, `application_epoch_host`, and
  `epoch_history_fixture_contract` passed 3/3 in 0.63 seconds.
- `application_epoch_persistence_github-com` and
  `application_epoch_persistence_ghes-3_18` passed 2/2 in 102.67 seconds.
- `epoch_maintenance_integration` passed 1/1 in 29.26 seconds through the real
  core/host/installed-Git/SQLite composition.
- `git_protocol_boundary`, `application_git_protocol_v2`, both native smart
  transport cases, and `git_process_contract` passed 5/5 in 77.75 seconds.
- The post-edit independent documentation audit found no blocking or high-risk
  issue. Its retention and completed-card-tense corrections were applied and
  rechecked.
- Immutable-base and working-tree `git diff --check` passed. Tracked and
  on-disk `.github/workflows` counts were 0/0.
