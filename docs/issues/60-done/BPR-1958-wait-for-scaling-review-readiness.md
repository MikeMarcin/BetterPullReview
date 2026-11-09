---
schema: "implement-rfc.card.v1"
id: "BPR-1958"
title: "Wait for the scaling course's composed Review readiness"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"test","found_by":"Rejected v0.4.2 build43 native Windows qualification","mandate":"Owner release-blocker fixes land through an implementer on main; coordinator scoped 2026-11-09 readiness correction to application_scaling_tests.cpp only.","milestone_plan":{"id":"bpr-1958-scaling-readiness","path":"docs/rfcs/plans/bpr-1958-scaling-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e5e7da456445d24fd718f0d730138aafec2dd3de","evidence":{"kind":"command","detail":"Exact test file dc422e... adds shared Review/required-pane completion and exact current-row wait. Native corrected full515file/1000thread course PASS26.52s/original180s; all semantic/allocation/workload/network oracles unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"e5e7da456445d24fd718f0d730138aafec2dd3de","evidence":{"kind":"artifact","detail":"Frozen97db+onlydc422e test overlay compiled under normal MSVC definitions;201 protected inputs unchanged. Corrected image726ed76d PASS; separate disabled-Review imagef1f95c7f expectednamedOverview/Review refusalCTest8/14.06s. QuietoldbaselinePASS24.62s, original failures and operator guard refusals retained; no fullrelease acceptance."}},{"criterion":"AC3","outcome":"pass","revision":"b3a45f3a292fb847dd97f5822d7bcf6ecfb945ee","evidence":{"kind":"command","detail":"Source merged and pushed canonical+origin/main b3a45f3a2; format22, DOCUMENTATION_OK,67documentationtests and scopedboardvalid. No other source path changed by this issue; source43/cache protected, full qualification still pending."}}]
---

# BPR-1958: Wait for scaling Review readiness

## Outcome and acceptance

- AC1. The scaling public course enters Review through its existing shared
  modality helper and waits for the exact file's current projection before
  selecting it. Workloads, measured gestures, allocation and semantic oracles,
  network boundary, timing policy and original watchdogs remain intact.
- AC2. Retain the original Windows failures and distinguish a meaningful
  negative control from corrected composed evidence. Bind native evidence to
  exact source, image, arguments and context; no final release acceptance is
  supplied by this engineering course.
- AC3. Focused verification, formatting and documentation checks pass, and
  the correction lands on main. Preserve all unrelated rejected-build failures.

## Scope

Only `tests/end_to_end/application_scaling_tests.cpp` changes product-test
behavior. The rejected frozen43 producer and its images remain immutable.
The nine Debug rows that could not start have a separate damaged-image
cause; startup, other Review readiness, notification and optimized timing
failures remain separate qualification obligations.

The Windows course immediately looked up `src/file_0031.cpp` after triggering
Review, before delivering the host's pending projection. Build43 Release
originally stopped during startup; Debug stopped at `actual file row exists`.
A serial engineering repetition reached that same file-row assertion in
Release. Its Release portion overlapped an accidentally surviving diagnostic
receipt query and supplies no quiet-machine timing evidence. The diagnostic
process was stopped by exact owned PID, creation and script identity.

## Engineering and main integration

Source `e5e7da456` changes this course only: the shared Review step waits for
host and window agreement and the required panes, and each exact file row is
looked up after its current projection arrives. The 515-file, 1,000-thread,
20,000-line fixture, structural/allocation oracles, selected-material checks,
network boundary and original 180-second watchdog remain unchanged.

The source is on authoritative main and origin/main through
`b3a45f3a292fb847dd97f5822d7bcf6ecfb945ee`. Native engineering evidence binds the
exact test-file SHA to a full frozen43 source export plus that file, compiled
with the generated MSVC project. Only owned source/output paths and equivalent
absolute linker-input spellings change; producer build edges are omitted.
All 190 linked input files and the other runtime/control inputs (201 protected
files in total) retain their hashes. The original producer remains source
`97db680a3337f69e578fedab95825e84e3c72a33`, clean.

- Windows 11 x64, Qt 6.12.0, offscreen, Limited Interactive session 3: the
  corrected full course passes in 26.52 seconds with every original oracle.
- The quiet unchanged baseline passes in 24.62 seconds. The earlier failure
  is intermittent; this is not a quiet before/after performance comparison.
- Disabling the public Review action in a separate negative variant fails at
  `COURSE_MODALITY_STALLED`, with both host and window still on Overview,
  followed by the exact required-Review failure (14.06 seconds). Later checks
  in this intentional failure receive no coverage credit.
- clang-format 22, documentation (`DOCUMENTATION_OK`), the 67 documentation
  contract tests and scoped board validation pass. No Mac native build or
  GUI course was run by this implementer.

Exact images, hashes, original properties, command results and streams stay
outside source under the release run's
`candidate43/windows-native-diagnosis-r1/scaling-engineering-r1` through `r3`.
The intermediate operator guards rejected a copied-project metadata change
before compilation and an owned-path slash-spelling difference before
execution. Both are retained; neither is counted as a passing product run.
All native command forests are empty, all eight task-owned scheduled tasks
were removed, and the failed negative control's owned temporary root was
removed after identity and Git-link checks.

The separate damaged-image recovery is recorded in the checkpoint. It repairs
five cached Debug images and restores nine originally unstarted courses; it
is not a product source fix or final release acceptance. Full current-main
qualification, remaining startup/shutdown and optimized-budget failures, and
the new native-session containment measurement remain coordinator obligations.
