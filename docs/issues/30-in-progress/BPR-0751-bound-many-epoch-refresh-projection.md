---
schema: "implement-rfc.card.v1"
id: "BPR-0751"
title: "Bound many-epoch refresh projection on the public path"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Bound many-epoch refresh projection on the public path

BPR-0635's 32-head synthetic Mac course passes exact provider reads, source,
Git/catalog identity and cache-byte eviction, but the existing history
regression fails its unchanged 100-ms UI timer-interval limit on every refresh:
237–255 ms. One source selection takes 2.152s against its 2s limit. The
current-source 25s/5ms sample in `build/bpr0635-post-lock-sample.txt` places
the main-thread work in accepted pull-request handoff, workbench observation
and publication, and Qt conversation projection; historical fixture script
compilation appears on a worker thread. BPR-0635's first fixture prerequisite
failed before timing, so whether the product projection delay predates that
candidate remains unproven. The failed run is
`build/bpr0635-composed-final.log`, test 279. This card splits the wider
product-path diagnosis from the synthetic byte-pressure fixture/course.

## Scope

- Compare an unchanged baseline and the BPR-0635 candidate under the same
  32-head synthetic, network-isolated Mac history course. Attribute the
  refresh and source-selection intervals before changing product behavior.
- If product publication is responsible, bound main-thread handoff, workbench
  and conversation projection work while preserving exact selected-file,
  semantic-change, comment and stale-result behavior. Keep core decisions in
  core, effects in application, and Qt presentation in the adapter.
- Do not lower the existing 100-ms UI interval, 12s refresh or 2s
  source-selection limits to obtain a pass. No real pull-request mutation,
  production credentials or remote service use.

## Acceptance

- The baseline/candidate comparison and measured main-thread attribution
  identify the cause of the 237–255-ms refresh pauses and the isolated
  source-selection overrun, including whether either predates BPR-0635.
- The network-isolated 32-head public history course passes its existing Mac
  timing limits with all exact heads, 929 account reads, source selections,
  supersession and retained Git/catalog pairs intact.
- Focused host/Qt/composed regressions, documentation gate and diff checks
  pass. Record hardware and remaining native-platform limits; no universal
  responsiveness claim follows from one Mac run.

The authored plan groups this repair with BPR-0635's still-open preserved
history check. The prior BPR-0635 binding and its failed preservation check
remain historical; acceptance requires a renewed binding for both cards.
