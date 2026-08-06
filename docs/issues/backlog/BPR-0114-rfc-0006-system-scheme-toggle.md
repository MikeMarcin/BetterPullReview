---
id: BPR-0114
title: Verify System mode follows a live OS scheme change
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0106, BPR-0183, BPR-0184]
---

# Verify System mode follows a live OS scheme change

On the real macOS composition, prove that System mode follows an
operating-system colour-scheme change while an explicit mode does not.

## This card was misfiled, and BPR-0180 says why

It was parked as owner-attended, which asserted that the behaviour existed and
only a person was missing. That was wrong. BPR-0180 measured that the
application composes no `appearance_profile` and does not link
`appearance_application_host` at all, so there is no appearance mode to select
and no variant to observe. RFC-BPR-0006 names this claim's surface as **Real
application** and its observation as **profile variant change**; neither
existed.

Attendance was never what was missing. BPR-0183 makes the profile drive the
composed window, and BPR-0184 supplies the gesture that selects an explicit
mode. Only then is there something for a person to watch.

The product owner authorised the appearance change on 2026-08-06 and that
authorisation is recorded here, but the run could not proceed. Authorisation is
per-session and must be sought again when this card is actually run.

## What is missing, once the dependencies land

The desktop owner, once, to authorise changing and restoring the global
appearance and to observe the result. The host must start in Light, because the
existing probe's transition is Light to Dark.

## Required direct evidence

- With System selected, `appearance_profile::variant()` changes when the owner
  changes the operating-system scheme, and the composed surfaces follow.
- With an explicit mode selected through BPR-0184's gesture, the same
  operating-system change alters neither the variant nor the surfaces.
- The scheme is changed by the owner in System Settings. Nothing in this
  repository changes the global appearance programmatically.
- The global appearance is restored afterwards, and the restoration is
  confirmed rather than assumed.
- The observation is read from the running application, not from a raster
  captured earlier.

## What must not be done instead

Showing that Qt's native palette propagates and calling it System mode. That is
`QMacStyle` following the operating system, which happens whether or not
BetterPullReview has an appearance model at all, and it is exactly what kept
this gap invisible for as long as it was.

The static fresh-install-defaults-to-System claim is separate and is proven
unattended by BPR-0106.
