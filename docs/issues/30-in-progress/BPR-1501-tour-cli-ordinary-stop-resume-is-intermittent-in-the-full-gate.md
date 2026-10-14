---
schema: "implement-rfc.card.v1"
id: "BPR-1501"
title: "Tour CLI ordinary stop's resume is intermittent in the full gate"
status: "In Progress"
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

## Evidence

Cause (AC1): a product ordering race in
`review_tour_publication_workflow::observe` (`src/app/src/review_submission_workflow.cpp`),
not a course defect. `recording-failure=15` (`recording_exhausted`) appears in
passing runs too and is not the cause.

- In the CLI course the restarted app reconnects through `gh`, so its host
  authority fingerprint (which includes the profile revision) differs from
  the first run's: the restored window's tour owner differs and it loads an
  empty draft (revision 1, no stops). The pending publication is still
  discovered from the recovery store with the original captured plan
  (generation 7, original owner).
- `observe` compared every draft projection with the held plan. When the
  draft projection arrived before discovery (the usual order), no plan was
  held and nothing happened. When contention delayed it until after discovery
  it cancelled the restart reconciliation (status "Publication cancelled",
  failing "restarted real app reconciles pending review before Resume"). And
  when an unchanged re-projection arrived after the post-publication refresh,
  it overwrote "Published:" with "Draft changed. Prepare a fresh preview..."
  (the gate's "Resume completes the original pending review"). The same inputs
  gave different outcomes depending on arrival order.
- Temporary stderr instrumentation (removed) recorded, for each failing run,
  `observe plan-gen=7 draft-rev=1 owner-eq=0` (owner differing only in
  `HostAuthority`) arriving after discovery or after the final `published`
  result. It also recorded handoff authorities `f5e4319e…` (first run) and
  `4f5f532b…` (restart).

Fix (AC2): `observe` now treats only a change of the observed draft (owner or
revision differing from the previous projection) as making the plan stale. A
window's first loaded draft and repeated projections of the same revision no
longer cancel or overwrite, in either order. Publish's dispatch-time checks
(plan generation versus draft revision when there is no record) are unchanged.
No check, budget or course step was loosened. The course is unchanged.

Reproduction: the row run via `ctest -R '^application_review_tour_cli_ordinary_stop$'`,
with load added by N `yes > /dev/null` processes (PIDs recorded, killed by
PID) on a 14-core Mac whose ambient load average was already about 21 to 24.

| Build | Extra load | Runs | Failures |
|---|---|---|---|
| before | 0 (ambient about 21) | 2 | 2 |
| before | 14 `yes` | 10 | 1 |
| before (instrumented) | 14 `yes` | 15 | 3 |
| before (instrumented) | 14 `yes` | 12 | 5 |
| after | 14 `yes` | 30 | 0 |
| after | 28 `yes` | 20 | 0 |

Before: 11 failures in 39 runs. After: 0 in 50. After the fix the other ten
`review_tour_publication_public_path` rows, `application_review_tour_cold_import_public_path`
and `application_review_submission_cli_ordinary` each passed once.
`src/app` changed, so the release seal must be regenerated.
