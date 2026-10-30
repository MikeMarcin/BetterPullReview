---
schema: "implement-rfc.card.v1"
id: "BPR-1833"
title: "Diagnose and restore the Start Review finish transition"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"release-v041-critical-main-restart-r2","release_issue":"BPR-1811","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","allocation_commit":"f737404e1c42c5ff532064bfdabfeeb0c97c5e15","published_source":"4dd4cd72d3d716329f286592d7f8efaa873de95a"}
---

# Diagnose and restore the Start Review finish transition

Build 7 Release qualification fails `application_start_review_contract` on Mac
and Windows. The first unchanged quiet Mac replay repeats the failure under
the original 40,000 ms application budget. Mac waits at the finish-page
predicate; Windows reaches the submitted phase with one post, zero published
comments and two held comments. Those observations do not establish one cause.

The root coordinator allocated this ID on canonical main and authorized an
isolated repair based on exact build 7. Import only its allocation metadata;
later canonical runtime is outside this release mandate. Published build 7
source, assets and every original receipt remain immutable.

## Acceptance

- AC1: Preserve both native failures and the unchanged quiet replay, identify
  the exact awaited predicates and distinguish fixture assumptions from runtime
  behavior using bounded direct evidence.
- AC2: Correct the confirmed cause on its narrow input surface. Preserve the
  original Release deadline, public gestures, functional assertions, provider
  boundaries and meaningful negative controls.
- AC3: Verify the corrected public composition on the affected native inputs,
  including later finish/refusal/uncertain/recovery predicates. Bind actual
  compiler, image, source and unchanged-input evidence without relabeling old
  images or claiming unreached checks passed.
- AC4: Complete focused format, documentation and relevant boundary checks;
  hand the exact candidate and remaining release obligations to BPR-1811.
  Unstarted build-7 Debug/Ubuntu profiles and seven Mac resource rows stay
  deferred until the repaired candidate is defined. The already-running
  Windows Release attempt continues for its raw failure inventory.

## Preserved evidence

The external state contains `successor-mac-release-tail-functional-r2`,
`successor-mac-start-review-quiet-r1` and
`successor-windows-start-review-failure-r1.log`. Timing, raw exits, unreached
predicates and supervision/resource incidents retain their original meaning.
