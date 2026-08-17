---
id: BPR-0066
title: Bound the Qt epoch-history working set
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0046, BPR-0047, BPR-0056]
---

# Bound the Qt epoch-history working set

The RFC-BPR-0003 Qt selector currently appends every accepted 100-epoch page.
A valid catalog may retain 100,000 epochs, allowing the presentation model to
grow to 200,000 anchor rows and making stable-ID operations repeatedly scan the
resident history. The full history must remain host/catalog-owned and paged;
Qt may retain only a fixed working set independent of the durable catalog
limit.

Select one bounded or virtual page-window design before implementation. It
must preserve exact revision-bound cursor acceptance, stable epoch and anchor
identity, independent left/right selection, complete selected OIDs even when a
selected anchor is outside the visible window, named comparison behavior,
explicit availability, incremental Qt signals, persistent-index legality,
catalog-revision/authority replacement, and access to older retained history.
Do not move catalog queries or semantic paging policy into Qt, copy the catalog
into core state, reset the model for ordinary page movement, or silently cap
the user's reachable history.

Direct tests must drive more pages than the resident limit and prove the row
cap, deterministic eviction/window movement, continued older-history access,
selection pinning or exact out-of-window presentation, forward/restart access
to recent history, stable signals/indexes, stale-page rejection, and bounded
lookup behavior. Re-run the real public composition/render and architecture
boundaries. Track any required core/host paging extension explicitly rather
than weakening these claims.

## Result

The selected design is one-page replacement. The core now authorizes explicit
`newest` and `older` navigation: older requests require the exact retained,
revision-bound continuation, while a newest request carries no cursor and is
legal after older traversal or catalog invalidation. Core retains only the
catalog identity, navigation kind, and continuation; page rows remain
transient.

The host projects the navigation kind and an explicit invalidated terminal.
A new catalog revision does not insert a new epoch into an accepted stale
page. A catalog with no accepted page may still project its first newest row,
which keeps the fresh and restart public paths usable. The selected executable
uses `Return to newest epochs` to bootstrap a persisted catalog with no
resident rows.

The Qt model retains one page capped at 100 epochs/200 anchors. It reconciles
accepted pages through stable-ID remove/move/insert/data-change notifications,
trims incremental insertions to the same cap, and never resets for routine page
movement. Separate `Load earlier epochs` and `Return to newest epochs` actions
emit semantic intents. A catalog-invalidated page remains visible but
read-only until newest reload succeeds. Exact selected OIDs and availability
remain in the comparison projection when anchors leave the page; selectors
show a nonresident accessible description rather than fabricated rows.

## Direct verification

- The core and Qt tests traverse 1,002 semantic pages and 1,003 rendered-model
  page projections. They reach ordinal one, reject older navigation without a
  continuation, restart newest without a cursor, stay at or below 100
  epochs/200 anchors after every terminal, bound per-page row signals, preserve
  overlapping persistent indexes, legally invalidate departed indexes, and
  report zero routine model resets.
- The application-host test proves newest/older navigation projection, exact
  cursor use, catalog-revision invalidation, cleared core bindings, and no
  invented insertion into the stale page. Existing reordered-source tests
  continue to reject stale terminals.
- The real SQLite behavior contract now walks the seeded 100,000-epoch catalog
  in 99-row pages: 1,011 revision-bound queries reach ordinal one with no gap,
  duplicate, reorder, or whole-history query. The strengthened contract passed
  1/1 in 36.92 seconds.
- The three 1.0x/1.25x/2.0x Qt tests render the actual loaded GitHub window
  with an exact 100-epoch/200-anchor page. The 1.0x raster was inspected at
  `build/dev/tests/adapters_qt/epoch-comparison-100.png`; it shows the exact
  selected OIDs and disabled earlier/newest actions while the newest page is
  visible. This build-tree evidence is intentionally not committed.
- The direct claim batch passed 15/15 in 166.87 seconds:
  `application_epoch_catalog_contract`, `application_epoch_contract`,
  `application_epoch_host`, `sqlite_epoch_catalog_behavior_contract`, all
  three `qt_epoch_comparison_scale_*` tests,
  `epoch_history_fixture_contract`, `epoch_maintenance_integration`,
  `application_native_appearance_probe_contract`, both selected GitHub/GHES
  public paths, both GitHub/GHES persistence restarts, and
  `application_render`.
- Core/dependency, architecture, Git-protocol, documentation-structure, and
  issue-graph boundaries passed 7/7 in 35.02 seconds. The canonical
  documentation validator reported `DOCUMENTATION_OK`, and `git diff --check`
  passed.

## Documentation ledger

- `docs/architecture/system-architecture.md`: updated with core-authorized
  direction, one-page Qt residency, stale revision behavior, and selected-pair
  independence.
- `docs/ux/workbench-specification.md` and
  `docs/ux/appearance-and-accessibility.md`: updated with the two paging
  actions, cap, nonresident selection, accessibility, and read-only stale
  state.
- RFC-BPR-0003 and both verification matrices: updated with bounded/reachable
  history requirements and direct proof.
- SQLite schema, Git/cache policy, provider/authentication behavior, package
  selection, and remote mutation semantics are `no-impact`; their owning
  contracts still passed in the selected matrix.

## Simplification disposition

The required serial code-simplifier pass removed an unreachable host-state
branch and made the explicit invalidation delta the sole stale-window decision
boundary. It deliberately retained typed core navigation, explicit
invalidation, stable-ID reconciliation, and accessible nonresident state
because each encodes required behavior. Focused tests were rerun after the
simplification and passed 5/5.

## Critical review disposition

The upgraded workflow classifies catalog identity, stale-result rejection,
and Qt model-index behavior as capability-critical. The callable agent
registry still reported orphaned nonterminal handles after the UI cleanup, so
the required fresh review used the documented serial fallback rather than
exceeding the delegation limit. The skeptical pass checked core cursor
authorization, host initial-versus-invalidated insertion, Qt page bounds and
signal legality, stale failure/retry, selected-pair independence, and the
selected executable bootstrap. It found two evidence gaps: intermediate Qt
pages were not required to expose their exact requested ordinal, and the real
100,000-row SQLite catalog was not traversed to ordinal one. Both tests were
strengthened and passed. The final ownership scan found no catalog/query port
in Qt, no page-row container in `epoch_state`, and no unbounded Qt insertion
path. No blocking finding remains.

## Deferred scope

This remediation makes no final visual-polish or selectable-theme claim.
RFC-BPR-0005 owns integrated presentation polish and native visual acceptance;
RFC-BPR-0006 owns System/Light/Dark/high-contrast selection and default-System
behavior. Direct target-native Windows execution remains in the existing open
Windows cards and did not block this available-platform slice.
