---
schema: "implement-rfc.card.v1"
id: "BPR-1111"
title: "The device-binding store latches corrupt on SQLite I/O and recovery errors after its first read"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-0884 (claude-fable-5-1, read-only, macOS from source), findings 1 and 3c","found_at":"2852dde0","reviewed_candidate":"02e3d59d","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision 8e1858ab-e9f7-4371-b854-71a6ab3e1946, 2026-09-24","related_cards":["BPR-0884","BPR-0841","BPR-0541","BPR-0644"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
acceptance: [{"criterion":"AC1 A transient SQLite failure after the first read is not a verdict: an injected I/O-class failure at one of inspect()'s later reads, and at read_rows' prepare and step, reports unavailable, and the same store is usable on its next call, shown by a contract that reuses one store; a class that cannot be injected portably is pinned at the narrowest level","outcome":"pass","revision":"f9c42297","evidence":{"kind":"command","detail":"macOS build/dev. sqlite_device_binding_storage transient_read_failures, one store per fault, each fault counted as injected once: SQLITE_IOERR_READ from the connection's own main-file I/O methods at PRAGMA quick_check and at the scope read (later reads in inspect()), at read_rows' step in the judgement and in load; SQLITE_NOMEM (sqlite3_hard_heap_limit64) at read_rows' prepare in the judgement. Each reports unavailable and the same store then reads the row. PROTOCOL, BUSY_RECOVERY and BUSY_SNAPSHOT (WAL-index codes no contract can produce on demand) are pinned at the shared classifier with the other environment codes.","path":"docs/issues/60-done/BPR-1111-the-device-binding-store-latches-corrupt-on-sqlite-io-and-recovery-errors-after-its-first-read.md#tests"}},{"criterion":"AC2 A genuine verdict still latches; BPR-0884's corruption pins still pass unchanged","outcome":"pass","revision":"f9c42297","evidence":{"kind":"command","detail":"sqlite_device_binding_storage: the unknown-schema-object latch (corrupt, still refused after repair, a new store loads), the oversized hostile field, the future version, the malformed legacy row and foreign scope and rows pass unchanged. The classifier pins CORRUPT, CORRUPT_INDEX, NOTADB, TOOBIG and an unrequested INTERRUPT (the step ceiling) as corrupt."}},{"criterion":"AC3 Every read establish judges runs inside the one snapshot, or the comment names each read that does not","outcome":"pass","revision":"f9c42297","evidence":{"kind":"observation","detail":"establish no longer re-reads the rows after inspect's ROLLBACK; every judged read is in inspect inside BEGIN...ROLLBACK, the migration reads back its own writes inside its write transaction, and the comment on establish says so. load's later row read is not part of the judgement, never latches, and is classified the same way.","path":"docs/issues/60-done/BPR-1111-the-device-binding-store-latches-corrupt-on-sqlite-io-and-recovery-errors-after-its-first-read.md#what-changed"}},{"criterion":"AC4 The lane test counts terminals per request and asserts exactly one for each","outcome":"pass","revision":"f9c42297","evidence":{"kind":"command","detail":"application_github_host test_device_binding_persistence_stays_drainable_when_its_posts_are_refused counts terminals per request: exactly one for each admitted request, none for a withdrawn one, and no earlier request gets a second one later."}},{"criterion":"AC5 Every new assertion fails on the unchanged product (negative control)","outcome":"pass","revision":"f9c42297","evidence":{"kind":"command","detail":"HEAD's store file with a test-hook shim answering its old later-read mapping (corrupt), expect non-fatal: 23 new assertions fail (five injected faults at unavailable and at the next call, and the 13 environment-code pins); the faults were injected every time. The five verdict pins and the cancelled pin pass on both, as AC2 requires. Trial: without quick_check finding parsing the quick_check fault still latches. The unchanged lane already gives exactly one terminal, so AC4's assertion cannot fail on it; a trial lane that delivers a second terminal to the stranded successor fails the new test and passes the old one.","path":"docs/issues/60-done/BPR-1111-the-device-binding-store-latches-corrupt-on-sqlite-io-and-recovery-errors-after-its-first-read.md#negative-controls-ac5"}},{"criterion":"AC6 No regression: sqlite_device_binding_storage, sqlite_private_store_open, application_github_host, application_device_binding_restart, _launch_renewal and _relaunch","outcome":"pass","revision":"f9c42297","evidence":{"kind":"command","detail":"6 of 6 pass on macOS: application_github_host, sqlite_private_store_open, sqlite_device_binding_storage 2.82 s, application_device_binding_restart 30.1 s, _launch_renewal 67.0 s, _relaunch 10.2 s. Windows and Linux confirm in their next ordinary pass (command on the card)."}}]
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

