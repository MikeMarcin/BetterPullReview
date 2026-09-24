---
schema: "implement-rfc.card.v1"
id: "BPR-1111"
title: "The device-binding store latches corrupt on SQLite I/O and recovery errors after its first read"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-0884 (claude-fable-5-1, read-only, macOS from source), findings 1 and 3c","found_at":"2852dde0","reviewed_candidate":"02e3d59d","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision 8e1858ab-e9f7-4371-b854-71a6ab3e1946, 2026-09-24","related_cards":["BPR-0884","BPR-0841","BPR-0541","BPR-0644"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
---

# BPR-1111: The device-binding store latches corrupt on SQLite I/O and recovery errors after its first read

## Source

[[BPR-0884]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `8e1858ab`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source at `2852dde0`. The product files were unchanged since the
candidate, `02e3d59d`. The review found no blocking issue. This card takes
its finding 1 and the third part of its finding 3. The review is recorded on
BPR-0884 under "Independent review (2026-09-24)".

## Defect

BPR-0884 decided that **only a verdict on the admitted file latches**. The
code is narrower than that claim. Line references are the reviewer's, and the
files are unchanged at the revision where this card was written.

- **Only the first read is classified.** In `inspect()`
  (`src/adapters/sqlite/src/device_binding_store.cpp:281-285`), a failed
  `PRAGMA user_version` is `corrupt` only when SQLite reports that the file is
  not a database. Anything else is `unavailable`.
- **Every later failure maps to `corrupt`.** That covers the later reads in
  `inspect()` (`:288-289`, `:305-306`, `:312-313`) and `read_rows`' prepare
  and step (`:145-146`, `:157-158`). So `SQLITE_IOERR_*`, `SQLITE_NOMEM`,
  `SQLITE_PROTOCOL` and `SQLITE_BUSY_RECOVERY` at those points latch the store
  as corrupt for its lifetime.
- **The snapshot comment is true only inside `inspect()`.** The judgement's
  read transaction is `BEGIN` ... `ROLLBACK` at `:260-264`. The `read_rows`
  re-read at `:276` runs after that rollback. The comment at `:253-257` says
  no read can meet another connection's lock once the first read holds a
  snapshot. That holds for `inspect()`'s reads only.

**Reachability.** The reviewer ran a scratch probe, which showed that
ordinary lock holders cannot reach these paths. The I/O and WAL-recovery
class remains. The fault predates BPR-0884. BPR-0884 narrowed it, because
cancellation and busy no longer latch.

**The lane test is weaker than its claim (finding 3c).**
`test_device_binding_persistence_stays_drainable_when_its_posts_are_refused`
(`tests/application/github_application_host_tests.cpp:5492-5549`) checks
that each request gets at least one terminal. It does not check exactly one.
The reviewer suggests counting terminals per request.

## Scope

- Classify step and prepare results in `read_rows` through
  `detail::classify_sqlite_result`
  (`src/adapters/sqlite/src/catalog_database.cpp:4291-4306`), which is the
  reviewer's suggestion. Do the same for `inspect()`'s later reads. A
  transient I/O, memory, protocol or recovery failure then reports
  `unavailable` and closes the connection. A genuine verdict still reports
  `corrupt` and latches.
- Drop the `:276` re-read, or move it inside the snapshot, so that the comment
  at `:253-257` holds for every read `establish` judges. If a read has to stay
  outside the snapshot, change the comment to name it.
- Pin exactly one terminal per request in the lane test, with a per-request
  counter.
- State the resulting rule wherever BPR-0884's rule is stated in live code
  comments or architecture notes. BPR-0884 is Done, so it gets a pointer
  rather than a rewrite.

## Acceptance

- AC1 A transient SQLite failure after the first read is not a verdict. An
  injected I/O-class failure at one of `inspect()`'s later reads, and at
  `read_rows`' prepare and step, reports `unavailable`. The same store is
  usable on its next call, and a contract that reuses one store shows it. The
  injection mechanism is the implementer's choice; a shim VFS is one way. If
  a class cannot be injected portably, record why and pin its classification
  at the narrowest level that can.
- AC2 A genuine verdict still latches. BPR-0884's corruption pins still pass
  unchanged.
- AC3 Every read `establish` judges runs inside the one snapshot, or the
  comment names each read that does not.
- AC4 The lane test counts terminals per request and asserts exactly one for
  each.
- AC5 Every new assertion fails on the unchanged product (negative control).
- AC6 No regression: `sqlite_device_binding_storage`,
  `sqlite_private_store_open`, `application_github_host`,
  `application_device_binding_restart`, `_launch_renewal` and `_relaunch`.

The code is platform-neutral. One platform's run meets the acceptance, unless
the injection turns out to depend on the platform's VFS.

## Recorded from the same review, not in scope

These are informational findings. No change is asked for.

- A stop at `ensure_open()`
  (`src/adapters/sqlite/src/device_binding_store.cpp:241-242`) can turn
  `corrupt` into `cancelled` without latching. The next call judges the file
  again.
- A stranded successor always gets `unavailable`
  (`src/application/src/device_binding_store.cpp:177`).
