---
schema: "implement-rfc.card.v1"
id: "BPR-1039"
title: "Windows: two course cleanups fail with WinError 32 because an application store file is still held when the temporary helper removes the root"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0876, uncontained trial on bpr-main-win","found_at":"470920d5","related_cards":["BPR-0876","BPR-0868","BPR-0867"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
---

# BPR-1039: Windows: two course cleanups fail with WinError 32 because an application store file is still held when the temporary helper removes the root

## Defect

Run uncontained, two Windows courses pass every step. They fail only when the
contained temporary helper (`scripts/contained_temporary_support.py`)
removes their root, because one application store file is still open:

| Course | File the helper cannot remove |
| --- | --- |
| `application_device_binding_relaunch` | `…\bpr-0644-relaunch-<id>\application\state\v1\device-bindings.sqlite3` |
| `application_profile_window_layout_public_path` | `…\bpr-0708-layout-<id>\data\state\v1\profile-registry.sqlite3` |

Both fail with `scripts.contained_temporary_support.RemovalFailed: contained
temporary: could not remove <file>: [WinError 32] The process cannot access
the file because it is being used by another process`. Since [[BPR-0868]] the
helper names the file instead of swallowing the error, and that is how this
was found.

A few seconds later, no course process and no application process was still
running. So either a process that had not yet exited held the handle when
the helper ran, or a handle outlived its process's visible exit, and the
helper's retry window ends before Windows releases the file. POSIX unlinks
open files, so only Windows sees this.

## Evidence (bpr-main-win, 2026-09-23)

`build/dev` at `470920d5` (MSVC 19.51 Debug, Qt 6.11.2), serial and
offscreen. Each course was made uncontained through a temporary
`WINDOWS_UNCONTAINED`, reverted afterwards, as [[BPR-0876]] records. The log
is `build/logs/bpr-0872-0876-main-win/trial-uncontained.log`, a station record
that is not committed.

- `application_device_binding_relaunch`: every step passes, then the helper
  raises `RemovalFailed` on `device-bindings.sqlite3`.
- `application_profile_window_layout_public_path`: completes in 38.14 s, then
  the helper raises `RemovalFailed` on `profile-registry.sqlite3`.

Registered contained, the two courses fail earlier, for reasons [[BPR-0876]]
tracks. These are `SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable` and
`seed failed (3)`. This card covers only the teardown.

## Scope

- Find who holds the file at removal: a child process not yet exited (the
  application, the agent supervisor, the MCP helper, or Git), a SQLite handle
  or connection pool the application does not close before it exits, or
  Windows releasing the file late (a delete-pending state or an indexer).
  Handle-level evidence at the moment of failure settles it: the holding
  process's image and PID.
- Fix it where it belongs. If the application exits while still holding a
  store, close it at quit. If the harness removes the root before the
  process tree has exited, wait for that exit. If the release is only late,
  give the helper a bounded retry that names the file when it gives up.
  Do not hide a leak with an unbounded wait.

## Acceptance

- AC1 The holder is named with evidence.
- AC2 Run uncontained on Windows, both courses pass N of N (N ≥ 3), with no
  `RemovalFailed`.
- AC3 A failed removal is still reported by name. [[BPR-0868]]'s behaviour
  is kept.
