---
schema: "implement-rfc.card.v1"
id: "BPR-0969"
title: "Verify cached-review offline and restart freshness"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0969-offline-inventory","path":"docs/rfcs/plans/bpr0969-offline-inventory/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b3197149a74bbb8030a3d4a534e956b11a64ed9f","evidence":{"kind":"observation","detail":"Prior real-composition inventory, unavailable versus empty, retained epoch/source/metadata distinctions; docs/verification/BPR-0969-offline-inventory-2026-09-25.md."}},{"criterion":"AC2","outcome":"pass","revision":"5ec14834d9fb1d26b7aede0824fe1eb84cf7f6f7","evidence":{"kind":"observation","detail":"Actual Mac safe-fixture restarts/network loss/reconnect with exact account/repository/base/head isolation, missing source and unknown freshness; current-process independent authentication required. docs/verification/BPR-0969-cached-review-2026-09-25.md."}},{"criterion":"AC3","outcome":"pass","revision":"5ec14834d9fb1d26b7aede0824fe1eb84cf7f6f7","evidence":{"kind":"observation","detail":"Integrated durable conversations and exact retained source across core/host/SQLite/Qt; rendered candidate and owning-layer cancellation/stale-session/negative controls. Unsupported disconnected startup, old retained pairs, binary/nonUTF8/over-limit data and platform/native limits explicitly retained. docs/verification/BPR-0969-cached-review-2026-09-25.md."}}]
---

# Verify cached-review offline and restart freshness

## Transferred obligation

Original RFC0013 acceptance row 5's composed cached-data offline/restart and visible freshness labels.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

BPR-0161 and current queue tests cover typed offline/recovery and stale-session refusal. Retained Git/catalog history is private exact revision evidence. Neither proves a complete cached-review offline/restart mode with visible freshness labeling. Current RFC status claims no new offline capability.

## Acceptance

- AC1: Inventory which cached review data the actual application can reopen offline, including metadata, source, threads and retained epochs; explicitly distinguish unavailable content from an observed empty result.
- AC2: Exercise network loss and restart through safe real composition, verifying cached/stale labeling and exact account/repository/revision isolation. Reconnection must adopt server truth without silently implying prior freshness.
- AC3: Implement any demonstrated missing required behavior within its owning core/host/Qt boundary, with exact-candidate rendered checks, cancellation and stale-session negative controls; retain all unsupported cases explicitly.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## Execution record

The bounded [Mac inventory](../../verification/BPR-0969-offline-inventory-2026-09-25.md)
is complete at source `b3197149a74bbb8030a3d4a534e956b11a64ed9f` under
`r4-standalone-offline-inventory-20260925`. Its three-process course demonstrates
retained live snapshot/source after refresh failure, unavailable content after
offline Resume/restart, and unchanged durable Git/epoch identities. The visible
failed-refresh freshness notice has a failing-before control and a successful
changed-server-thread recovery. It adds no offline account authority or durable
PR snapshot. The second and third acceptance criteria remain pending; this card
is unfinished.

The next bounded [Recent isolation slice](../../verification/BPR-0969-recent-identity-2026-09-25.md)
under `r4-standalone-recent-identity-20260925` repairs demonstrated same-profile
account exposure. Schema five keeps exact accepted account IDs; older rows
remain unattributed and cannot inherit a login. Both switch directions, restart,
offline Resume and delayed old-account completion are exercised through safe
composition. This is partial AC2 evidence and a bounded AC3 repair; the canonical
acceptance criteria are unchanged and unfinished. The
[Recent slice handoff](../../rfcs/plans/bpr0969-offline-inventory/recent-identity-handoff.json)
retains the exact candidate, verification and remaining obligations.

The [durable overview slice](../../verification/BPR-0969-durable-overview-2026-09-25.md)
under `r4-standalone-durable-snapshot-20260925` persists bounded overview metadata
from accepted reads. After current-process authentication, a transport-failed
open can display the exact account/destination's retained overview with unknown
freshness and exact base/head IDs. It creates no accepted snapshot, source,
conversation, epoch or mutation authority. Actual process restart, corrupt-store
refusal, account/destination isolation and reconnection use safe composition;
core/host controls cover cancellation and stale identity. The canonical AC2/AC3
remain unfinished and this card returns to Backlog. The
[overview handoff](../../rfcs/plans/bpr0969-offline-inventory/durable-snapshot-handoff.json)
binds candidate, verification, scope limits and the next action.

