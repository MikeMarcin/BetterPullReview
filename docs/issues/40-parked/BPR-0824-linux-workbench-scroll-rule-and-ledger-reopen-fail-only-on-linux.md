---
schema: "implement-rfc.card.v1"
id: "BPR-0824"
title: "Linux: the workbench position rule and the review ledger reopen fail only on Linux"
status: "Parked"
rfcs: ["RFC-BPR-0007", "RFC-BPR-0028"]
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux under BPR-0816 (pre-existing)","found_at":"4883dbd9","run":"run-20260916-stations","dispatch":"dsp-002"}
parked_on: "human-review"
---

# BPR-0824: Linux: the workbench position rule and the review ledger reopen fail only on Linux

## Defect

`bpr-linux` reported four focused failures that were identical before and
after [[BPR-0816]] (see
[the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md),
"Focused Linux tests"). All four pass on macOS at the `99712ec3ada8e8482f3a855d3873d20690283f95` integration pass
(`build/r2-logs/dsp2-int2-courses.log`).

- `qt_workbench_window_scale_100`, `_125` and `_200`:
  `FAILED: the positions yield before the number does and keep their rule
  once it has gone` (`tests/adapters_qt/workbench_window_tests.cpp:1531`).
- `application_review_ledger_composition_contract`: the seed passes. The
  reopen fails with `REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED`; the store
  loads but holds no receipt.

These are not platform-bound by design. A difference that exists only on
Linux (Qt platform plugin and font metrics under offscreen/WSLg, filesystem
or SQLite durability, locale) is the likely cause. The review ledger case
could be a durability defect, so it matters more than the layout one.

## Scope

- Run the courses at the Linux station's current head after it merges main,
  so [[BPR-0818]]'s drawer change is included, and name each mechanism.
- `REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED`: establish whether the receipt
  never reached disk, reached a different root, or is filtered on load. A
  durability or root-selection defect is a product fix.
- The position rule: establish whether the course depends on font metrics or
  the platform plugin. Fix the course only if its claim is still asserted
  honestly on Linux.

## Acceptance

- All four pass on the Linux station, and the causes are named here.
- They still pass on macOS.

## Linux evidence and parking, 2026-09-17 (bpr-linux, task-20260917-lnx-verify, dsp-001-r2)

The full record is in
[the Linux evidence](../../verification/BPR-0824-linux-workbench-and-ledger.md).
Both fixes are on `linux-lane`: the ledger in `5875ce4f`, the workbench in
`3eff1b10`, pushed with the tip `de528ce3`.

### Causes

- **`REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED` is a course race.** It is not
  durability, root selection or load filtering.
  - The receipt reached disk and the reopen loaded and carried it over.
  - The probe asserted `ReceiptLoadHits == 1U`. The composing window's queued
    `refresh_shared_ledger` re-reads its own carry-over commit through
    `refresh_receipt` (since `78d4fa5c`), which makes the counter 2.
  - Qt's glib dispatcher on Linux usually runs that re-read before the probe's
    10 ms tick: 12 of 13 runs failed, against 2 of 13 with `QT_NO_GLIB=1`.
  - **Fix.** A `MaterialReceiptHits` host counter, which only
    `observe_material` increments. The probe requires one material observation
    and one material receipt hit. A deleted receipt still fails the probe.
- **The position rule: the course pinned macOS geometry at 960 px.** macOS has
  a 78 px window-button inset, Core Text and the command glyph. On Linux there
  is no inset, DejaVu Sans and "Ctrl+1": the number still fits at 960 and the
  positions have already yielded, which is the declared order. Two product
  defects on every platform sat behind it in `apply_strip_width`:
  - The modality form was decided against the previous resize's form, so
    resizes alternated between two layouts.
  - The toolbar's size hint lagged `set_compact`.
  - **Fix.** The form now comes from the width, with a toolbar invalidate.
    The course asserts the declared order at every width of a sweep instead of
    one macOS width. It keeps the 1920, 1280 and floor checks, and a single
    large resize pins the invalidate. The old compaction and the fix without
    the invalidate both fail it.
- **A masked third failure: the seam check at 1.0 and 2.0.** The basic Windows
  style that the headless Linux lane forces does not set `Qt::WA_Hover` on the
  splitter handle, so no hover repaint happens. Under a forced style the check
  now grants the attribute and still measures the repaints and `underMouse()`.

### Verification on Linux at `de528ce3`

- **Builds.** Both presets built stock with `--fresh` and `--clean-first`: `dev`
  1241 of 1241 steps, `app-release` 429 of 429, no first-party diagnostic.
- **Courses.** `qt_workbench_window_scale_100`, `_125` and `_200` pass (20.85,
  21.49 and 22.62 s). `application_review_ledger_composition_contract` passes
  (4.36 s), and passes 10 of 10 on repeat. `application_review_ledger_host`
  passes.
- **Independent review.** Two `rfc-reviewer` passes, ledger and workbench,
  found nothing blocking. One portability point was corrected in `3eff1b10`.
- **Not run.** macOS; this station cannot run it.

### Parked on human-review: the owner's question

Under the headless Linux lane's forced basic Windows style, which of these
should `test_the_split_cursor_stays_over_a_resting_handle` do?

- **(a)** Grant `Qt::WA_Hover` to the handle itself, as `3eff1b10` does. The
  premise "the style sets `WA_Hover`" is then asserted only where the
  application's own style runs, such as macOS.
- **(b)** Re-polish the handle with a Fusion style instance at whole-number
  scales, so that the Linux lane still sees a real style set the attribute.

**Also open.** Should that premise hold for other Linux platform themes (gtk,
Breeze), or only for the macOS and Fusion styles the test comment names?

**Still owed after the answer.** The four courses on macOS. Until then this card
cannot finish.

**Follow-up candidate, not changed.** `review_ledger_round_trip` still takes
"present on entry" from `ReceiptLoadHits > 0U`, which the same re-read can
satisfy.
