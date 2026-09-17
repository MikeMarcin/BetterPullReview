---
schema: "implement-rfc.card.v1"
id: "BPR-0823"
title: "Two appearance contracts never see the diff evidence comparison"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux under BPR-0816 (pre-existing), confirmed on macOS by the dsp-002 integration pass","found_at":"99712ec3ada8e8482f3a855d3873d20690283f95","run":"run-20260916-stations","dispatch":"dsp-002"}
acceptance: [{"criterion":"AC1 both contracts pass on macOS and the cause is named","outcome":"pass","revision":"10f30f0496a9f452be01d625a23d9e53e24b71db","evidence":{"kind":"command","detail":"First bad 284d1eac (BPR-0771): PR opened through the Home bar lands on Overview so the comparison is 0x0; fix chooses Review before measuring and fails fast. Both pass at 10f30f04 and the integrated head (build/r2-logs/dsp2-int8-ctest.log, dsp2-int9-courses.log); negative control dsp2-0823-ctest-negative.log."}},{"criterion":"AC2 Linux reruns all three appearance contracts; separate Linux cause gets its own card","outcome":"pass","revision":"780d0a1c3d2c52932e007601d179657168ba3124","evidence":{"kind":"command","detail":"bpr-linux 3 runs x 3 contracts all pass (docs/verification/dsp2-linux-confirmation.md, BPR-0823); gesture EPOCH_FIXTURE_SEED_FAILED does not reproduce at a stock build, so no separate Linux cause remains to card."}}]
---

# BPR-0823: Two appearance contracts never see the diff evidence comparison

## Defect

Two appearance contracts fail on both platforms, in the same way:

- `application_appearance_theme_customization_contract`;
- `application_appearance_store_fallback_contract`.

- **macOS, at the `99712ec3ada8e8482f3a855d3873d20690283f95` integration pass**
  (`build/r2-logs/dsp2-int2-courses.log`). Theme customization passes
  every appearance stage, through `equal-stored-commit-acknowledged`, then
  prints `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` and times out at 80 s.
  Store fallback prints the same marker and times out
  (`appearance_store_fallback_contract.cmake:89`). In the same run,
  `application_appearance_gesture_contract` passes (73.29 s).
- **Linux, `bpr-linux`.** Both fail identically before and after
  [[BPR-0816]]'s changes, and there the gesture contract fails too, with
  `EPOCH_FIXTURE_SEED_FAILED` and `RUN_BUDGET_EXPIRED … stage=4`. See
  [the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md),
  "Focused Linux tests".

The appearance behaviour under test passes. What fails is the courses'
diff-evidence step, which never sees a visible comparison. When that
started is not established. Before this card existed, a red architecture
gate and a nested checkout hid several failures like this.

## Scope

- Find the first bad revision on macOS, and the mechanism. The candidates
  include the drawer, Home and window-composition changes of the last week:
  [[BPR-0751]]'s refresh hold, which [[BPR-0818]] has since ended; the
  RFC-BPR-0032 Home work; and [[BPR-0705]]'s per-profile Settings.
- Fix the product if a person no longer sees the comparison in that state.
  Change the courses only under a cited accepted decision.
- Then check the Linux gesture-contract seed failure: the same cause, or a
  separate Linux defect.

## Acceptance

- Both contracts pass on macOS, and the cause is named here.
- The Linux station reruns all three appearance contracts and records the
  result. A separate Linux cause gets its own card.

## Cause and fix (dsp-002)

**First bad revision: `284d1eac`** ([[BPR-0771]], "Retire the stacked session
chrome"). The bisect ran from `e337c1fa` (pass) to `867e4bed` (fail).
Parent `e0c7caaf` passes both contracts (theme 25.82 s, store fallback
13.11 s); `284d1eac` fails both (`build/r2-logs/dsp2-0823-bisect.log`,
`dsp2-0823-ctest-{e0c7caaf,284d1eac}.log`).

**Mechanism: a course that did not follow a product change.** BPR-0771
removed the locator, so the fixture course opens its pull request through the
Home bar. A pull request opened that way lands on Overview (RFC-BPR-0028's
opening overview, [[BPR-0661]]'s default modality). The centre then shows the
overview page, and the comparison's visible area is 0x0
(`dsp2-0823-ctest-head-diag.log`). A person sees the comparison once they
choose Read or Review, which is the designed behaviour, so the product was
not changed.

The timeout was an older fault, from `07faf400` ([[BPR-0219]]). After the
visibility failure the tick returned instead of breaking, so shutdown never
rearmed and the process hung until the harness timeout.

**Fix `10f30f0496a9f452be01d625a23d9e53e24b71db`.** The diff probe chooses Review before it measures, which is
what this path showed before BPR-0771, and a visibility failure now breaks
and fails fast. With the modality step disabled, both contracts fail with
`DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` in about 12 s
(`dsp2-0823-ctest-negative.log`).

**macOS.** At `10f30f04` both contracts pass, and
`application_appearance_gesture_contract` still passes
(`build/r2-logs/dsp2-int8-ctest.log`). Both pass again at the integrated
head (`dsp2-int9-courses.log`). With this fixed,
`application_diff_workbench_public_path` reached a separate stale pass
expression, now [[BPR-0828]].

**Status.** The macOS criterion is met. Linux reruns of all three appearance
contracts, including the gesture contract's `EPOCH_FIXTURE_SEED_FAILED`, go
with the next `bpr-linux` envelope. The card stays In Progress until they
are recorded.

## Linux reruns; finished (dsp-003)

`bpr-linux` reran all three appearance contracts three times at `780d0a1c`,
which contains `10f30f04`, and main merged it as `48b6422b`. All 9 runs pass:
theme customization, gesture and store fallback, each printing its `_OK`
marker and never `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` ("BPR-0823" in
[the dsp-002 Linux confirmation](../../verification/dsp2-linux-confirmation.md)).

The gesture contract's earlier Linux `EPOCH_FIXTURE_SEED_FAILED` /
`RUN_BUDGET_EXPIRED … stage=4` does not reproduce (3 of 3 pass). It was seen
only at `ef3f782b` on a probe tree built with `-Wno-error`. Its cause was not
bisected, so it is not named; it was not a separate Linux defect at a stock
build, and no card is opened for it.
