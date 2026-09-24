---
schema: "implement-rfc.card.v1"
id: "BPR-1104"
title: "Windows: the review-restoration course still stops at EPOCH_CACHE_FAILURE:7 (revision-prepare, spool_failure) after BPR-1040's retry"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1050's 32-row table","found_at":"bcd57840","related_cards":["BPR-1040","BPR-1050","BPR-0876"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
---

# BPR-1104: Windows: the review-restoration course still stops at EPOCH_CACHE_FAILURE:7 (revision-prepare, spool_failure) after BPR-1040's retry

## Defect

[[BPR-1040]] named three operations on a just-written pack spool that
Windows briefly refuses, and retried them, bounded. It left one
`spool_failure` in eight review-restoration runs unexplained. The failure is
still there. In [[BPR-1050]]'s 32-row table at `bcd57840`,
`application_profile_window_review_restoration_public_path` stopped at its
16th phase, the `unavailable-file` course's `auth-seed`:

```text
auth-seed failed (1)
EPOCH_CACHE_FAILURE:7:
EPOCH_EFFECT_FAILED:revision-prepare:0
FAILED: reading epoch ready
```

`7` is `git_cache_failure::spool_failure`. The 15 phases before it passed.
The course then passed on its own (365.48 s) and in the sweep (366.21 s), so
it is intermittent. The logs are station records under
`build/logs/bpr-1050-main-win/` and are not committed.

## Scope

- Measure a rate over N runs.
- Name the operation that fails and its Windows error, from a reproduced
  failure.
- Fix it in the layer that owns it, bounded and named, as BPR-1040 did.
- Show the rate drop over the same kind of runs.

## Acceptance

- AC1 The failing operation and its Windows error are named with evidence
  from a reproduced failure.
- AC2 The failure rate is measured before and after the fix over the same
  runs, and the fix brings it to zero in N runs large enough to have shown it
  before.
- AC3 Negative control: the same build and runs without the fix fail at the
  operation AC1 names.
- AC4 macOS and Linux keep their paths, or any shared change is run there.

## Named twice, fixed at the launch and at the delete (bpr-main-win, 2026-09-24)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2), at
`7f347d65` plus this change. The logs are station records under
`build/logs/bpr-1104-main-win/` and are not committed.

**How it was measured.** The failing phase is `auth-seed`, which builds a
fresh epoch in a new data root. A station loop (not committed) runs that phase
exactly as `profile_window_review_restoration_contract.py` does: a new
owner-private root per run under a contained temporary directory, the same
arguments and environment as the registered row, one run at a time, standard
error kept and standard output never read. One seed takes about 17 s, so this
gives a rate per epoch-building phase far faster than the 366-second course.
Reverted traces named each failure.

**First cause: the quarantined spool's delete (AC1).** At `7f347d65`, 5 of 43
seeds stopped at `EPOCH_CACHE_FAILURE:7`, `revision-prepare:0`,
`FAILED: reading epoch ready`: the card's failure. Every one was the same
step. After staging, `remove_exact_private_file` renames the spool to a
quarantine name, which [[BPR-1040]] retries, and then deletes it, which it
did not:

```text
BPR1040_REMOVE delete removed=0 error=32 The process cannot access the file because it is being used by another process. missing=0
BPR1040_REMOVE delete-after-50ms removed=1 error=0 missing=1
BPR1040_SPOOL site=4149
EPOCH_CACHE_FAILURE:7:
```

`error=32` is `ERROR_SHARING_VIOLATION`. The same delete succeeded 50 ms
later in four cases and 250 ms later in the fifth.

**Second cause: a Git child that inherited a stream on another spool (AC1).**
With only the delete retried, 0 of 80 seeds stopped at the delete, but 3 of 80
stopped at `EPOCH_CACHE_FAILURE:10` (`process_failure`) and
`revision-prepare:6`, 1.31 s after the stage before it. That is BPR-1040's
third shape: `private_input_file::open`, which opens the spool with no sharing
to feed Git and retries for about 1.3 s. A trace at that open named the
holder:

