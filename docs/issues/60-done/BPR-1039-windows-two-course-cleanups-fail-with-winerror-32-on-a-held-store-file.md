---
schema: "implement-rfc.card.v1"
id: "BPR-1039"
title: "Windows: two course cleanups fail with WinError 32 because an application store file is still held when the temporary helper removes the root"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0876, uncontained trial on bpr-main-win","found_at":"470920d5","related_cards":["BPR-0876","BPR-0868","BPR-0867"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
acceptance: [{"criterion":"AC1 The holder is named with evidence","outcome":"pass","revision":"f416d2bb","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug: before the fix both courses failed 3 of 3 uncontained with RemovalFailed [WinError 32]; a station wrapper asked the Windows Restart Manager (RmGetList) at the failed removal: device-bindings.sqlite3 held by 'pid=16756 app=Python self=True', profile-registry.sqlite3 by 'pid=46304 app=Python self=True', each course holding six live sqlite3.Connection objects. The holder is the course's own Python process: `with sqlite3.connect(...)` never closes, and on CPython 3.14 the connection lives in a reference cycle.","path":"docs/issues/60-done/BPR-1039-windows-two-course-cleanups-fail-with-winerror-32-on-a-held-store-file.md#the-holder-is-the-course-itself-closed-both-pass-bpr-main-win-2026-09-23"}},{"criterion":"AC2 Run uncontained on Windows, both courses pass N of N (N >= 3), with no RemovalFailed","outcome":"pass","revision":"f416d2bb","evidence":{"kind":"command","detail":"At f416d2bb (built sources identical), application_device_binding_relaunch and application_profile_window_layout_public_path run uncontained pass 3 of 3 each (DEVICE_BINDING_RELAUNCH_OK, PROFILE_WINDOW_LAYOUT_OK), no RemovalFailed, no leftover root; also 3 of 3 each at 0615cc67's sources with the same course change. On the way, e2ea86ad fixed an MSVC-only abort (array iterators incompatible) in has_file_fixture_keys that had stopped the layout seed at f7e93488 3 of 3.","path":"docs/issues/60-done/BPR-1039-windows-two-course-cleanups-fail-with-winerror-32-on-a-held-store-file.md#the-holder-is-the-course-itself-closed-both-pass-bpr-main-win-2026-09-23"}},{"criterion":"AC3 A failed removal is still reported by name; BPR-0868's behaviour is kept","outcome":"pass","revision":"f416d2bb","evidence":{"kind":"command","detail":"scripts/contained_temporary_support.py is unchanged; the pre-fix runs show RemovalFailed naming both files; contained_temporary_support_contract and contained_temporary_usage_contract pass.","path":"docs/issues/60-done/BPR-1039-windows-two-course-cleanups-fail-with-winerror-32-on-a-held-store-file.md#the-holder-is-the-course-itself-closed-both-pass-bpr-main-win-2026-09-23"}}]
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

## The holder is the course itself; closed, both pass (bpr-main-win, 2026-09-23)

**AC1: the holder, named.** Each course was run uncontained on `build/dev`
(MSVC 19.51 Debug, Qt 6.11.2), using its registered command without the
network sandbox. At `0615cc67`'s sources both failed 3 of 3 exactly as
recorded above: `RemovalFailed` with `[WinError 32]` on
`device-bindings.sqlite3` and on `profile-registry.sqlite3`. A station
wrapper, which was not committed, then ran each course once. When the helper
failed to remove a file, the wrapper asked the Windows Restart Manager
(`RmGetList`) which process held it:

| Course | File | Holder |
| --- | --- | --- |
| `application_device_binding_relaunch` | `…\application\state\v1\device-bindings.sqlite3` | `pid=16756 app=Python self=True` |
| `application_profile_window_layout_public_path` | `…\data\state\v1\profile-registry.sqlite3` | `pid=46304 app=Python self=True` |

In both, the holder is the course's own Python process. The application had
exited, and no child process was left. At that moment each course still had
six `sqlite3.Connection` objects alive. The courses read their stores with
`with sqlite3.connect(...) as connection:`, and that only ends the
transaction; it never closes the connection. On CPython 3.14 the connection
then sits in a reference cycle until the cyclic garbage collector runs. On
POSIX an open file can still be unlinked, so only Windows sees the failure.
[[BPR-0980]] had found and fixed the same thing in one place,
`profile_activation_contract.py`'s `windows()`.

**The fix.** It is in the courses, where the handle is held. The new
`tests/end_to_end/bpr_store_connection.py` gives courses `store_connection()`.
It commits when the block succeeds, rolls back when it raises, and closes
the connection either way. The two courses now open every store through it,
and so do the two other contained courses that opened stores the same way
(`profile_owner_admission_contract.py` and
`profile_window_review_restoration_contract.py`): 13 openings in four files.
`profile_activation_contract.py` already closed its connection and is
unchanged. The helper's retry was not lengthened: the file was never released
late. It was simply never released.

**AC2.** Run uncontained, both courses pass 3 of 3 with no `RemovalFailed`,
printing `DEVICE_BINDING_RELAUNCH_OK` and `PROFILE_WINDOW_LAYOUT_OK` each
time. This held at `0615cc67`'s sources, and again at `f416d2bb`, where this
change landed. No `bpr-0644-relaunch-*` or `bpr-0708-layout-*` root
was left in `%TEMP%`.

**A regression found on the way, fixed separately.** At `f7e93488`, before
`e2ea86ad`, the layout course's seed aborted with MSVC's
`array iterators incompatible` (`0xC0000409`) in all three runs. A
temporary CRT report hook in the test printed the stack. The stack ends in
`has_file_fixture_keys` (`src/adapters/github/src/github_read_contract.cpp`),
added in `f3d19c2a` for [[BPR-0892]]: a `constexpr` local `std::array`
searched inside a lambda, which MSVC may materialize as separate objects.
`e2ea86ad` makes it `static constexpr`. `github_read_contract`,
`github_paged_read`, `github_contract_fixture_security`,
`application_profile_activation_public_path`,
`application_profile_window_restart_public_path` and
`application_device_binding_launch_renewal` pass after it. The failing runs
before it are its negative control.

**AC3.** `scripts/contained_temporary_support.py` is unchanged, so a failed
removal still raises `RemovalFailed` and names its file ([[BPR-0868]]). The
pre-fix runs above show it naming both files.
`contained_temporary_support_contract` and `contained_temporary_usage_contract`
pass.

**Registered, the rows are still red, for [[BPR-0876]]'s reasons.** Run
contained as registered, `application_device_binding_relaunch` stops at
`SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable` and
`application_profile_window_layout_public_path` at `seed failed (3)`.
`application_profile_owner_admission_public_path` stops at
`FAILED: seed registry` ([[BPR-0907]]), and
`application_profile_window_review_restoration_public_path` at
`auth-seed failed (3)`. `application_profile_activation_public_path` passes.
The leftover roots from the pre-fix runs, and one container root that the
contained review-restoration run leaked, were removed by hand.

The logs are station records under `build/logs/bpr-1039-main-win/` and are
not committed.