## Built (Mac batch 8, 2026-09-24)

Claimed at `9bf06725`. The product change, its tests and the architecture
note are `f9c42297`. Station `bpr-main-mac`: macOS 26.6.2 arm64, AppleClang,
Qt 6.11.2, `build/dev` Debug.

### What changed

`src/adapters/sqlite/src/device_binding_store.cpp`:

- **Every read is classified, not only the first.** The store's reads go
  through one classifier, `read_failure`. It is `detail::classify_sqlite_result`,
  as the reviewer suggested, with the read scope's own limits added as
  verdicts. That covers all of `inspect()`'s reads, and `read_rows`' prepare
  and step wherever `read_rows` runs: in the judgement, in `load`, `store` and
  `erase`, and in the migration.
  - **Verdicts (`corrupt`):** `SQLITE_CORRUPT` and `SQLITE_NOTADB`, which is
    SQLite's own finding. Also the read scope's two limits on hostile content:
    an interrupt nobody asked for (the step ceiling BPR-0884 already counted as
    a verdict) and `SQLITE_TOOBIG` (the 1 MiB length limit). A missing row, a
    value of the wrong type, or a second row where one is expected is also a
    verdict, as before.
  - **Everything else is `unavailable`:** I/O (`SQLITE_IOERR_*`), `NOMEM`,
    `PROTOCOL`, `BUSY`, `BUSY_RECOVERY`, `BUSY_SNAPSHOT`, `LOCKED`, `CANTOPEN`
    and `FULL`. So is `SQLITE_ERROR`: `classify_sqlite_result` treats it as
    environment. On this schema it can arise only before the file's identity
    is judged, for example an unsupported schema format. A hostile file of
    that kind is refused again at every call instead of once, which costs one
    bounded read each time. The host treats `corrupt` and `unavailable` alike,
    except that only `corrupt` latches.
  - A read that was asked to stop is `cancelled`.
- **quick_check's own findings are classified too.** This was found while
  building the injection. SQLite's `PRAGMA quick_check` does not fail when it
  cannot read a page. It reports the page as a finding, *"unable to get the
  page. error code=N"*, followed by *"never used"* for every page behind it.
  quick_check reads every page, so it is the read most likely to meet a
  failing disk, and before this change its I/O failure was a `corrupt`
  verdict. A finding that names an unread page is now judged by `N`. The
  file's own names cannot forge the text: the schema is checked exactly before
  quick_check runs.
- **Scope item 2: the re-read is dropped.** `establish` no longer reads the
  rows after the snapshot's `ROLLBACK`. `inspect` has already judged them
  inside the snapshot. A migration reads back what it wrote inside its own
  write transaction, and nothing else follows. So every read `establish`
  judges runs inside the one snapshot (AC3), and the comment now says so.
  `load` still reads the rows after the store opens, and that read is
  classified the same way. It is not part of the judgement and never latches.
- A test-build hook, `detail::testing::classify_device_binding_read`, exposes
  the classifier for the codes a contract cannot make SQLite produce.

Scope item 4: `docs/architecture/github-host-implementation.md` states the
rule under BPR-0884's paragraph. BPR-0884 is Done and is not rewritten.

### Tests

`sqlite_device_binding_storage`, new `transient_read_failures`. Each fault
is injected into one real read of one store, counted, and then the **same
store** is called again:

| Fault | Where | Asserted |
| --- | --- | --- |
| `SQLITE_IOERR_READ` from the file | `PRAGMA quick_check` (a later read in `inspect()`) | `unavailable`; the same store then reads the row |
| `SQLITE_IOERR_READ` | `SELECT profile FROM binding_scope` (a later read in `inspect()`) | the same |
| `SQLITE_IOERR_READ` | `read_rows`' step in the judgement | the same |
| `SQLITE_IOERR_READ` | `read_rows`' step in `load`, after the judgement | the same |
| `SQLITE_NOMEM` | `read_rows`' prepare in the judgement | the same |
| Classifier pin | `IOERR`, `IOERR_READ`, `IOERR_SHORT_READ`, `IOERR_SHMMAP`, `NOMEM`, `PROTOCOL`, `BUSY`, `BUSY_RECOVERY`, `BUSY_SNAPSHOT`, `LOCKED`, `CANTOPEN`, `FULL`, `ERROR` | `unavailable` |
| Classifier pin | `CORRUPT`, `CORRUPT_INDEX`, `NOTADB`, `TOOBIG`, `INTERRUPT` | `corrupt` |
| Classifier pin | `INTERRUPT` or `IOERR_READ` with a stop requested | `cancelled` |

