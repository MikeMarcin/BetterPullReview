---
schema: "implement-rfc.card.v1"
id: "BPR-1501"
title: "Tour CLI ordinary stop's resume is intermittent in the full gate"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1500--BPR-1501--7193b24a1ace.json","origin":"0.2.0 release normal gate on main ca825e18f, owner release request 2026-10-13"}
---

# Tour CLI ordinary stop's resume is intermittent in the full gate

`application_review_tour_cli_ordinary_stop` failed in the 0.2.0 full gate on
ca825e18f after 23.1 s:

```text
read operation=0 count=92 recording-failure=15
review-tour-publication:fail Resume completes the original pending review
```

It passed in the gate on cd4860e30 and 5 of 5 alone (about 3.1 s each).

## Acceptance

- AC1: The failure is reproduced (under contention if needed) and its cause
  named: product race or course defect.
- AC2: The fix removes it without loosening what the course checks, and the
  row passes repeatedly under contention.

## Boundaries

No live GitHub mutation; fixtures only.
