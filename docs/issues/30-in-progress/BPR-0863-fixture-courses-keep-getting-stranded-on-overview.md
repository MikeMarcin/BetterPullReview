---
schema: "implement-rfc.card.v1"
id: "BPR-0863"
title: "Fixture courses keep getting stranded on Overview, and each one is rediscovered the hard way"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005, on the third recurrence","found_at":"7d548233","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0863: Fixture courses keep getting stranded on Overview, and each one is rediscovered the hard way

## Defect

Three end-to-end courses in this run alone failed because the window they
drive opens on **Overview**, where the product deliberately offers less than
the course assumes. Each cost a separate diagnosis:

- [[BPR-0823]]: the `DiffProbe` course.
- [[BPR-0858]]: the unavailable-diff course, whose status label was hidden
  because Overview replaces the comparison. Fixed by entering Read through
  the strip's public modality action before selecting a file.
- [[BPR-0860]]: the workbench composition course, whose drag of the
  files-drawer handle dispatched nothing because [[BPR-0727]] collapses all
  three drawers in Overview and a collapsed drawer has zero width. Fixed by
  entering **Review**, the only modality whose files drawer is expanded
  ([[BPR-0724]] collapses it in Read and Discuss too).

The trap has a clear cause. Since [[BPR-0771]] these courses name their pull
request in the Home bar, so the window opens on [[BPR-0661]]'s default
`OpenModality`, which is Overview. The modality then decides drawer state
inside `set_workbench_modality_intent`. A course written against an older
entry path silently gets a window whose drawers, comparison and status label
are not what it was written for, and the failure surfaces as a stall or a
missing label rather than as "wrong modality".

None of the three was a product defect. Each was a real course asserting a
gesture the product intentionally does not offer in the modality the course
happened to land in.

## Scope

Make the modality a course states rather than inherits. Either:

- a shared step the courses call to leave Overview for a named modality,
  waiting until the window and host agree, so the sequence lives in one place
  instead of being re-derived; or
- a course option that names the modality the course needs, with the harness
  entering it before the first assertion and failing by name if it cannot.

Whichever shape, the failure when a course is in the wrong modality should
name the modality and what that modality withholds, so the next occurrence is
one run rather than one bisect. `BPR-0860`'s
`WORKBENCH_FILES_DRAWER_EXPAND_STALLED` term already prints `modality=` and
`drawer_controls=`; that is the pattern to generalize.

Also record, somewhere a course author reads, which modality expands which
drawer. Today that is spread across BPR-0661, BPR-0724 and BPR-0727.

## Acceptance

- A course states the modality it needs, and a course in the wrong modality
  fails with a term naming the modality rather than stalling or losing a
  label.
- The three courses above use the shared path, and still pass on macOS and
  Linux.
- The modality-to-drawer-state rule is written down once.