```text
BPR1104_OPEN gave-up error=32 path=...\incoming\v1\abae68f0....pack.part
BPR1104_OPEN share-read-open=1 error=0
BPR1104_HOLDERS result=0 count=1 needed=1
BPR1104_HOLDER pid=18044 self=0 app=Git for Windows service=
BPR1104_HOLDER_PARENT pid=18044 parent=45600 parent_is_self=1
BPR1104_HOLDER_CMD pid=18044 cmd="...\git.exe" ... --git-dir=...\repositories\v1\662a63ef....git index-pack --stdin ...
```

The holder was this process's own child: the `index-pack` of a different
operation, holding this operation's spool. An open that shared reading
succeeded, so the held handle only read, and it was still held 10 s later.
`index-pack --stdin` reads its pack from a pipe and never opens a spool by
name, so the handle can only have been inherited. Every Unix Git launch sets
`CloseFileDescriptors`, and a guarded Windows launch goes through the
supervisor, whose Git inherits exactly its three standard handles. An
unguarded Windows launch, which includes every cache operation, was left to
Qt's default: `bInheritHandles` with no list, so Git inherited every
inheritable handle the process held at that instant. MSVC's C runtime makes
`std::ifstream` inheritable by default, and two readers open spools that way:
the adapter's own `equal_private_files`, just before it removes a spool, and,
in this testing build, the fixture's durable-copy scan
(`device_durable_copies`), which reads every file under the data root each
time the course asks for its evidence. A trace of the adapter's streams shows
none opened the held spool before the failure, which leaves the scan.

**The fix, in the Git adapter.** Both parts are Windows only.

- `standard_handles_only` (`installed_git_process.cpp`) gives every unguarded
  Git launch a `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` of its three standard
  handles, the Windows counterpart of `CloseFileDescriptors` and the rule the
  supervisor already applies. It fails closed: a start that bypassed the list
  inherits nothing and is refused as `process_start`.
- `remove_file_name` (`installed_git_cache.cpp`) puts the quarantined spool's
  delete under the same bounded `retry_while_held` as BPR-1040's three
  operations. A delete that still fails after about 1.3 s fails the removal
  exactly as before.

**The rate (AC2, AC3).** Each row is the same loop, 17 s a seed:

| Build | Seeds | `spool_failure` (7) | `process_failure` (10) | Failed |
| --- | ---: | ---: | ---: | ---: |
| `7f347d65`, as it was (with a reverted trace) | 43 | 5 | 0 | 11.6 % |
| Delete retry only (trace) | 80 | 0 | 3 | 3.8 % |
| Handle list only | 80 | 2 | 0 | 2.5 % |
| Both, as committed | 80 | 0 | 0 | 0 % |

The first three rows are the negative controls: each part alone leaves the
failure the other one removes, and the build without either fails at the
operation named above.

The course itself stopped at the epoch in 1 of the 4 runs this station
made at `bcd57840` (BPR-1050's table run; the three others passed). On this
candidate it passes 2 of 2, in 366.23 s and 371.38 s, and prints only the two
`EPOCH_CACHE_FAILURE:3` lines its deliberately unavailable phases printed
before.

**Checks.** On this candidate, serial: `git_process_contract` (46.95 s),
`git_cache_contract` (72.98 s), `git_sparse_cache_contract` (26.00 s),
`git_protocol_boundary`, `native_git_smart_transport_contract`,
`application_git_protocol_v2`, `architecture_boundaries` and
`architecture_boundary_contract` pass, and so do the three courses of
BPR-1040's AC2: release maintenance (14.22 s), attended recent replay
(25.99 s) and profile multi-window (35.54 s). The full `build/dev` tree builds
without a warning. `clang-format` is clean. `clang-tidy` reports nothing on the
new code; its five findings in `installed_git_process.cpp` and two in
`installed_git_cache.cpp` predate this change.

**AC4.** Every change in behaviour is inside `Q_OS_WIN`: the handle list,
the refusal of a start without it, and the delete's retry. On macOS and Linux
`remove_file_name` makes the same `std::filesystem::remove` call the removal
made inline before, and the launch is untouched.

**Not changed here.** The C runtime streams stay inheritable; with the list
in place no Git child can take one. The fixture scan is a testing path and is
left as it is. [[BPR-1040]]'s account of its holder, a real-time scanner, is
not rewritten. Some of its holds may have been this inheritance. With the list
alone, 2 of 80 seeds still stopped at the delete; they were not traced, and
the delete's retry covers them.
