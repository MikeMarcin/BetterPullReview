---
schema: "implement-rfc.card.v1"
id: "BPR-0811"
title: "Windows: device-binding restart no longer activates the public preview"
status: "Ready"
rfcs: ["RFC-BPR-0016", "RFC-BPR-0017"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-win under BPR-0806","found_at":"d587b5f1","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0811: Windows: device-binding restart no longer activates the public preview

## Defect

`application_device_binding_restart` on `windows-dev` at `d587b5f1`, through
ctest and directly, after 47.61 s:

    FAILED: ordinary Public preview selection becomes active

Before [[BPR-0806]] it failed earlier, at `FAILED: application startup succeeds`
after 0.09 s. It passes on macOS at `c0f2fa31` (29.34 s). RFC-BPR-0016 closed
Auto-Verified on 2026-09-04 with its final-head course replayed green on
Windows x64 ([[BPR-0285]]), so this is a Windows regression against a closed
RFC's recorded platform claim. The Windows default-root startup failure fixed
in BPR-0806 probably hid it since some later change.

## Scope

- On the Windows station, find what differs from macOS when the course
  selects the ordinary Public preview after restart: profile or host
  selection, the anonymous-read capability, storage admission, or the
  forward-slash path class BPR-0806 fixed at another site.
- Find the first bad revision if that is cheaper than reading, using
  `0d19c990` (RFC-BPR-0016's Windows closeout head) as the known good.
- Fix at the source. Do not relax the course.

## Acceptance

- `application_device_binding_restart` passes through `ctest --preset windows-dev`
  on the Windows station, and the cause and first bad revision (if found) are
  named on this card.
- The macOS course still passes.
