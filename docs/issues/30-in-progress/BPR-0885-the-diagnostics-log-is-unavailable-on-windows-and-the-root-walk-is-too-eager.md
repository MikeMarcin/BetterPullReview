---
schema: "implement-rfc.card.v1"
id: "BPR-0885"
title: "The diagnostics log is unconditionally unavailable on Windows, and its root walk creates shared ancestors"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable logging partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0885: The diagnostics log is unconditionally unavailable on Windows, and its root walk creates shared ancestors

## Defect

Two faults in `src/adapters/logging/src/quill_log.cpp`, filed together because
they are the same function's ends.

**1. Windows has no diagnostics log at all.** `open_log` exists only under
`#if !defined(_WIN32)` (`:308-312`). On Windows `File` stays null and
`status()` reports `unavailable` forever. No card in
`docs/issues/{00..50}` names this, so it is not a recorded limitation — it is
just absent. Given how much of this run's Windows work turned on reading what
the application reported, a platform with no log is a real handicap.

**2. The root walk creates shared ancestors and refuses symlinked ones.**
`:271-284` `mkdirat(..., 0700)`s **every missing component** of
`<root>/logs` from `/` downwards — including directories above the storage
root, which are not the application's to create — and opens each with
`O_NOFOLLOW`. So a root under a symlinked ancestor, which on macOS includes
`/var` and `/tmp`, yields `unavailable` with no diagnostic at all. The stores
already have the right shape for this: `validate_private_storage_root`.

## Scope

- Give Windows an `open_log` with the same properties the POSIX one enforces:
  owner-private, no reparse point, single link, exclusive lock. If that cannot
  be done, write down why and make `status()` say so rather than reporting a
  bare `unavailable`.
- Validate the root the way the stores do, and create only the `logs`
  component. Creating `/Users`, `/home` or `/var` is not this component's
  business.
- Report the reason when the log cannot be opened. A silent `unavailable` is
  how both of these survived.
- While there: check that whatever Windows writes honours the redaction
  properties `application_logging_tests.cpp:87-88` pins — no access token,
  refresh token, data-root path or repository name. [[BPR-0891]] covers the
  missing Windows registration of that row and should land with this.

## Acceptance

- Windows either has a log with the stated properties, or a recorded reason and
  an honest status.
- A storage root beneath a symlinked ancestor opens its log on macOS.
- The component never creates a directory above the storage root.
- A failure to open names its cause.
