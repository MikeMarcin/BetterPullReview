---
schema: "implement-rfc.card.v1"
id: "BPR-0223"
title: "Nothing asks for a notification and nothing consumes an activation"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0231"]
---

# Nothing asks for a notification and nothing consumes an activation

The notification adapter and notifier were composed, but the shipped queue did
not request a notification and no landing surface consumed an activation.
BPR-0231 subsequently supplied an honest observer-rule queue item and the real
incremental landing target this card needed.

## Required final state

- A warranting transition from an accepted queue item requests a metadata-only
  notification through the composed notifier; merely constructing the sink is
  not evidence.
- The first accepted provider state is a quiet baseline. Activity is derived
  only from later accepted provider or ledger facts, never synthesized to make
  the path fire.
- Activation selects and focuses the exact visible pull-request identity. A
  hidden or absent target refuses visibly without changing presentation
  preferences or opening provider navigation.
- The request and activation wires each have an independent cut-wire
  counterexample.
- An identityless platform must not route an old notification click to a newer
  review.

## Implemented state

- The composition root forwards every accepted queue projection unchanged to
  the shipped workbench before considering notifications. It retains the first
  accepted head for one exact queue session and pull-request identity as a quiet
  baseline.
- A later accepted head reports `ReviewedByMe` only from the accepted
  current-viewer review and reports `HeadMovedSinceMyReview` only when both the
  baseline and that review commit differ from the current head. Session,
  identity, and invalidation boundaries clear the retained activity authority.
- Newly warranting `(session, pull-request identity, bucket)` transitions pass
  through `review_queue_notifier` with the existing fixed metadata-only payload.
  Active-transition deduplication is process-local and is not persisted.
- `QSystemTrayIcon` exposes no per-message activation identity. Its adapter now
  fails closed: it submits at most the first actionable payload for the sink
  lifetime, retains that exact immutable target, suppresses all later payloads,
  consumes at most the first click, and never rearms.
- The activation callback selects and focuses the exact full identity in the
  visible queue proxy. Filtered and absent targets publish a local refusal and
  leave preferences and provider navigation unchanged.
- The public four-process contract enrolls the existing two-head GitHub.com
  fixture through the real rule action, proves head one is quiet with zero
  submissions, drives the real Refresh action to head two, observes the same
  review become `changed_since_my_review`, submits and activates the exact
  metadata-only target, proves reopen does not fabricate a delivery, and cuts
  the request and activation wires independently on that same transition.

## Verification

- `application_review_queue_notification`
- `application_notification_delivery`
- `qt_system_tray_notification_sink`
- `qt_workbench_window_scale_100`
- `application_queue_composition_contract`

The focused matrix passes 5/5 after the final simplification. Direct notifier
and Qt contracts prove A is submitted, B is suppressed, the first click routes
exact A, duplicate or delayed clicks do nothing, and the sink does not rearm.
The composition contract proves the honest accepted-head transition and both
non-vacuous cuts with zero provider mutation. Documentation, formatting, and
diff checks pass. Independent critical closeout review found no remaining
blocking or high-risk finding.

Actual operating-system display and permission remain the attended BPR-0168
gate; this card proves submission to the adapter and exact activation routing,
not that every desktop displayed a banner.