The [historical file catalogue continuation](../../verification/BPR-0969-file-catalogue-2026-09-25.md)
under `r4-standalone-review-content-20260925` extends that same atomic overview
with bounded exact current/previous paths, provider change kinds and reported
line counts. Its catalogue remains historical evidence of the accepted base/head;
it supplies no source navigation or current review authority. The coordinator
explicitly accepted this independently useful slice. Actual restart and legacy
payload courses plus owning-layer empty, bounds, changed-base/head, cancellation,
stale-session and corruption controls retain unavailable semantics. The canonical
AC2/AC3 remain unfinished. The
[current handoff](../../rfcs/plans/bpr0969-offline-inventory/review-content-handoff.json)
records the exact source, evidence, pending work and writer release.

The [historical thread continuation](../../verification/BPR-0969-thread-history-2026-09-25.md)
under `r4-standalone-conversations-20260925` retains complete bounded review-thread
collections, including roots/replies, stable identities and inert anchor/resolution
observations, in that same atomic overview record. Current-process authenticated
transport-failure admission is unchanged. Its read-only reader says freshness
unknown and distinguishes unavailable from observed empty. Actual restart,
legacy payloads, both account directions, changed-server recovery and owning-layer
cancellation/stale-session/bounds controls pass. General comments, review summaries,
source composition, disconnected admission and full AC2/AC3 remain unfinished.
The coordinator accepted this bounded slice. The
[thread handoff](../../rfcs/plans/bpr0969-offline-inventory/conversations-handoff.json)
binds candidate `b4a4f43ae19872432fb74a3e1fd244e9507e2166`, exact evidence and
remaining obligations; no native/human or full cached-review acceptance is claimed.

Latest owner/coordinator steering retains the same implementer and writer for
`durable-cached-overview` until full remaining AC2/AC3 reaches an actual exit or a
concrete policy/human/native blocker. This candidate is progress within that
milestone. Continue the integrated general-comment/review-summary, retained-source,
revision-recovery and freshness outcome under current-process authentication. Disconnected account admission
remains separate: never treat a saved credential, Recent row or retained Git/
catalogue identity as present account authority. Complete changed-revision recovery
without silently presenting historical content as current. Preserve unavailable
versus observed-empty data and reconnect to server truth. No disconnected
authentication, native/human acceptance or full cached-review mode is claimed.

## Integrated acceptance — 2026-09-25

The same implementer completed the remaining supported current-process authenticated
course under `r4-full-remaining-acceptance-20260925`, source `5ec14834d9fb1d26b7aede0824fe1eb84cf7f6f7`.
The [integrated evidence](../../verification/BPR-0969-cached-review-2026-09-25.md)
binds durable metadata, file catalogue, general comments, review summaries, threads
and exact retained source across network loss, actual restarts and reconnection.
Owning-layer and composed controls cover account/profile/repository/revision
isolation, legacy/corrupt/excessive data, missing Git packs and anchors, cancellation,
stale sessions, unknown freshness and replacement by server truth. Historical
source never creates a live snapshot, epoch, comparison, receipt or mutation lease.

AC1's prior inventory remains valid; AC2/AC3's supported course is complete.
Unsupported cases remain explicit as AC3 requires: fully disconnected startup
without independently accepted account identity; cached pairs differing from the
latest available retained epoch; missing, binary, non-UTF8 or over-limit source;
current conversation actions and participant/check/progress data. Current account
authority cannot come from stored credentials, Recent or cache content. There is
no new offline identity policy, native/human or Windows/Linux acceptance claim.
No explicit human criterion is attached to this card. The
[full milestone handoff](../../rfcs/plans/bpr0969-offline-inventory/full-acceptance-handoff.json)
records exact tests, remaining policy/platform limits, command quiescence and writer
release. This is the integrated milestone result, not another partial continuation.
