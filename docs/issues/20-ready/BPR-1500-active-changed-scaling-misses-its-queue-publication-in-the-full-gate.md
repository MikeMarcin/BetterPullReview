---
schema: "implement-rfc.card.v1"
id: "BPR-1500"
title: "Active changed scaling misses its queue publication in the full gate"
status: "Ready"
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
