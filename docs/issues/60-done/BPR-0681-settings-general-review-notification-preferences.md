---
schema: "implement-rfc.card.v1"
id: "BPR-0681"
title: "Compose General review and notification preferences"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
acceptance: [{"criterion":"AC-02/04/05/06/09/10 accepted queue and notification preferences","outcome":"pass","revision":"cd1c1623 plus finish","evidence":{"kind":"observation","detail":"Settings exposes six queue defaults, scoped queue and notification resets, metadata-only preview, enabled/pause controls and honest platform capability text. Existing Reviews behavior defaults and observer management remain unchanged. General explains current startup/restoration capability pending RFC-0024. Host field-specific intents merge rapid independent edits over pending writes; Qt projects accepted values only. Core policy suppresses paused/disabled/quiet/unknown buckets; format 6 migrates previous queue schemas while preserving filters and local memberships."}},{"criterion":"Focused public, migration, authority, scale and portability evidence","outcome":"pass","revision":"cd1c1623 plus finish","evidence":{"kind":"command","detail":"application_review_queue_enrollment passes including rapid field edits, write refusal and restart; sqlite_queue_enrollment_storage passes including v1-v5 migrations, exact v5 filter preservation, notification restart and canonical refusal. qt_workbench_window_scale_100/125/200 pass (7.39/5.09/5.56s), including acceptance-before-display and scoped resets. application_queue_composition_contract passes 4.94s: seed/reopen, rapid selected controls, preview reaching existing ambiguous-activation refusal, pause preventing another attempt, request/activation cuts, no provider mutations. core-only configure/build and all 125 tests pass 160.35s, including both architecture checks. Canonical tidy on ten TUs and fresh Qt workbench pass. Documentation and diff/format pass. Existing review-behavior acceptance BPR-0661 is reused for unchanged implementation; no native permission or human verdict invented."}}]
---

# Compose General review and notification preferences

## Scope

Expose existing accepted review/queue preferences and bounded notification settings; present unavailable startup restoration honestly until 0024.

Owned paths: workbench/queue/notification core and hosts; relevant stores; Qt Settings pages; composition and focused tests.

## Acceptance

AC-02/04/05/06/09/10: typed acceptance, exact reset, persistence failure isolation, local-only rules and metadata-only notifications; no automatic transitions enabled by default.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Implemented core notification enablement/pause, queue format-6 migration,
accepted Settings controls and scoped resets. Field-specific intents merge rapid
independent edits in the host's pending record; widgets retain no unsaved bag.
General states current startup capability. Focused host, store and Qt checks
pass, including rapid edits, save failure and native 100/125/200 scaling.
The selected queue composition course passes preview's safe platform refusal,
paused suppression, rapid filter gestures, durable restart and request/activation
wire cuts. The complete core-only build and 125 tests pass (160.35s), including
both architecture boundaries. Canonical static analysis, format/diff checks and the documentation gate pass.

## Selected implementation

Reuse the queue host's accepted preference publication and transaction boundary.
Add a bounded core notification policy (enabled and paused), persist it with
queue preferences in format 6, and consult it before metadata-only delivery.
Both Home filters and Settings controls must project the accepted record;
remove the current optimistic Qt queue mutation. Settings exposes queue defaults
and scoped resets without resetting notification choices. Notification controls
show transport availability separately from unobservable OS permission, and
preview uses the existing exact-target metadata-only request. General explains
current landing/restoration capability until RFC-0024 supplies its authority.
Verification: core policy, queue host/store refusal and migration, composed
Settings dispatch, and the existing review/queue/notification public paths.
Attended conflict scan for these paths returned no conflicts.
