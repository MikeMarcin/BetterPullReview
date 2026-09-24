---
schema: "implement-rfc.card.v1"
id: "BPR-1103"
title: "Windows: application_attended_recent_replay and application_personal_discovery_public_path leave their QTemporaryDir data roots in %TEMP% on every run"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1051's Windows measurement and BPR-1050's sweep","found_at":"bcd57840","related_cards":["BPR-1051","BPR-1050","BPR-1047","BPR-1044","BPR-1039"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
---

# BPR-1103: Windows: application_attended_recent_replay and application_personal_discovery_public_path leave their QTemporaryDir data roots in %TEMP% on every run

## Defect

Two rows pass on Windows and leave their temporary directories behind in the
system temporary directory, every run:

| Row | Left per run | Each holds |
| --- | --- | --- |
| `application_attended_recent_replay` | 2 `attended_recent_replay-XXXXXX` | `data/` with `profile-runtime.lock` and five SQLite stores (`appearance-settings`, `epoch-catalog`, `profile-registry`, `release-update`, `review-queue`), in one of the two with their `-wal` and `-shm` files |
| `application_personal_discovery_public_path` | 1 `personal_discovery_public_path-XXXXXX` | `data/` with the same, plus `logs/diagnostics.log`, `.git-process-boundary.lock` and `work/` |

Nothing reports it: the rows pass, and `QTemporaryDir`'s destructor fails
silently. The names are the `QTemporaryDir` default template, the executable's
name and six random characters.

## Measured (bpr-main-win, MSVC 19.51 Debug, Qt 6.11.2)

- [[BPR-1051]]'s Windows runs at `bcd57840`, each row alone with `TEMP` and
  `TMP` pointed at a new empty directory: the replay row left 2 roots
  (30 entries), the discovery row 1 root (24 entries).
- [[BPR-1050]]'s 154-row `^application_` sweep at `bcd57840`: comparing
  `%TEMP%` before and after, these three roots are the only entries any row
  left.
- `%TEMP%` on the station held 21 older ones from 2026-09-23: 14
  `attended_recent_replay-*` and 7 `personal_discovery_public_path-*`. The
  station removed only the six its own runs made; the 21 were left to the
  operator.

## Scope

- Confirm the likely cause, files still open when `QTemporaryDir` removes the
  root.
- Make each course remove its root reliably, and fail loudly if it cannot,
  rather than leak it silently.
- Remove the 21 older leftovers only if they are provably from these courses,
  by their exact name pattern; otherwise list them.

## Acceptance

- AC1 The cause is named with evidence from a real Windows run.
- AC2 Both rows pass on Windows and leave nothing in a fresh `TEMP`, in N of N
  runs (N >= 3 each).
- AC3 Negative control: a course that cannot remove its root fails, naming
  what is left; and the courses as they were leave their roots again.
- AC4 macOS and Linux keep their path, or the change is run there.
