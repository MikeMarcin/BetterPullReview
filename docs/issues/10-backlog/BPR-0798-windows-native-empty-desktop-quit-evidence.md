---
schema: "implement-rfc.card.v1"
id: "BPR-0798"
title: "Windows: observe the native empty-desktop quit"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: []
platforms: ["win"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"RFC-BPR-0033 B1, carried as not run since the profile-chooser milestone","acceptance":["B1"]}
---

# BPR-0798: Windows: observe the native empty-desktop quit

## Outcome

RFC-BPR-0033 B1 says that on Windows, closing the last window of the last open
profile quits through the existing drain and Quit path, and that the restore
records still tell that close from a Quit as RFC-BPR-0024 requires. On this
pack's macOS lane only the **injected** policy outcome was ever proven:
`native_empty_desktop_policy()` answers `stay_resident` on macOS, so the quit
branch is never taken there.

Every closeout row in this pack has carried that as **not run, never as
passing**, and said in as many words that no card carrying `platforms` metadata
existed to hold it. This card is that home. It was allocated on main in d8 so
the Windows lane has somewhere to record the observation.

## Acceptance

- On target-native Windows, closing the last window of the last open profile
  reaches quiescence through the existing drain and Quit path with exit code 0,
  and presents no chooser on the way out.
- The restore record distinguishes that close from an explicit Quit: the next
  launch opens a window whose identity differs from the explicitly closed one.
- `native_empty_desktop_policy()` is observed answering `quit` on the target,
  rather than assumed from the `#if`.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run**: no
Windows observation exists for this behaviour, and none is to be inferred from
the injected policy course.

## Boundaries

No GitHub or GHES mutation, credential access or provider change is authorized
by this card. macOS residency is [[BPR-0796]]; Linux is [[BPR-0799]].
