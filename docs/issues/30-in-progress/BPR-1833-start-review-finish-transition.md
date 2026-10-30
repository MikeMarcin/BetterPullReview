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

## Diagnosis and correction

The unchanged quiet Mac replay repeats the original 40-second expiry. A private
logging-only native probe shows every finish-page predicate true, the Comment
click and event processing returning in seven milliseconds, and the next entry
at submitted phase with the inline mutation host absent. BPR-1816 deliberately
defaults a confirmed review to closing the pull request and returning Home;
its accepted card requires older multi-submit courses to select their stay
setting. The original Start Review finish/again setup omitted that gesture.
The Windows phase-16 observation is consistent with the different ordering of
confirmation and the deferred close; it does not establish a provider stall.

Only those two fixture modes now choose Comment's existing stay-on-pull-request
checkbox once, wait for its accepted projection, and require Approve and Request
changes to retain their Home defaults. All original exact-body, idempotence,
refusal, unknown-answer, recovery and later reread assertions and the 40-second
deadline remain. Both additions are under `BPR_BUILD_TESTING`; production finish
policy is unchanged. Full-application and app-content pins bind these reviewed
bytes; the production mutation-branch digest and source inventory are unchanged.

The complete seven-subcourse Start Review chain passes on the private native
Mac Release closure. The existing default-Home/Comment-only opt-out public course
also passes on that exact private archive. A first attempt at the latter was
interrupted by a foreign-process presence guard and retains no course credit;
the remaining-only functional replay preserves it. The 2,302 protected inputs
and five private outputs match their pre/post seals; no retained image is
relabeled. Windows/Ubuntu focused proofs and final acceptance remain pending.

The unrelated Windows Release benchmark's original 100-ms input-target failure
is retained separately from its unchanged focused replay, which passes with all
three fresh tooling prerequisites. No timing waiver or load cause is inferred.

## Operator observations retained outside source

The complete Windows Release inventory finishes 859 passed and two failed out
of 861, with no missing rows. Its Start Review failure and benchmark failure
remain raw. Private Windows command-preparation refusals ran no compile/course.
The subsequent exact MSVC compilation exits zero, while its strict supervisor
reports and cleans one owned lingering `vctip.exe`; the outer failure and later
object/input adoption are separate from native public-path acceptance.

An unselected local composition-driver invocation inadvertently entered a
tests-off configure. Only its exact owned root and descendants were stopped;
the raw interrupt and unadmitted telemetry gap remain. No full composition or
build credit is taken. The guarded explicit source-inventory/hostile method
then passes. Further composition-driver invocations require a named selector.
