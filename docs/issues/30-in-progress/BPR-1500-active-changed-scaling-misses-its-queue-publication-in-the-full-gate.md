---
schema: "implement-rfc.card.v1"
id: "BPR-1500"
title: "Active changed scaling misses its queue publication in the full gate"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1500--BPR-1501--7193b24a1ace.json","origin":"0.2.0 release normal gate on main ca825e18f, owner release request 2026-10-13"}
---

# Active changed scaling misses its queue publication in the full gate

`application_active_changed_scaling` failed in both 0.2.0 full gates (on
cd4860e30 and on ca825e18f) after about 10 s:

```text
queue_public_changed=1 item_publications=0 catalogue_visits=0 repository_full_builds=0
FAILED: public refresh publishes one queue item and rebuilds no retained queue or repository catalogue
```

Alone it passed 5 of 5 (about 17 s each, load about 7). The full gate runs
four independent tests beside the serial Qt and end-to-end courses
(`scripts/run_test_suite.py`), so the course sees CPU contention.

## Acceptance

- AC1: The failure is reproduced under contention, and its cause is named: a
  product race or a course that reads before the publication it waits for.
- AC2: The fix removes the race (or makes the course wait on the publication
  itself) without loosening what it checks, and the row passes repeatedly
  under contention like the gate's.

## Boundaries

No budget is loosened and no case is removed.

## Evidence

Reproduced on `main` 43042d0c0 (macOS, 14 cores), with 14 `yes > /dev/null`
processes started and then stopped by PID. Load averages were 22 to 26:
1 run in 3 failed after 10.9 s with the gate's exact
`queue_public_changed=1 item_publications=0 ...` failure.

Cause (AC1): the course read the count before the publication it was checking.
The product is correct, so this is not a product race. Follow enrolls the pull
request with only its title as facts. The public refresh then brings the full
facts, so `review_queue_application_host::finish_refresh` counts
`AppliedRefreshes` and takes the store path (`observe_facts` changed, so
`persist_record`). The single item is published only by
`finish_store_enrollment`, after the store answers on the effects lane. The
course waited only for `AppliedRefreshes` and a ready status and epoch. Alone,
the store finished before those were ready. Under contention it did not.
The new diagnostic line shows the path: `applied_refreshes=1
persistence_requests=1 item_publications=1`.

Fix (AC2): the fixture access's `application_scaling_evidence` now carries
`QueueQuiescent`, the queue host's existing `is_quiescent()`. The course also
waits for that, so it reads after the store answers. The check is unchanged:
exactly one item publication, no catalogue visits, no repository full builds.

The first fixed batch turned up a second contention failure in the same row,
in 1 run of 8: "the held gesture is durable, then published, once the store
answers". It failed 14 s into the run, well inside `wait_for`'s 30 s budget.
So the loop saw its condition true, and the separate reading in `expect` then
saw it false. `quiet()` reads the epoch host's `job_count()`, which worker
threads change under a mutex, and no event is pumped between the two readings.
`wait_for` now decides on the sample that ended the loop. If a second reading
differs, it logs `wait_condition_moved_after_met`. No condition and no budget
changed. The cause is inferred from the timing; the new line did not fire in
the runs below.

After, under the same 14-process load (load averages 22 rising to 58, with
other agents' builds also running): 10 of 10 passed, taking 22.9 to 30.5 s.
Without my added load: 3 of 3 passed, about 22.6 to 23.4 s, with load averages
49 to 62 from other agents.

`src/app` changed (`github_application.cpp` and
`github_application_fixture_access.hpp`, test-only fixture access), so the
round-trip contract needs resealing.
