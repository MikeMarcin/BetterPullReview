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

## Shared step, Linux evidence (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux` (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2), `build/dev`, on
`main` at `dec43dda`. Logs are under `build/r2-logs/bpr0863-*` on that station.
They are not committed.

**Shape: the card's first option, a shared step the course calls.** Its
second option, a harness-wide course option, would need one mechanism per
harness: the automated run in `main.cpp`, the probes in
`github_application.cpp` and the benchmark's synchronous waits. All three
already share `github_application_fixture_access`, so the step lives there,
in the `BPR_BUILD_TESTING` region:

- `course_modality_step` holds the modality the course states and the
  surfaces it needs there (`course_surfaces`: comparison, files drawer,
  conversations drawer, repositories drawer).
- `enter_course_modality(application, step)` presses the strip's public
  modality action once, as a reviewer does. It answers `entered` when the
  window and the host agree: the strip, the core's retained modality and all
  three drawers, with no host request outstanding, so the drawer store the
  modality causes has landed, and every needed surface is shown. Otherwise,
  after 5 s, it fails with one of two named terms:
  - `COURSE_MODALITY_STALLED`: the window never reached the modality;
  - `COURSE_MODALITY_WITHHOLDS`: it did, and the modality withholds a needed
    surface.

  The bound is time, not ticks. The automated run advances the step every
  10 ms, and the benchmark re-reads it every millisecond (4,582 calls in
  5 s in control B). A floor of 16 calls keeps one long tick from reading as
  a stall.
- `course_modality_facts(application)` is the tail both terms print, and a
  course's own failure lines print it too. It generalizes [[BPR-0860]]'s
  `modality=` and `drawer_controls=`: the modality in the window and in the
  host, the centre, `offered=` and `withheld=` read from the widgets rather
  than from a table, each drawer on both sides, the drawer controls,
  outstanding requests and the splitter sizes.

**The four courses stranded on Overview now state their modality** and enter
it through the step, where each used to enter it on its own:

| Course | States | Needs | Was |
|---|---|---|---|
| diff probe, `main.cpp` ([[BPR-0823]]) | Review | comparison | `choose_modality(review)` only if on Overview; no wait |
| large-review `enter_read` ([[BPR-0858]], [[BPR-0940]]) | Read | comparison | strip press, then `surface.isVisible()` bounded only by the 90 s budget |
| workbench composition, before the drag ([[BPR-0860]]) | Review | files drawer | strip press plus its own drawer-state wait |
| UIforETW selected diff ([[BPR-0859]]) | Review | comparison | strip press plus `UIFORETW_REVIEW_MODALITY_STALLED` |

Each step sits where the course entered its modality before, so no earlier
assertion or dispatch count moves. The course-specific waits after it stay:
the workbench drag waits for the files drawer to get width
(`WORKBENCH_FILES_DRAWER_EXPAND_STALLED`), and UIforETW waits for both panes
(renamed `UIFORETW_REVIEW_PANES_HIDDEN`). Their failure lines now end with the
shared facts. `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` ends with them too.

**The rule is written down once**, in
`docs/verification/fixture-catalog.md`, "The modality a course states". It
has the modality-to-centre-and-drawer table with each rule's owner
([[BPR-0674]], [[BPR-0724]], [[BPR-0727]], [[BPR-0721]]), how to use the step,
real lines from the controls below, and the list of courses that state a
modality. `tests/AGENTS.md` points course authors to it. The code cites the
catalogue rather than restating the table, and `offered=`/`withheld=` are
observed, so neither can drift from the product.

### Negative controls (each a temporary edit, reverted, then rebuilt)

| Break | Row | Log | Result |
|---|---|---|---|
| A: the workbench course states Read instead of Review | `application_workbench_composition_contract` | `bpr0863-negA-states-read.log` | `COURSE_MODALITY_WITHHOLDS:wanted=Read needs=files_drawer missing=files_drawer waited_ms=5009 … modality=Read host_modality=Read centre=comparison offered=comparison,conversations_drawer withheld=files_drawer,repositories_drawer … splitter_sizes=0,0,956,320`; process fails at 12.4 s of its 40 s budget |
| B: the step presses Overview whatever the course states | `application_diff_workbench_public_path`, `application_unavailable_diff_public_path` | `bpr0863-negB-strip-presses-overview.log` | `COURSE_MODALITY_STALLED:wanted=Review waited_ms=5008 … modality=Overview host_modality=Overview centre=overview offered=none withheld=comparison,files_drawer,conversations_drawer,repositories_drawer`, exit at 5.9 s; the benchmark prints the same line with `wanted=Read` at 5.0 s, then `Read modality did not present the comparison` |
| C: the diff probe skips the step (a stranded course) | `application_diff_workbench_public_path` | `bpr0863-negC-no-step.log` | `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED:modality=Overview host_modality=Overview centre=overview offered=none withheld=comparison,files_drawer,conversations_drawer,repositories_drawer … splitter_sizes=0,0,1280,0`, exit at 0.86 s |

Control A is the one-bisect failure BPR-0860 describes: the course is in a
modality that does not offer the drawer it drags. It is now named in one run,
together with what that modality withholds.

### Linux runs

Serial, `ctest --test-dir build/dev -j1 --output-on-failure -R …`, offscreen by
registration:

