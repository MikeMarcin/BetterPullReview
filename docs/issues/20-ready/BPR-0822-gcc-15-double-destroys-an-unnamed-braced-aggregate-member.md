---
schema: "implement-rfc.card.v1"
id: "BPR-0822"
title: "GCC 15.2 destroys an unnamed braced aggregate member twice when a later initializer throws"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux under BPR-0816 (task-20260917-lnx-build)","found_at":"4883dbd9","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0822: GCC 15.2 destroys an unnamed braced aggregate member twice when a later initializer throws

## Defect

While fixing [[BPR-0816]], `bpr-linux` found a GCC 15.2 miscompile, not just
a warning. Take an aggregate whose member is itself an aggregate holding a
`std::string`. If that member is brace-initialized without naming its type
(`.Receipt = { .ServerCommentId = ... }`), and a later member's initializer
throws, GCC destroys the member twice. Outside the tree, AddressSanitizer
reports `attempting double-free` at `-O0`; at `-O2` the process aborts. Clang
21 and 22 are correct. Naming the type avoids it.

[[BPR-0816]] fixed the two sites GCC warned about at `-O3`:

- `src/adapters/github/src/github_mutation_contract.cpp`, where the later
  copies can throw `bad_alloc`;
- `src/adapters/sqlite/src/review_ledger_database.cpp`, where the edge cannot
  fire in practice.

**The warning appears only at `-O3`, so unwarned instances may remain.** No
survey was made. See
[the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md),
"The GCC miscompile". The reproducer is station-local on `bpr-linux`
(`/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/repro/asan.cpp`) and
is not committed.

## Scope

- Survey `src/` for designated initializers that brace-initialize an
  aggregate-typed member without naming its type, followed by a member whose
  initializer can throw. A clang-query or tree-sitter matcher is fine; say
  which, and how false negatives were bounded.
- Name the type at each site that matters, as BPR-0816 did. Keep values,
  order and behaviour unchanged.
- Decide whether a durable guard is worth it: a GCC `-O3` warning build on
  the Linux station, a tidy/format rule, or a note in
  `_coding-standard/STANDARD.md`. Record the decision.

## Acceptance

- The survey's method, and every site it found, are listed here with their
  disposition.
- The committed reproducer passes (no ASan report) under GCC 15.2 on the Linux
  station once the pattern is avoided, and fails with the pattern present.
  It may live under `tests/` as a Linux-only compiler canary if kept.
- The Linux `dev` and `app-release` presets still build stock, and the macOS
  dev build still passes.
