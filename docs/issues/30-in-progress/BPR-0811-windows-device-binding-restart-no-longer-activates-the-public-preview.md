---
schema: "implement-rfc.card.v1"
id: "BPR-0811"
title: "Windows: device-binding restart no longer activates the public preview"
status: "In Progress"
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

## Evidence

Station `bpr-win` ran task `task-20260917-win-regressions`. The base was
`53a0a7477264c71423b11fa3bc85d2e668b389b4`.

### Cause

**This is not a Windows regression against a recorded green run.** The course
has never passed on Windows.

- `tests/end_to_end/device_binding_restart_tests.cpp` does not exist at
  `0d19c990`, the RFC-BPR-0016 Windows closeout head that [[BPR-0285]]
  recorded.
- The file was added a day later by `141250a6` ([[BPR-0541]], 2026-09-04),
  which is not an ancestor of `0d19c990`, and it was verified on the Mac
  `build/dev` graph.
- Its last lifetime, "preview regression", calls `activate_public_preview()`
  and waits for the signed-out mode. It depends on the *shipped* public-preview
  selection without saying so.
- `shipped_public_preview_capability()` in `src/app/src/github_application.cpp`
  returns `Available = true` only under `__APPLE__` ([[BPR-0316]]). Linux and
  Windows keep the verified unavailable selection until their own acceptance
  slices.

On Windows, then, the composition hands the host no anonymous authority
(`AnonymousPullRequests == nullptr`). `activate_public_preview_now` refuses with
`public_preview_unavailable`, which the panel projects as `mode_unavailable`.

Throwaway diagnostics, reverted, showed exactly that:

- `activate profiles=1 selected=1 card=1`: the intent was dispatched.
- `mode=0 refusal=1 status=2 text=Reading without signing in is unavailable in this build.`

[[BPR-0806]]'s startup failure hid this until `d0964fdd`. The first bad revision
is the course's own introduction, `141250a6`. No product change regressed.

### Fix

The course now states its precondition. Before the preview lifetime it sets
`options.PublicPreviewEnabledCourse = true`, the recorded-fixture selection
override the setup destination course already uses ([[BPR-0732]]). With it,
the lifetime drives the real host, provider and admission rules on every
platform.

- The assertions are unchanged: the preview becomes active, and the retired
  near-expiry renewal does not deactivate it after 6 s.
- On macOS the shipped selection is already available, so the override changes
  nothing there.
- Shipping public preview on Windows is not this card's to decide: it needs its
  own target-native acceptance slice.
- The change is test source only, and it is portable.

### Verification

| Check | Result |
|---|---|
| `ctest --preset windows-dev -R "^application_device_binding_restart$" --output-on-failure` (Debug, MSVC, uncontained registration) | Passed, 38.62 s |
| The same executable run directly with `QT_QPA_PLATFORM=offscreen` | `REFRESH_RESUMES:1` and `device-binding-restart-tests:pass`, 38.03 s |
| Before the fix, on the same build | `FAILED: ordinary Public preview selection becomes active` |

The macOS course is not run on this station. The integrator runs it at merge;
it is expected to be unchanged because the forced selection equals the shipped
one there.
