---
schema: "implement-rfc.card.v1"
id: "BPR-0829"
title: "The anonymous public preview course's pass expression omits its epoch line history"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-verify new defect 1","found_at":"3b968fcc0027c774187c11489f4020c2463bcd2f","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0829: The anonymous public preview course's pass expression omits its epoch line history

## Defect

`application_anonymous_public_preview_public_path` fails on Linux, and the
station reports that a revert A/B test fails the same way. Its pass
expression (`tests/end_to_end/CMakeLists.txt`, ~2179) runs straight from
`PUBLIC_PREVIEW_HISTORY_INCOMPLETE:true` to
`PUBLIC_PREVIEW_SMART_ACQUISITIONS:1`. Since `26fd7f7c` (2026-09-12,
RFC-BPR-0029 composed epoch gaps), `src/app/main.cpp` has printed
`PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:<text>` between those two lines on every
run.

The station also saw `PUBLIC_PREVIEW_SMART_ACQUISITIONS` vary between 1 and 2
across Linux runs. A varying count is a separate question: either a real
duplicate acquisition, or a course that counts an operation whose number
depends on timing.

## Fix and scope

- Name the history line by shape (`PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:[^\r\n]*`).
- Do not loosen `SMART_ACQUISITIONS:1` until the variance is explained. If
  macOS repeats hold at 1, the Linux variance goes to the Linux station to
  diagnose, and this card records what it finds.

## Acceptance

- The course passes on macOS, repeated.
- The Linux station either passes it with `SMART_ACQUISITIONS:1` or names
  the cause of the second acquisition.

## Progress (dsp-002): history line named; the acquisition count is 2 on macOS too

The pass expression now names `PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:[^\r\n]*`.
With that, the one mismatch left on macOS is the count: two runs at the
integrated head print `PUBLIC_PREVIEW_SMART_ACQUISITIONS:2` and
`PUBLIC_PREVIEW_ENABLED_OK`, against the expression's `:1`
(`build/r2-logs/dsp2-int9-courses.log`, `dsp2-int9-repeat.log`).
The Linux station saw 1 and 2.

So the second acquisition is not Linux-specific. Whether it is a duplicate
acquisition in the product or a timing-dependent count in the course is the
open question. The expression keeps `:1`.

**Next.** Find which operations perform the smart acquisitions in this course
on macOS, by instrumentation or a bisect from `26fd7f7c`. Fix a duplicate in
the product, or state why two is correct and pin the course to a
deterministic count.

## Cause and fix (dsp-003)

**No duplicate acquisition: the course read a count that background work
was still changing.** Every lease goes through `++Acquisitions` in the
anonymous smart-Git factory (`src/app/src/github_application.cpp` ~1921).

- Lease 1 is the live epoch's pack (`src/application/src/epoch_application_host.cpp` ~5786).
- When the live epoch commits (~5575), `start_historical_import` retains each
  force-push occurrence that has no local epoch. The fixture timeline has
  three (events 91701, 91702 and 91703, `src/app/src/github_public_path_fixture.cpp`
  ~818-826). Each import takes its own lease (~6282), one at a time, and the
  next starts when one finishes (~6503). `docs/architecture/git-and-epoch-model.md`
  (~455) allows this.
- The course offered its refused writes on the tick the epoch settled. Those
  writes end the run, and shutdown cancels the import in flight. So the count
  read 1, 2 or 3, depending on how far the chain had got. Instrumented runs
  showed the cancel reaching the job. When the run was allowed to continue
  until every import finished, the count was 4 every time, and a fifth
  attempt found nothing left to acquire.
- This came in with `d1bcd1cf`, before `26fd7f7c`; that commit only added the
  history line. The Linux counts (2, 2, 1, 1, 2) fit the same race.

**Fix (course only, no product change).** In `src/app/main.cpp`, the plain
enabled course now waits until every "Event time" node on the epoch line is
retained, before it offers the writes. The verdict requires that wait. A run
that expires while waiting prints
`PUBLIC_PREVIEW_WAITING_FOR:retained force-push occurrences`. The historical,
empty-timeline, rewritten-base and enterprise variants skip the wait, and so
the script-driven epoch courses, which always pass one of those flags, are
unaffected. The registration expects `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4`,
with a comment naming the four leases. It uses the epoch phase budget
(120 s, TIMEOUT 170), because the three imports take about 7.5 s each.

**macOS evidence.** Logs are in `build/r2-logs/`.

- Before the fix, the unchanged course printed 2, 1 and 2
  (`dsp3-0829-baseline.log`). Instrumented series printed 1 or 2
  (`dsp3-0829-instrumented*.log`). Waiting for only the first retained head
  gave 3, 2, 2, 3 and 3 (`dsp3-0829-fix-instrumented.log`).
- In the slice worktree, the course passed 5 of 5 at load 3 to 4
  (`dsp3-0829-final2.log`).
- At main `5a2e264a` with the change, the course passed 3 of 3 (32.7 to
  33.0 s; `dsp3-0829-int-course-{1,2,3}.log`). In the same build these also
  passed: `test_network_isolation_manifest`, `architecture_boundaries`,
  `documentation_structure`, `application_epoch_persistence_github-com`,
  `application_epoch_maintenance_public_path`,
  `application_epoch_ordinary_update_recreated` and
  `application_epoch_portable_occurrence_two_catalogues`
  (`dsp3-0829-int-regress.log`).

**Follow-ups:** [[BPR-0838]] (the counter is read across threads without
synchronization) and [[BPR-0839]] (two of the imports refetch the live head
with no haves; if that changes, this course's `:4` changes with it).

**Still open:** the Linux rerun of the new course, and the round-trip
contract's reseal after an independent review of this `main.cpp` change.
