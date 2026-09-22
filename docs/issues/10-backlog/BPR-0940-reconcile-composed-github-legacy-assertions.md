---
schema: "implement-rfc.card.v1"
id: "BPR-0940"
title: "Reconcile legacy composed GitHub assertions with current reading and sign-in contracts"
status: "Backlog"
rfcs: []
depends_on: []
priority: P2
metadata: {"origin":"r20-usable-review-amendments integration verification","scope":"Broader pre-existing composed Qt contract drift; not part of BPR-0935 through BPR-0939 acceptance","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency"}
---

# Reconcile legacy composed GitHub assertions

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending: Complete composed GitHub and workbench-window layout reconciliation at the existing scales; known failures remain open.
No owner decision, native observation or repair is supplied by this transfer.

The broad `qt_composed_github_scale_100` suite is not green during the r22
integration pass. Focused new loading, visual, CLI, core/host and actual
production-composed public courses pass; do not describe the broader suite as
passing on their strength.

The initial failures asserted retired contracts: adjacent different root
conversations despite the current selected-root reader; Home sign-in opening
Settings instead of the existing shared dialog; loading remaining on Home
despite BPR-0795; and an inline-comment action without an exact code anchor.
The r22 test-only upkeep corrects those precise fixtures/expectations without
changing product behavior. The next reached failure is:

`with GitHub CLI chosen the card speaks for it: the kind line says so, Connect is its retry, the warning stays and Sign in is not filled`

It occurs in `test_one_card_per_server_holds_its_ways_in`. The card's current
shared sign-in setup mode and the legacy multi-host Settings expectation need
independent reconciliation. Do not simply weaken action/authority assertions
or change shipped sign-in behavior to satisfy an obsolete test. BPR-0933 owns
the separate owner-requested CLI warning cleanup.

## Acceptance

- AC1: Identify each remaining assertion against current approved UI behavior and preserve account, exact-anchor, cancellation and no-effect boundaries.
- AC2: Update stale fixture entry paths or repair an actual regression with a failing-before control, without suppressing tests or reducing authority checks.
- AC3: Run the complete composed GitHub Qt suite at its existing scale registrations and record the exact revision and results.

## Evidence

At r4 source `3efe61dfcf6874d35400bf777a7e86dbbf0a4693`, the ordinary
`application_large_review_benchmark` passes its exact source-document,
navigation, 1,000-wheel and stale-selection assertions, but direct inspection
of its new `large-review-window.png` shows the current Overview page. The
hidden documents being exact does not prove readable on-screen source. Extend
AC1–AC3's current-entry-path reconciliation to this benchmark render and assert
the intended visible Read surface without weakening its existing data checks.
The separate unavailable-diff render correctly shows Read and the typed limit.
No whole-window or visible selected-source acceptance is inferred from the
ordinary benchmark's passing process result.

See the [r22 bounded evidence](../../verification/rfc-bpr-0013-usable-review-20260920.md).
The current failure is durable and remains open; no full-suite acceptance is claimed.

The r23 follow-up also reaches the related workbench-window layout assertion at
all three existing scales: `the epoch row is the first row of the client area
and as wide as it`. The measured row top is 72 versus client top 50. A temporary
control restoring the old, non-wrapping comparison status still fails identically
(2.10 seconds); that control was removed and the unchanged test rebuilt. The
new endpoint-failure presentation is not exercised by this layout setup. Preserve
the existing failure until the current compose/readiness/epoch-row layout
contract is reconciled; do not count this suite as green. Include these window
scale registrations in this card's contract reconciliation and verification.
