---
schema: "implement-rfc.card.v1"
id: "BPR-1230"
title: "Windows: github_review_comment_permalink fails at canonical RFC0022 path escaping, contained and uncontained"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
---

# BPR-1230: Windows: github_review_comment_permalink fails at canonical RFC0022 path escaping, contained and uncontained

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`github_review_comment_permalink` fails on Windows. Contained, it stops at
`FAILED: publication SQLite journal opens`. Run directly, uncontained, it gets
further: it prints `recorded-contract-refusal-courses:29`, then
`FAILED: range/historical/plain-view shares canonical RFC0022 path escaping`,
exit 1. So the uncontained failure is an assertion about path escaping, which
Windows fails. It is not a boundary effect.

The source, `tests/github_contract/review_comment_permalink_tests.cpp`, last
changed in `24b7f66a` (2026-09-27). Its target needed
`BetterPullReview::Platform` to compile on Windows, which `537f718d` added.

## Scope

- Find which escaping differs on Windows. Plausible, not measured: a native
  path separator or drive letter reaching the escaped path.
- Fix it in the layer that owns the escaping. Decide the row's containment
  separately: the store open fails contained as the other scratch-root store
  rows do.

## Acceptance

- The row passes on Windows, and the escaping assertion passes unchanged.
- macOS and Linux still pass.

## Fixed at `9f8c34cc`, verified on Windows (bpr-main-win, 2026-09-30)

There were three causes. Two were in the product.

- `compile_tour_permalink` built its `std::filesystem::path` from a
  `std::string`. Windows reads that in the ANSI code page, so `α` came back as
  different bytes. The path is now built from the UTF-8 bytes. This was the
  failure recorded above.
- A review permalink went through `generic_u8string()`. On Windows that turns
  a backslash, a legal byte in a Git path, into a separator before
  `encoded_path` could refuse it. So `dir/back\slash.cpp` compiled into a link
  to `dir/back/slash.cpp`. The path is now taken as given, and `encoded_path`
  refuses a backslash before its own generic conversion.
  - Production endpoint paths come from Git tree entries, which are always
    separated by `/`. So nothing legitimate is refused.
- In the test, the endpoint path was built from a UTF-8 `std::string` in the
  same way, and it now converts from UTF-8. Its `dir/bad\xFF` case is
  POSIX-only. A Windows path is UTF-16 and cannot hold bytes that are not
  UTF-8: MSVC's conversion throws, and the row aborted.

The row is now `WINDOWS_UNCONTAINED`, pinned with its measurement. Contained, it
stops at `publication SQLite journal opens` under its scratch root, as its
sibling store rows do.

On Windows, `github_review_comment_permalink` passes, 11.14 s, through `ctest`.
`test_network_isolation_manifest` does not name it.

Still open: a macOS and a Linux run of the row. The card stays in Backlog
until then.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

`github_review_comment_permalink` passes, 3.92 s. That is the Linux half of "macOS and Linux still pass".

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## macOS — red-test backlog milestone, 2026-09-30

macOS 26 (Darwin 25.6) arm64, Apple clang, Qt 6.11.2 offscreen, `build/dev` (Debug, `-Werror`) at `69493e235`, which contains the Windows fix.

`github_review_comment_permalink` passes through `ctest`, 3.63 s. With the Linux run above, that is "macOS and Linux still pass"; all three platforms now pass.
