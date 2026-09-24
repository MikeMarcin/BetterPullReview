---
schema: "implement-rfc.card.v1"
id: "BPR-1116"
title: "An in-process renewal that gives up, or a restored session whose refresh slot is missing, still erases the remembered grant"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0644 keep-grant round, Mac batch 7, 2026-09-24","origin":"owner decision 3c46d164 on BPR-0644-grant-policy-v001, attended session cbd48b91000b00f1e72e1ad62c7c384c, note 'Shouldn't remove until the user removes it in the settings menu.'","related_cards":["BPR-0644","BPR-0541","BPR-1048","BPR-1112"],"reservation":"docs/issues/_reservations/BPR-1116--BPR-1116--c8c449f21922.json"}
---

# BPR-1116: An in-process renewal that gives up, or a restored session whose refresh slot is missing, still erases the remembered grant

## Why

On 2026-09-24 the owner answered [[BPR-0644]]'s grant-policy question
(decision `3c46d164`): keep the material, fail visibly, and retry at the next
launch. His note states the rule behind it, verbatim:

> Shouldn't remove until the user removes it in the settings menu.

[[BPR-0644]] builds that rule for **launches**: a restore whose keychain is
locked or denied, whose network is down, whose access slot is missing, or
whose `/user` refuses the token keeps the grant, and so does a launch renewal
that is offline, refused, or past its refresh lifetime. The launch renewal no
longer writes a tombstone before its first attempt.

Three automatic removals are outside that card's launch scope and still
happen **while the application is running**:

1. **An in-process renewal that gives up.** The accepted session's renewal
   retries a network failure three times, five seconds apart, then reports
   `reenrollment_required`. The device host calls `invalidate_grant`, and
   `github_application_host::invalidate_device_grant_now` deactivates the
   accepted session. The deactivation's `invalidate` then runs `start_delete`,
   which tombstones the row and erases both slots. A laptop that is offline
   when the renewal timer fires loses the grant this way.
2. **The in-process rotation's tombstone.** `prepare_device_rotation`, for an
   accepted session, still tombstones the row before the refresh material is
   spent ([[BPR-1048]]'s barrier). If that renewal then fails, or the process
   ends before the rotated grant's binding is stored, the next launch finds the
   row tombstoned and does not restore it.
3. **A restored session whose refresh slot is missing.** After a restored
   session is accepted, the host probes for the refresh slot. If it answers
   absent, `handle_refresh_probe` calls `binding_failure()` and dispatches a
   deactivation, and the grant is erased although its access token still works
   for hours.

A fourth, narrower window: quitting while a launch renewal's rotated access
token is still being stored deletes the provisional credential at shutdown.
The row stays live, so the next launch renews from the rotated refresh token
if that store landed.

## Scope

- Apply the owner's rule to the three in-process removals above. A renewal
  that gives up keeps the grant: the session ends or stays, visibly, and the
  row and both slots remain for the next launch. What the running session shows
  meanwhile is part of the design.
- Decide what the in-process rotation's crash-consistency barrier becomes
  without a tombstone that the next launch reads as a removal. BPR-0644's
  launch renewal answers this for itself: the row stays live, and a crash
  after the spend leaves the next launch to spend whatever the refresh slot
  holds. Coordinate with [[BPR-1112]], which also touches
  `prepare_device_rotation`.
- Keep the removals that are the person's own: Sign out, a replacing token, and
  a new device grant.

## Acceptance

- AC1 An accepted device grant whose in-process renewal gives up for a network
  or credential-store failure keeps both slots and a live row, and the next
  launch restores or renews it. A unit case and a composed course show it, and
  each fails with the erasure restored.
- AC2 A refused in-process renewal keeps the grant, says it needs signing in
  again, and offers Begin and Sign out, as BPR-0644's launch does.
- AC3 A restored session whose refresh slot is missing is not signed out and
  erased on its own. The card records what it does instead.
- AC4 `application_github_host`, `application_device_authorization_refresh`,
  the device-binding courses and the composition contracts pass.

## Boundaries

The core decides, the host performs, Qt presents. No live GitHub, credential or
device-flow request; fixtures only. [[BPR-0541]]'s rule that nothing resumes
from unvalidated material stands: keeping a grant is not using it.
