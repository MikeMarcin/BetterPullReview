---
id: BPR-0534
title: Catch MSVC's shadowing error on the clang hosts
status: Backlog
rfc: RFC-BPR-0008
depends_on: []
---

# Catch MSVC's shadowing error on the clang hosts

## Why

`src/app/main.cpp` landed on 2026-09-03 with an inner `auto passed`
declared while an outer `passed` was live ([[BPR-0316]] part one). Apple
Clang and GCC built it silently under the tree's warning set; MSVC raised
C4456 and, with warnings as errors, refused to build the Windows dev tree,
which blocked [[BPR-0315]] and [[BPR-0515]] on the station until the
integrator renamed it. The same class — a warning one compiler treats as an
error and the others never emit — has now cost a station round trip four
times this week (C4189 under [[BPR-0518]], C4996 under [[BPR-0519]],
quoting and CRLF under [[BPR-0522]], and this).

## Required final state

- The clang and GCC warning sets enable local-scope shadowing diagnostics
  (`-Wshadow` or the narrowest flag that matches C4456's scope) as errors in
  the same configurations that treat warnings as errors on MSVC.
- Every existing shadow the flag surfaces is fixed by renaming, not by
  narrowing the flag; the count is recorded on this card.
- `docs/development/build-and-test.md` names the cross-compiler warning
  classes the tree treats as errors and where each is enforced.

## Non-goals

- Enabling every MSVC-only diagnostic on clang; only the shadowing class this
  card names.
