---
schema: "implement-rfc.card.v1"
id: "BPR-0841"
title: "Harden the private store open helper after BPR-0813's critical review"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"dsp-003 critical review of BPR-0813 (711d3ddc)","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0841: Harden the private store open helper after BPR-0813's critical review

## Origin

An independent critical review of [[BPR-0813]] (`711d3ddc`, merged into main
at `902cda35`) found **no blocker**. It read the source, SQLite 3.53.4's
amalgamation and the recorded Windows evidence. These are its non-blocking
findings, recorded on BPR-0813.

## Scope

1. **Leak on `bad_alloc`.** `detail::open_private_store_database`
   (`src/adapters/sqlite/src/catalog_database.cpp` ~4771) catches `bad_alloc`
   without closing a connection it has already opened. On Windows that keeps
   the store file held. Close on every failure path, for example with an RAII
   guard, as the helper's contract says.
2. **Retry latch.** `device_binding_store.cpp` (~215) now sets `Failed` when
   admission fails, so later `ensure_open` calls return `corrupt` without
   retrying. Before BPR-0813 an admission failure returned `unavailable` and
   did not latch. Restore the earlier retry behaviour for admission failures,
   or record why latching is correct.
3. **Side files (Windows).** SQLite opens `<name>-journal` by path, without
   refusing a reparse point. A reparse point pre-placed in `state\v1` would be
   followed. The code behaved the same before BPR-0813, and exploiting it
   needs write access to the owner's state directory. Decide whether the
   catalog's secure VFS route should cover the private stores' side files too.
4. **Host-dependent course (Windows).** The `symbolic-link` course needs
   Developer Mode or the symlink privilege, and without it fails rather than
   skipping with a named reason.
5. **Harness.** `private_store_open_tests` prints `pass` for an unrecognised
   argument.
6. **Path encoding (Windows).** `path.u8string()` can throw
   `std::system_error` for a lone-surrogate root name, and the helper catches
   only `bad_alloc`. Map it to an ordinary failure.

## Acceptance

- Items 1, 2, 5 and 6 are fixed, each with a course or a reverted trial that
  shows it matters. Items 3 and 4 are fixed or explicitly declined with a
  reason.
- The Windows and macOS store courses listed on BPR-0813 still pass.
