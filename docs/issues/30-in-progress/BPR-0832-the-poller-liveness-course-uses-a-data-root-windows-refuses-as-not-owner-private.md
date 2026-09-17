---
schema: "implement-rfc.card.v1"
id: "BPR-0832"
title: "The poller liveness course uses a data root Windows refuses as not owner-private"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0832: The poller liveness course uses a data root Windows refuses as not owner-private

## Defect

`application_poller_liveness` fails on Windows at "application startup
succeeds", so neither its BPR-0802 boundary line nor [[BPR-0827]]'s tick
subtest is reached. A reverted diagnostic on `bpr-win` showed
`startup_failure::registry`: the course's bare `QTemporaryDir` data root is
not owner-private on Windows, where a temporary directory inherits its
parent's access-control list. On macOS and Linux `mkdtemp`'s `0700` already
is owner-private, so the course passes there (Linux: dsp-002 confirmation).

Reported by `bpr-win` in `task-20260917-win-hardening` step 1
(`docs/verification/BPR-0802-windows-confirmation.md`), not carded there.

## Scope

- Give the course a data root that is owner-private by construction, using
  the product's own definition (`bpr::adapters::sqlite::make_storage_root_owner_private`
  or the course's existing private-root helper), not a Windows-only skip.
- [[BPR-0820]] narrows the automated-run private profile the same way; reuse
  that rule rather than add a second one.

## Acceptance

- `application_poller_liveness` passes on the Windows station, reaching its
  boundary line and the tick subtest.
- It still passes on macOS.
