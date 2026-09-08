---
schema: "implement-rfc.card.v1"
id: "BPR-0681"
title: "Compose General review and notification preferences"
status: "In Progress"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
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

Not run. Created by the authorized BPR-0592 planning pass.

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
