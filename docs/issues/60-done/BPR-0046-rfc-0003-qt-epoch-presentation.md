---
schema: "implement-rfc.card.v1"
id: "BPR-0046"
title: "Implement exact epoch comparison controls in Qt"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0041","BPR-0045","BPR-0056"]
---

# Implement exact epoch comparison controls in Qt

Add a reusable provider/Git/filesystem-free comparison-controls presentation
with stable epoch identities, two independent selectors, named-comparison
actions, exact non-elided/copyable OIDs, cause and availability facts, and
loading, retained-refresh, empty, canceled, failed, pruned, cache-deleted, and
never-observed states.

Qt dispatches semantic epoch IDs or alias intents and applies core/host
projections; it never compares OIDs, classifies causes, resolves aliases,
chooses fallback epochs, reads a cache, or launches Git. Update rows
incrementally with exact insert/remove/move/data-change signals and preserve
stable selection, popup/focus state where possible, keyboard operation,
visible native focus, accessible relationships, and one bounded deduplicated
announcement.

Embed the controls in the interim loaded-content surface for public reachability
without claiming RFC-BPR-0005's final workbench or RFC-BPR-0006's selectable
themes/fonts/syntax.

## Implementation audit frontier

Begin from the provider/Git/filesystem-free epoch projection introduced by
BPR-0045 and the bounded history-page deltas introduced by BPR-0056. Audit the
live GitHub window's retained-content composition, model-update conventions,
focus/accessibility behavior, appearance invalidations, and deterministic
render harness before selecting widget boundaries. Reuse the existing public
window only as the interim reachability surface; do not make it an epoch
authority or pre-implement the final RFC-BPR-0005 workbench.

## Outcome

- Added one provider/Git/filesystem-free `epoch_history_model` that retains
  delivered Qt pages, exposes each stable epoch's exact base/head anchors, and
  applies inserts, removals, moves, row changes, exact prune availability, and
  bulk cache-deletion availability without a routine reset.
- Added one reusable `epoch_comparison_control` with independent left/right
  selectors, three named-comparison actions, bounded page/refresh/cancel
  actions, complete copyable SHA-1/SHA-256 OID fields, explicit history and
  comparison states, native focus, label/buddy and accessible surfaces, and
  one bounded deduplicated polite announcement.
- Selector and QAction gestures dispatch only catalog-bound semantic intents.
  Programmatic projections are signal-blocked and remain authoritative for
  checked/selected state. Review anchors remain exact selected endpoints
  without being fabricated as epoch rows.
- Embedded the control in the real interim loaded pull-request window. Added
  the missing exact resulting-availability fact to the core/application delta
  so Qt can render `pruned` without inferring maintenance policy.

## Direct verification

- `cmake --build --preset dev -j 6` passed with the new Qt sources, test target,
  and real loaded-window composition.
- The focused core/host, existing GitHub-window scales, and new epoch-control
  scales passed 8/8. The three new cases directly exercise selector/QAction
  translation, stable signals/indexes, all required states, reverse/identical/
  review-anchor pairs, exact 40/64-character OIDs, focus/accessibility,
  announcements, appearance invalidation, and actual-window rendering.
- The final complete Qt suite passed 9/9 in 14.95 seconds, including the 3,000-case,
  5,000-permutation, and large-rotation conversation-model property target.
- The complete Qt-free `core-only` matrix passed 27/27 in 38.76 seconds after
  the typed availability-delta change.
- The documentation/architecture focused CTest gate passed 5/5 in 32.74
  seconds; the direct architecture mutation suite passed 104/104.
- `python3 scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK`; tracked and on-disk GitHub Actions workflow counts were
  0/0; `git diff --check` passed.
- The 1.0x, 1.25x, and 2.0x rasters were inspected under the ignored build
  tree and show the control attached to the loaded window with complete OIDs
  and no clipping. They are deterministic offscreen interaction/geometry
  evidence only: BPR-0049 retains target-native system Light/Dark and
  available-Linux UX, BPR-0053 retains real Windows UI evidence, and BPR-0047
  retains production epoch-host wiring and persistence.

## Documentation and simplification

Updated the system and Git/epoch architecture, workbench and appearance UX
references, build/test guide, fixture catalog, RFC-BPR-0003 verification
ledger, and live issue board. The code-simplifier pass removed unused Qt
scaffolding while retaining explicit model-update and presentation-state
branches whose distinct signals, focus behavior, and typed wording are part of
the verified contract. No GitHub mutation, GitHub Actions, RFC-BPR-0004 diff,
RFC-BPR-0005 final-polish, or RFC-BPR-0006 selectable-theme behavior was added.