**Injection, portable (AC1).** An `sqlite3_auto_extension` wraps the store
connection's own main-file I/O methods (`SQLITE_FCNTL_FILE_POINTER`), so it
works over the default Unix VFS and Windows' `win32-longpath` alike. A
`SQLITE_TRACE_STMT` hook arms one failing `xRead` at the chosen run of the
chosen statement, after `sqlite3_db_release_memory` drops the pages the
connection has cached. Out-of-memory at a prepare comes from
`sqlite3_hard_heap_limit64`. It is armed when the statement before it
finishes (`SQLITE_TRACE_PROFILE`) and lifted, with the soft limit restored,
when the connection closes (`SQLITE_TRACE_CLOSE`). **Not injectable, pinned
at the classifier:** `PROTOCOL`, `BUSY_RECOVERY` and `BUSY_SNAPSHOT` come
from the WAL index when a transaction opens, while another connection holds
the index or is recovering it. Inside the judgement's snapshot no such read
happens. The re-read that was outside the snapshot, where they could arise,
is gone.

**AC2.** BPR-0884's corruption pins are unchanged and pass. An unknown
schema object is `corrupt`, and it stays latched after the file is repaired.
So do the oversized hostile field, the future version, the malformed legacy
row, and foreign scope or rows.

**AC4.** `test_device_binding_persistence_stays_drainable_when_its_posts_are_refused`
counts terminals per request. Each admitted request has exactly one, a
withdrawn one has none, and no earlier request gets a second one later.

### Negative controls (AC5)

- **The unchanged product** (`HEAD`'s store file, plus a shim for the test
  hook that answers `corrupt` for every code, which was that product's
  answer at a later read), with `expect` made non-fatal. **23 new
  assertions fail:** all five injected faults, each at `unavailable` and at
  "the same store reads the file on its next call", and all 13
  environment-code pins. The injection itself worked every time: no "the fault
  was injected once" assertion failed. The 5 verdict pins and the `cancelled`
  pin pass on both products. They pin AC2's unchanged verdicts, so no
  product change should make them fail.
- **The quick_check parser is load-bearing.** With the change in place but
  `unread_page` answering nothing, the quick_check fault fails at
  `unavailable, not corrupt` and at the next-call assertion.
- **The per-request counter.** The unchanged lane already gives exactly one
  terminal, so this assertion cannot fail on it. A trial lane that also
  delivers a second `unavailable` to the stranded successor fails the new
  test at "the successor reports unavailable exactly once", and passes the old
  test, which kept only the last result.

### Verification

| Check | Result |
| --- | --- |
| AC6 set, 6 rows | 6 of 6 pass: `application_github_host`, `sqlite_private_store_open`, `sqlite_device_binding_storage` (2.82 s), `application_device_binding_restart` (30.1 s), `_launch_renewal` (67.0 s), `_relaunch` (10.2 s). The storage contract passes again at the committed candidate, after a `<string_view>` include and two test-only tidy fixes |
| clang-format 23.1.1 | Clean on the four touched C++ files. Their HEAD versions were clean |
| `_coding-standard/scripts/tidy.py` | Nothing on a changed line in the store, the storage contract or the lane test. Two findings on new contract lines, a decrement inside a condition and an implicit `sqlite3_file**` conversion, were fixed before the commit. The host test file's four naming findings predate this change |

The code is platform-neutral, and so is the injection: it wraps whatever I/O
methods the connection's VFS has. One platform's run meets the acceptance, as
the card says. Windows and Linux confirm it in their next ordinary pass. On
Windows the injection runs over `win32-longpath`:

```sh
ctest --test-dir <build> --output-on-failure -R "^(sqlite_device_binding_storage|sqlite_private_store_open|application_github_host|application_device_binding_restart|application_device_binding_launch_renewal|application_device_binding_relaunch)$"
```