| Row | HEAD `dec43dda` (`bpr0863-baseline2-head.log`) | Candidate (`bpr0863-integration1.log`) |
|---|---|---|
| `application_diff_workbench_public_path` | Pass 2.75 s | Pass 2.60 s |
| `application_appearance_theme_customization_contract` | Pass 5.29 s | Pass 5.58 s |
| `application_appearance_store_fallback_contract` | Pass 3.26 s | Pass 3.33 s |
| `application_workbench_composition_contract` | Pass 3.17 s | Pass 3.01 s |
| `application_unavailable_diff_public_path` | Pass 4.13 s | Pass 3.87 s |
| `application_approximate_diff_public_path` | Pass 3.98 s | Pass 3.75 s |
| `application_large_review_benchmark` | Pass 7.92 s | Pass 6.92 s |
| `application_uiforetw_selected_diff_contract` | **Fail** 2.00 s | **Fail** 1.94 s, same failure |

`application_uiforetw_selected_diff_contract` is red at HEAD for reasons that
are not this card's, and the candidate fails identically. The strip now has 28
actions, and the course's exact row predates [[BPR-0990]]'s
`refreshQueueAction`
(`UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions=28`). Behind that, the comparison
takes 842/1000 of the window height, below the capture's 850 floor
(`UIFORETW_CAPTURE_LAYOUT_FAILED:987:842`, identical at HEAD). The shared step
itself passes in that course: every marker after it, from
`UIFORETW_RESOURCE_GESTURE_OK` to `UIFORETW_SELECTED_DIFF_TRACE_OK`, prints
OK. A diagnostic build that corrected only the strip row, since reverted
(`bpr0863-diag-uiforetw-row-corrected.log`), then failed only on the height
ratio. A separate card is needed. Its ID was requested from the coordinator;
this card does not fix it.

A first baseline (`bpr0863-baseline-2435.log`) took 31 s and 34 s for two
rows. It was the first run after a rebuild. The clean HEAD rerun above and
the candidate agree to within a few tenths of a second.

Also on the candidate, all pass: `application_review_round_trip_composition_contract`
(with the reseal below), `architecture_boundaries`, `git_protocol_boundary`,
`conditional_branch_identifier_contract`, `live_mutation_isolation` (after a
full `cmake --build --preset dev`, warning-free: 28 translation units, 29
links), `live_mutation_isolation_contract`, `qt_composed_github_scale_{100,125,200}`,
`application_native_appearance_probe_contract`,
`application_review_ledger_composition_contract`,
`application_defect_composition_contract` and `application_composition_contract`
(454.5 s, `bpr0863-composition-candidate.log`). That last contract builds a
tests-off Release graph and scans it, so the step and its terms stay out of
production.

**Rerun after moving onto `b93adf45`**, which brings [[BPR-1017]], [[BPR-0986]]
and [[BPR-1035]]. A full `cmake --build --preset dev` is warning-free. The same
20 rows give the same results (`bpr0863-integration2-b93adf45.log`): 19 pass,
and `application_uiforetw_selected_diff_contract` fails on the same two lines,
with the step's markers OK. `application_composition_contract` passes again
(492.9 s, `bpr0863-composition-b93adf45.log`).

`conditional_branch_identifiers` stays red, as tracked on [[BPR-0866]]. It
reports the same 9 findings at HEAD and on the candidate, on both bases
(`bpr0863-condbranch-{head,candidate}*.log`, compared with line numbers
normalized). clang-format 22 leaves the four C++ files unchanged.
clang-tidy 21 on the three touched translation units passes
(`bpr0863-tidy.log`), and every warning it lists is on an untouched line. The
documentation gate prints `DOCUMENTATION_OK`.

### Pinned-code review and reseal

Pinned files changed: `src/app/src/github_application.cpp` (full-application
digest and production manifest) and `src/app/main.cpp` (production manifest).
The change was prepared on `dec43dda` and moved onto `b93adf45`, which
carries [[BPR-1017]]'s reseal (`bfd927e7`). Nothing has touched `src/app/`
since that reseal, so the reviewed range is exactly this change. Every edit is in a
`BPR_BUILD_TESTING` region:

- the new fixture-access step and facts;
- the workbench-composition and UIforETW probe states and phases;
- the stall lambda's tail;
- `main.cpp`'s `automated_run` member and diff-probe branch.

It reads window and host state and presses the strip's existing public action.
No product, credential, provider, vault, transport or mutation path changes.
The mutation-branch pin is unchanged at `a7e2081e…d1e8a`. New values on
`b93adf45`: full application
`fc0af7d0ff1c4ae386ab847006567bed58f6dd9a823936042312478d52061422`, production
manifest `6295781bbcf3f6d4404a651ca339e015ed97f105a052a9085ed1c49a564455c8`.

### Acceptance state

- **A course states its modality; a wrong modality fails by name** — pass on
  Linux (controls A, B and C above).
- **The three courses use the shared path and still pass on macOS and Linux**
  — Linux pass (table above). **macOS pending.**
- **The rule is written down once** — pass (the fixture catalogue section).

### Still needed: the macOS run (Mac integrator)

On macOS, `build/dev` at this card's commit or later, one serial run:

```sh
ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_diff_workbench_public_path|application_appearance_theme_customization_contract|application_appearance_store_fallback_contract|application_workbench_composition_contract|application_unavailable_diff_public_path|application_approximate_diff_public_path|application_large_review_benchmark|application_review_round_trip_composition_contract)$'
```

Observe:

- all eight pass;
- no `COURSE_MODALITY_STALLED` or `COURSE_MODALITY_WITHHOLDS` appears;
- note the load average. The step's 5 s bound is the one assumption a slow
  Mac could test. BPR-0860 recorded 21 s workbench rows at load 2.2–4.5, and
  a spurious stall would print `waited_ms=` and `waits=` with the host's
  `outstanding_requests=`.

`application_uiforetw_selected_diff_contract` is expected to stay red for the
two unrelated reasons above. Worth one look: no `COURSE_MODALITY_*` line, and
`UIFORETW_RESOURCE_GESTURE_OK` printed. If all of this holds, the card can
finish. Windows is not part of this acceptance.
