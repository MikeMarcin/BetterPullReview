---
schema: "implement-rfc.card.v1"
id: "BPR-1032"
title: "Qt 6.11 keeps a HarfBuzz buffer in every measured line's layout, about 1.8 KiB a line"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0893, Mac batch 2, 2026-09-23","found_at":"67d5be84","related":["BPR-0893","BPR-0967"],"upstream":"qtbase 8209078e0e (2025-11-04), QTextEngine: keep the hb_buffer_t across shapeTextWithHarfbuzzNG calls"}
---

# BPR-1032: Qt 6.11 keeps a HarfBuzz buffer in every measured line's layout, about 1.8 KiB a line

## Finding

Since the Qt 6.11.2 upgrade (`92c4e159`, 2026-09-21), opening the large
review's 20,000-line comparison holds about 70 MiB more live memory than it
did under Qt 6.10.3. [[BPR-0893]] measured it and traced it to one upstream
change:

- qtbase `8209078e0e` ("QTextEngine: keep the hb_buffer_t across
  shapeTextWithHarfbuzzNG calls", 2025-11-04) turned the HarfBuzz buffer that
  shaping used to create and destroy on every call into a `QTextEngine` member.
  It is released only in `~QTextEngine`. `clearLayout()`, `freeMemory()` and
  `invalidate()` leave it alone. Qt's `6.11` and `dev` branches are unchanged.
- A `QPlainTextEdit` keeps one `QTextLayout` per block for the life of the
  block. The review panes measure every line natively
  (`plain_text_pane_measurement::advance`, `measured_alignment_axis.cpp`),
  and that shapes every block. So every measured line now retains one
  320-byte buffer and two 768-byte arrays: 1,856 bytes.
- Measured with `malloc_history` at `large_file_ready`, same course, the two
  Qt versions: the allocations under `plain_text_pane_measurement::advance`
  are 159,259 for 17.0 MiB at Qt 6.10.3 and 159,176 for 73.5 MiB at Qt
  6.11.2. `QTextEngine::shapeTextWithHarfbuzzNG` alone holds 70.4 MiB in
  119,340 live allocations, three per line, where Qt 6.10.3 held none.
- The commit before the upgrade (`92c4e159^`) and the upgrade itself, whose
  only source change is a Windows header, differ by that amount. The process
  footprint above start at `large_file_ready` is 121.6–122.6 MiB before and
  184.2–186.2 MiB after.

The cost scales with measured lines, not with the file's bytes. A
100,000-line generated file compared side by side would hold about 350 MiB
for this alone. The code is platform-independent, so Windows and Linux pay it
too, although only macOS has measured it.

## Outcome

Either the review panes stop paying a retained HarfBuzz buffer for every line
they measure, or this card records why that costs more than it saves.

## Scope

- The panes are `NoWrap`, but they measure natively because a line that falls
  back to another font can be taller than its neighbours. Any change has to
  keep that exactness. A line whose every character the primary font covers,
  with no format that changes the font, might be measured without shaping it.
  Whether that fast path is exact is a question to settle, not an assumption.
- There is no public Qt API that releases one block's buffer short of
  destroying the block. Reaching into `QTextEngine` through private headers is
  not an answer. Neither is patching the pinned Qt.
- Report upstream if there is a case. The buffer is per engine by design, and
  a `QPlainTextEdit` is the case where one engine per block makes that
  expensive.
- The Mac footprint budgets [[BPR-0893]] re-derived accept this cost at
  224 MiB. A fix here lowers the measured peak, so re-derive them again from
  the new measurement rather than leaving the headroom in place.

## Acceptance

- The large review's `large_file_ready` live heap, which
  `application_large_review_resources` prints as `LiveHeapGrowthBytes` since
  [[BPR-0893]], drops by most of the 70 MiB, or the card records the
  measurement that says it cannot.
- Pane geometry stays exact: `qt_syntax_pane_composition`,
  `qt_diff_workbench_scale_100/125/200` and the large-review courses pass,
  with a fallback-font line in a fixture.

## Boundaries

The core decides, the host performs, Qt presents. Measurement stays in the Qt
adapter. No dependency, pin or Qt patch changes without their own decision.
