---
schema: "implement-rfc.card.v1"
id: "BPR-1118"
title: "sqlite_profile_activation leaks /tmp/bpr-act-* and /tmp/bpr-xdg-* when an expectation fails, because expect calls std::exit"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"BPR-1113's negative controls, which removed the leftovers by name after each run (d03e2d61)","related_cards":["BPR-1113","BPR-0906","BPR-0881","BPR-0979"],"reservation":"docs/issues/_reservations/BPR-1117--BPR-1118--7bb2c9ebce22.json"}
---

# BPR-1118: sqlite_profile_activation leaks /tmp/bpr-act-* and /tmp/bpr-xdg-* when an expectation fails, because expect calls std::exit

## Why

`tests/sqlite_contract/profile_activation_tests.cpp` makes its scratch roots
with `mkdtemp`:

- each `scratch_directory` is a `/tmp/bpr-act-XXXXXX` root. It is grown to a
  real profile root's length with two nested directories;
- the Linux runtime-directory courses make `/tmp/bpr-xdg-XXXXXX`.

Each is removed by a destructor or by a call at the end of its course. But
`expect` reports a failed expectation with `std::exit(EXIT_FAILURE)`, which
runs no destructor of a local object. So every failing run leaves its roots in
`/tmp`. So does an exception the test does not catch, because it ends in
`std::terminate`.

[[BPR-1113]]'s negative controls hit this. They removed each run's
leftovers by exact name. Two older leftovers from 2026-09-23 are still in
`/tmp`: `/tmp/bpr-act-mWIPEe` and `/tmp/bpr-act-Kwl7Qp`.

## Scope

- A failed expectation, or an exception, still fails the row with its
  `FAILED:` line. It then unwinds, so every scratch root is removed.
- Each root is owned from the moment it exists. That includes a
  `scratch_directory` whose constructor fails after `mkdtemp`, and the
  `bpr-xdg` directories.
- A course that rewrites `XDG_RUNTIME_DIR` restores it before its scratch
  root goes. The root's endpoint is then removed where it resolved in the
  inherited environment, as on the passing path.
- Record what still leaves a root: a kill, a CTest timeout, an abort or a
  crash.
- Prove the two 2026-09-23 leftovers are this test's before removing them by
  exact name, and record them.

## Acceptance

- AC1 An expectation that fails at any stage of the POSIX courses leaves no
  `/tmp/bpr-act-*` or `/tmp/bpr-xdg-*`, and the row still fails with that
  expectation's `FAILED:` line. The stages are:
  - a scratch root half built;
  - a server bound in a private runtime directory;
  - a widened runtime directory;
  - the exact-length runtime directories;
  - a planted symlink;
  - a stale socket.

  A reverted trial at the same stages, with `HEAD`'s `std::exit`, leaves
  them.
- AC2 An exception from the standard library also fails the row with a named
  line and removes the roots. At `HEAD` it aborts and leaves them.
- AC3 The passing row is unchanged on Linux: `sqlite_profile_activation`
  passes and prints the same lines.
- AC4 The two 2026-09-23 leftovers are shown to be this test's, removed by
  exact name, and recorded here.
- AC5 The change is portable, so the row builds and passes on macOS and
  Windows.
