---
schema: "implement-rfc.card.v1"
id: "BPR-1104"
title: "Windows: the review-restoration course still stops at EPOCH_CACHE_FAILURE:7 (revision-prepare, spool_failure) after BPR-1040's retry"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1050's 32-row table","found_at":"bcd57840","related_cards":["BPR-1040","BPR-1050","BPR-0876"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
acceptance: [{"criterion":"AC1 The failing operation and its Windows error are named with evidence from a reproduced failure","outcome":"pass","revision":"51784082","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, a station loop running the course's auth-seed phase exactly as the course does, with reverted traces. (1) At 7f347d65 all 5 of 43 failures (EPOCH_CACHE_FAILURE:7) were remove_exact_private_file's delete of the quarantined spool after staging: error=32 ERROR_SHARING_VIOLATION, the same delete succeeding 50-250 ms later. (2) With that delete retried, 3 of 80 stopped at EPOCH_CACHE_FAILURE:10 / revision-prepare:6: private_input_file::open's exclusive open gave up with error 32; an open sharing reads succeeded; the Restart Manager named the holder as this process's own git.exe child (parent_is_self=1) running index-pack --stdin for another operation's repository, still holding the spool 10 s later: an inherited handle, since unguarded Windows Git launches used Qt's default bInheritHandles with no handle list and MSVC C runtime streams are inheritable.","path":"docs/issues/60-done/BPR-1104-windows-the-review-restoration-course-still-stops-at-a-spool-failure-after-bpr-1040.md#named-twice-fixed-at-the-launch-and-at-the-delete-bpr-main-win-2026-09-24"}},{"criterion":"AC2 The failure rate is measured before and after the fix over the same runs, and the fix brings it to zero in N runs large enough to have shown it before","outcome":"pass","revision":"51784082","evidence":{"kind":"command","detail":"Same 17-second auth-seed loop: as it was 5 of 43 failed (11.6%); with both parts as committed (standard_handles_only handle list for unguarded Git, remove_file_name bounded delete retry) 0 of 80. The full application_profile_window_review_restoration_public_path passes 2 of 2 (366.23 s, 371.38 s) against 1 epoch stop in 4 runs at bcd57840. git_process_contract, git_cache_contract, git_sparse_cache_contract, git_protocol_boundary, native_git_smart_transport_contract, application_git_protocol_v2, architecture_boundaries, architecture_boundary_contract and BPR-1040's three AC2 courses pass; clang-format clean; clang-tidy adds no finding.","path":"docs/issues/60-done/BPR-1104-windows-the-review-restoration-course-still-stops-at-a-spool-failure-after-bpr-1040.md#named-twice-fixed-at-the-launch-and-at-the-delete-bpr-main-win-2026-09-24"}},{"criterion":"AC3 Negative control: the same build and runs without the fix fail at the operation AC1 names","outcome":"pass","revision":"51784082","evidence":{"kind":"command","detail":"Same loop: without either part 5 of 43 failed at the delete; with the delete retry but no handle list 3 of 80 failed at the exclusive open (inherited holder); with the handle list but no delete retry 2 of 80 failed at the delete. Each part alone leaves the failure the other removes.","path":"docs/issues/60-done/BPR-1104-windows-the-review-restoration-course-still-stops-at-a-spool-failure-after-bpr-1040.md#named-twice-fixed-at-the-launch-and-at-the-delete-bpr-main-win-2026-09-24"}},{"criterion":"AC4 macOS and Linux keep their paths, or any shared change is run there","outcome":"pass","revision":"51784082","evidence":{"kind":"freeform","detail":"Every behavioural change is inside Q_OS_WIN: standard_handles_only, the refusal of an unguarded start without it, and the delete's retry. On macOS and Linux remove_file_name makes the same std::filesystem::remove call the removal made inline before, and the Git launch is untouched."}}]
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

## Post-close: Linux at `e2dc8f82` (bpr-main-linux, 2026-09-24)

**The Git rows pass on Linux, 4 of 4, with both parts of the fix in the
tree.** This adds a Linux run to AC4's reasoning above. The Windows-only
handle list and the Windows-only delete retry changed nothing these rows
observe off Windows. This station did not run macOS.

The integrator asked for this rerun. It was made in the station's main
checkout, with a clean tree, at `e2dc8f82` ("Finish BPR-1104"). That revision
contains `51784082`. The station is WSL2 Ubuntu 26.04 with GCC 15.2 and Qt
6.11.2. The tree is `build/dev` (Debug) from the `dev` preset, which compiles
with `-Werror`. The build of `e2dc8f82` recompiled `installed_git_process.cpp`
and `installed_git_cache.cpp` and has no compiler `warning:` line. So the
change's code outside `Q_OS_WIN`, which is `remove_file_name`'s plain
`std::filesystem::remove` branch, compiles here with warnings as errors. The
rows ran once each, serially (`-j1`), with `QT_QPA_PLATFORM=offscreen` and
with `DISPLAY` and `WAYLAND_DISPLAY` unset. They followed [[BPR-1102]]'s
Linux rows in the same session, which started at a load average of
7.97/2.09/0.70.

`ctest --test-dir build/dev -j1 --output-on-failure -R "git_(process|protocol|cache)|installed_git"`
passed 4 of 4:

| Row | Time |
| --- | --- |
| `git_protocol_boundary` | 0.51 s |
| `application_git_protocol_v2` | 0.01 s |
| `git_process_contract` | 5.73 s |
| `git_cache_contract` | 4.67 s |

`git_process_contract` compiles `installed_git_process.cpp` into its own
executable. `git_cache_contract` compiles that file and
`installed_git_cache.cpp`.

No row matched `installed_git` by that name. No row registered in `build/dev`
has that string in its name, because it is the prefix of the two changed
source files. The selector therefore ran exactly the four rows above. The
card's other Windows checks were not run here: `git_sparse_cache_contract`,
`native_git_smart_transport_contract`, the architecture rows, BPR-1040's
three courses and the review-restoration course.

The logs are station records under
`build/logs/bpr-main-linux-20260924/bpr-1102-1104-post-close/`
(`bpr-1104-post.log`, and `build-dev-e2dc8f82.log` for the build). They are
not committed.

## Post-close macOS rerun (2026-09-24, integrator)

At `e2dc8f82` on macOS 26.6.2 arm64 (`build/dev` Debug, incremental rebuild
clean), `ctest --preset dev -j1 -R "git_(process|protocol|cache)|installed_git"`:
`git_protocol_boundary` 0.40 s, `application_git_protocol_v2` 0.40 s,
`git_process_contract` 94.17 s and `git_cache_contract` 262.91 s, all passed.
The Windows-only handle restriction changes nothing observable on macOS,
confirming AC4's code-judged macOS half by a run. The recommended
independent review of the Windows inheritance change remains the owner's
decision.

## Post-close owner decision, 2026-09-24: yes, an independent critical review (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-1104-independent-review-v001`, revision `51784082`,
fingerprint `d557512c…`, decision `ae582fb8-d480-456a-8ef5-79011030e131`, sequence 586,
2026-09-24T19:17:18Z, `actor: human`: **select**, verbatim:

> Yes: run an independent read-only review of 51784082

Note, verbatim:

> Yes use rfc-reviewer-critical to double check the work

An `rfc-reviewer-critical`, dispatched by the coordinator and not by the lane
that wrote the change, reviews `51784082` read-only and reports its findings. It
changes no code, board state or GitHub. The card stays Done. A finding that
needs work gets its own card, and the review's result is recorded here.
