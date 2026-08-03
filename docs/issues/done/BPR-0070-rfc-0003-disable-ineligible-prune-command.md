---
id: BPR-0070
title: Disable prune for ineligible selected epoch availability
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0067]
---

# Disable prune for ineligible selected epoch availability

The BPR-0067 public maintenance path remains fail-closed because the existing
core transition and host catalog validation reject stale, `pruned`,
`cache_deleted`, and `never_observed` targets. Its command projection currently
checks authority, catalog, pending lanes, request capacity, accepted-pair
presence, and epoch-anchor kind, but not whether the selected epoch anchor's
availability can enter a prune operation. The public menu can therefore offer
and confirm an operation that is guaranteed to be rejected safely.

Make the core command projection disable prune unless at least one selected
epoch target has an availability admitted by the exact prune transition. Keep
the host/transition validation as the final authority. Cover mixed epoch/review
anchors, same-epoch deduplication, available/unavailable/corrupt targets, prior
prune, cache deletion, never-observed state, stale confirmation, disabled
reason/accessibility presentation, and zero dispatch while disabled.

This is a nonblocking UX/command-projection refinement discovered by the final
BPR-0067 critical review. It is not an authorization bypass, destructive-scope
bug, or reason to delay RFC-BPR-0004 implementation.

## Implementation

`core::prunable` is added beside `anchor_availability` in the epoch model as
the single definition of prune eligibility: `available`, `unavailable`, and
`corrupt` admit a prune operation, while `pruned`, `cache_deleted`, and
`never_observed` do not. The application's existing
`valid_prune_anchor_availability` now defers to it instead of restating the
same three-way rule, so the command projection and the durable authority
cannot drift apart.

The command projection in `epoch_commands` previously required only that one
anchor of the accepted pair be an epoch anchor. It now requires an anchor that
is both an epoch anchor and prunable. The disabled reason stays
`invalid_maintenance_target`, which is the same code the durable path already
returns for the same rejection, so the pre-check and the final authority speak
with one voice and the existing Qt reason text needed no change.

Nothing about authority moved. `valid_maintenance_target` in the core
transition still does not inspect availability, and the host catalog remains
the fail-closed decision point. A directly injected intent for an ineligible
epoch still reaches them and is still refused there.

## Direct verification

- `core_epoch_policy` covers available, unavailable, and corrupt anchors
  admitting prune; `pruned`, `cache_deleted`, and `never_observed` pairs
  disabling it with `invalid_maintenance_target` while whole-cache deletion
  stays enabled; same-epoch deduplication, where one prunable side of the same
  epoch is sufficient; a review anchor being unable to supply eligibility for
  an ineligible epoch anchor; a corrupt epoch anchor beside a review anchor
  still admitting prune; and an injected intent still reaching the durable
  authority so the change is provably not an authorization boundary.
- The new cases were confirmed to fail without the fix. With the projection
  change stashed, `core_epoch_policy` fails on the ineligible-availability
  case; restoring it passes. The test is not vacuous.
- `qt_epoch_comparison_scale_100`, `_125`, and `_200` pass serially. They now
  assert that a disabled prune command states its exact core reason in both the
  tooltip and the status tip, and that triggering it opens no confirmation and
  dispatches no maintenance intent.
- The focused serial matrix passes 7/7 in 109.25 seconds:
  `architecture_boundaries`, `architecture_boundary_contract`,
  `core_epoch_policy`, `application_epoch_contract`, `application_epoch_host`,
  `epoch_maintenance_integration`, and
  `application_epoch_maintenance_public_path`. The last two exercise the real
  Git/SQLite composition and the selected executable, so the change is verified
  through the public path and not only the core table.
- The 181-file format gate and the strict first-party `clang-tidy` audit pass,
  and `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`.

## Documentation disposition

The system architecture records `core::prunable` as the single eligibility
definition and states plainly that the projection is a pre-check rather than an
authorization boundary. The workbench specification records that prune stays
disabled for a selection that could only fail, and that a disabled command
carries its exact reason in the assistive-technology surfaces. No new reason
code, destructive scope, or authority was introduced.

## Result

The public menu no longer offers and confirms a prune that is guaranteed to be
rejected. Eligibility now has one definition shared by the projection and the
durable authority, and the fail-closed guarantee stays exactly where it was.
BPR-0070 is complete.
