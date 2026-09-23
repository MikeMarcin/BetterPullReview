---
schema: "implement-rfc.card.v1"
id: "BPR-0864"
title: "The workbench composition probe's remaining phases can still expire the run budget with only a stage number"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005, from the BPR-0860 fix","found_at":"7d548233","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0864: The workbench composition probe's remaining phases can still expire the run budget with only a stage number

## Defect

[[BPR-0860]] capped the workbench composition probe's drawer-width phases and
gave them named terms, because an uncapped wait there burned the whole 40-second
run budget and reported nothing but `RUN_BUDGET_EXPIRED … stage=2`. Diagnosing
it needed instrumentation and a hunt for the introducing commit.

The same probe's other phases in
`src/app/src/github_application.cpp` are still uncapped and would fail the
same unhelpful way:

- `awaiting_observation`
- `awaiting_diff`
- `awaiting_change_projection`
- `awaiting_invalidation`
- `awaiting_failure`

A stall in any of them costs a full budget and a fresh investigation, when a
named term plus the phase's own state would cost one run. BPR-0860's
`stall(term)` lambda and its `WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED` /
`WORKBENCH_DRAWER_WIDTH_STORE_STALLED` / `WORKBENCH_FILES_DRAWER_EXPAND_STALLED`
terms are the pattern to extend; the work is mostly choosing what each phase
should print.

This is not a live failure: every phase passes today. It is a diagnosability
debt with a known, cheap shape, carded so the next stall is not another bisect.

## Scope

- Give each remaining phase a bounded wait, well inside the run budget, and a
  named term.
- Print the state that phase actually depends on, the way the width terms
  print splitter sizes, drawer state, modality and outstanding requests.
- Reuse BPR-0860's `stall()` lambda rather than adding a second mechanism.
- `src/app/src/github_application.cpp` is pinned by the round-trip contract, so
  the change reseals with a recorded review.
- Check whether sibling probes in the same file share the pattern; if they do,
  say so here rather than widening this card silently.

## Acceptance

- No phase of the workbench composition probe can reach `RUN_BUDGET_EXPIRED`
  before naming why; each names its own stall.
- Each named term is shown to fire, by a deliberately broken run per phase or
  by an equivalent check.
- The row still passes on macOS and Linux.

## Every phase bounded, Linux evidence (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux` (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2), `build/dev`, on
`main` at `f28f696a` (the claim, on `1dfe0600`). Logs are under
`build/r2-logs/bpr0864-*` on that station. They are not committed.

**Re-check at HEAD: six phases, not five.** The list above missed
`awaiting_file_projection`. When neither the files drawer's selection nor the
intent it dispatches arrives, that phase returned `pending` with no bound, the
same as the other five. The rest of the list was accurate. Since [[BPR-0863]],
`awaiting_files_drawer` first enters Review through the shared modality step,
which bounds itself (`COURSE_MODALITY_STALLED` / `COURSE_MODALITY_WITHHOLDS`).
The splitter wait after the step was already capped by BPR-0860.

**Shape: one wait for every phase, through BPR-0860's `stall()`.** In
`exercise_workbench_composition`, a phase that is not yet satisfied now returns
`wait(term, facts)`:

- `stall(term, waited, facts)` is BPR-0860's lambda. It now takes the phase's
  own facts, and still ends with [[BPR-0863]]'s `course_modality_facts` tail.
  That tail already prints `outstanding_requests=`, so the width facts no
  longer print it a second time.
- `wait` keeps the phase pending until the bound, then stalls by name. The
  bound is `workbench_composition_wait`, 5 s of wall clock, with a floor of 16
  calls, the same as the modality step's.
- `advance(phase)` is now the only way to move between phases, and the only
  thing that clears the wait's clock and count. So a wait cannot restart
  itself on a tick and never reach its bound, which is the trap [[BPR-0859]]
  found in the UIforETW probe. The clock starts at a phase's first wait. In
  `awaiting_files_drawer`, that is after the modality step has answered, so
  the step's 5 s and the splitter's 5 s do not overlap.

**The two width phases changed from BPR-0860's 512-tick cap to the same time
bound.** That keeps one mechanism in the probe instead of two. A tick cap
stretches with the event loop under load, and the run budget does not; that is
the reason `main.cpp` gives for measuring the budget in wall-clock time. At the
run's 10 ms tick the two caps are the same length on an idle machine; BPR-0860
measured 512 ticks at about 5.3 s on macOS.

**The width facts now print the drag's expectations only after the drag.**
Before, `WORKBENCH_FILES_DRAWER_EXPAND_STALLED` printed `expected_persisted=1
requested_files_width=0 initial_files_width=0`, because the drag's baselines
had not been taken yet. Those numbers were false. That phase now prints the
drawer's facts only. The two drag terms print the same fields as before, in
a different order.

Every stall line has this form:

```text
TERM:waited_ms=… wait_cap_ms=5000 waits=… <the phase's facts> <course_modality_facts>
```

| Phase | Term | The phase's facts |
|---|---|---|
| `awaiting_observation` | `WORKBENCH_OBSERVATION_STALLED` | `snapshot=`, `snapshot_files=`, `window_read_quiescent=`, `navigation=`, `actionable=`, `review_active=` |
| `awaiting_file_projection` | `WORKBENCH_FILE_SELECTION_STALLED` | `intent_dispatches=` / `expected_intent_dispatches=`, `file_selections=` / `expected_file_selections=`, `current_row=`, `drawer_rows=`, `host_selected=` |
| `awaiting_diff` | `WORKBENCH_SELECTED_DIFF_STALLED` | `selected_diff_begins=`, `selected_diff_publications=` (each with `expected_…`), `selected_diff_failures=`, `last_selected_diff=`, `comparison_result=`, `comparison_selection=`, `host_selected=`, `diff_jobs=`, `diff_pending=`, `diff_quiescent=`, `epoch_jobs=` |
| `awaiting_change_projection` | `WORKBENCH_CHANGE_DISPATCH_STALLED` | `change_dispatches=` / `expected_change_dispatches=`, `next_change_enabled=`, `previous_change_enabled=`, `selection_moved=`, `groups=` |
| `awaiting_files_drawer` (after the step) | `WORKBENCH_FILES_DRAWER_EXPAND_STALLED` | `dispatches=`, `persisted_layouts=`, `layout_updates=`, `host_files_width=`, `splitter_widgets=`, `persistence_available=`, `profile_scoped=` |
| `awaiting_width_store` | `WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED` or `…_STORE_STALLED` | the same, plus `expected_dispatches=`, `expected_persisted=`, `requested_files_width=`, `initial_files_width=` |
| `awaiting_invalidation` | `WORKBENCH_INVALIDATION_STALLED` | `generation=` / `invalidated_generation=`, `invalidations=` / `expected_invalidations=`, `review_active=`, `actionable=`, `host_selected=`, `navigation=`, `publications=` / `invalidated_publications=` |
| `awaiting_failure` | `WORKBENCH_FAILURE_DELIVERY_STALLED` | `failures=` / `expected_failures=`, `review_active=`, `navigation=`, `publications=` / `observed_publications=` |

Each phase's facts are the inputs to the condition that phase waits on. Host
outstanding requests appear in the tail. The failure paths that were already
immediate, and every original assertion, are unchanged.

**Phase durations on Linux**, from a timing print in a temporary diagnostic
build (`bpr0864-timing{1,2,3}.log`, `bpr0864-final-timing.log`,
`bpr0864-procstart{1,2,3}.log`). The probe starts 1.31–1.34 s into the
process, and the observation is ready on its first call, within 2 ms. File
projection takes 10–11 ms, the selected diff 37–59 ms, the change 17–20 ms, the
modality step plus the drawer's room 57–68 ms, the width store 18–29 ms and
the invalidation 10 ms. The probe's last phase begins about 0.2 s after it
starts, in a 1.6–1.9 s process. So 5 s is about 80 times the slowest phase
here. BPR-0860's broken macOS runs reached the width phase at about 11 s, so a
stall anywhere in this probe on macOS should name itself by about 17 s, against
a 40 s budget.

### Negative controls: one break per phase

All nine breaks were in one temporary diagnostic build of the final candidate
`a789ce25…` (the source's SHA-256). `BPR0864_BREAK=n` selected the break. The
diagnostic edits were then removed; the restored file matches the candidate
hash, and nothing of them remains (`grep -c bpr0864` gives 0). Each run is the
contract's first run: the same command and environment, run directly
(`bpr0864-run.sh`, logs `bpr0864-final2-neg<n>.log`). Every run ended in
`WORKBENCH_COMPOSITION_TRACE_FAILED`. None printed `RUN_BUDGET_EXPIRED`.

| n | Break | Term, and the facts that name the cause | Exit |
|---|---|---|---|
| 1 | the accepted handoff never reaches the workbench host (`observe` skipped) | `WORKBENCH_OBSERVATION_STALLED:waited_ms=5002 … snapshot=present snapshot_files=1 window_read_quiescent=true navigation=absent actionable=0 review_active=true …` | 6.41 s |
| 2 | the file gesture is never made (`setCurrentIndex` skipped) | `WORKBENCH_FILE_SELECTION_STALLED:waited_ms=5008 … intent_dispatches=8 expected_intent_dispatches=9 file_selections=0 expected_file_selections=1 current_row=-1 drawer_rows=1 host_selected=absent …` | 6.51 s |
| 3 | the composition drops the selected-diff publication | `WORKBENCH_SELECTED_DIFF_STALLED:waited_ms=5003 … selected_diff_begins=1 expected_selected_diff_begins=1 selected_diff_publications=0 expected_selected_diff_publications=1 … last_selected_diff=absent comparison_result=absent … host_selected=present diff_jobs=0 …` | 6.61 s |
| 4 | the change dispatch is lost before it counts | `WORKBENCH_CHANGE_DISPATCH_STALLED:waited_ms=5002 … change_dispatches=0 expected_change_dispatches=1 next_change_enabled=true previous_change_enabled=false selection_moved=false groups=3 …` | 6.51 s |
| 5 | the drawer-room condition is forced false after the step (see below) | `WORKBENCH_FILES_DRAWER_EXPAND_STALLED:waited_ms=5002 … dispatches=0 persisted_layouts=4 layout_updates=1 host_files_width=320 splitter_widgets=4 … modality=Review … files_drawer=expanded … splitter_sizes=0,320,632,320` | 6.71 s |
| 6 | the drag delta is forced to 0 (BPR-0860's break A) | `WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED:waited_ms=5001 … dispatches=0 … expected_dispatches=1 expected_persisted=5 requested_files_width=361 initial_files_width=320 …` | 6.61 s |
| 7 | the width dispatch returns before the host resize (BPR-0860's break B) | `WORKBENCH_DRAWER_WIDTH_STORE_STALLED:waited_ms=5010 … dispatches=1 persisted_layouts=4 … expected_dispatches=1 expected_persisted=5 …` | 6.61 s |
| 8 | the exact invalidation is never sent | `WORKBENCH_INVALIDATION_STALLED:waited_ms=5006 … generation=2 invalidated_generation=2 invalidations=0 expected_invalidations=1 review_active=true …` | 6.61 s |
| 9 | the observation of the unavailable review is never issued | `WORKBENCH_FAILURE_DELIVERY_STALLED:waited_ms=5001 … failures=0 expected_failures=1 review_active=false navigation=absent …` | 6.61 s |

**Before, for comparison.** Break 3 was also built against HEAD's probe, with
only the switch and the dropped publication added (`bpr0864-before-neg3.log`).
That run ended at 40.56 s:
`RUN_BUDGET_EXPIRED:elapsed_ms=40019 budget_ms=40000 over_ms=19 stage=2
status="Pull request ready."`. Every marker from
`WORKBENCH_SELECTED_DIFF_REQUEST_FAILED` onward read FAILED. The candidate
names the same fault in 6.6 s.

**Break 5 is a forced condition.** Unlike the other eight, it does not remove
a product dependency. The realistic cause of that term was a modality that
withholds the files drawer, and [[BPR-0863]]'s step now names that first
(`COURSE_MODALITY_WITHHOLDS`, its control A). After the step, the drawer is
shown and expanded and nothing is outstanding. Two realistic breaks were
tried, and the course passed both times (`bpr0864-neg5.log`,
`bpr0864-neg5b.log`):

- setting the drawer's maximum width to 0 as the phase began;
- setting it again on every tick after the strip press while the step was
  pending.

The drawer restores its maximum width whenever its expanded state is applied
(`workbench_drawer.cpp` ~558), which is the likely reason neither break held.
So the term now guards only two cases: a splitter that gives an expanded,
shown drawer no room, and a request left outstanding after the step. The
forced run shows the term is reachable and prints the right facts. The earlier
candidates `4771891a…` and `c8a60aae…` gave the same nine terms
(`bpr0864-neg<n>.log`, `bpr0864-final-neg<n>.log`).

### Linux runs on the candidate

Serial `ctest --test-dir build/dev -j1 --output-on-failure -R …`:

| Row | HEAD `f28f696a` | Candidate |
|---|---|---|
| `application_workbench_composition_contract` | Pass 2.96 s (`bpr0864-baseline-head.log`) | Pass 3.03, 2.99, 3.00 s (`bpr0864-final-row{1,2,3}.log`) |
| `application_review_round_trip_composition_contract` (resealed) | — | Pass 1.66 s |
| `architecture_boundaries`, `git_protocol_boundary`, `conditional_branch_identifier_contract`, `test_network_isolation_manifest`, `live_mutation_isolation_contract` | — | Pass (`bpr0864-final-gates.log`) |
| `application_composition_contract`, which builds a tests-off Release graph and scans it, so the new terms stay out of production | — | Pass 463.9 s (`bpr0864-composition-candidate.log`) |

The first three runs of an earlier candidate binary, taken right after a
rebuild, took 13.3, 27.8 and 11.1 s. The next three runs of the same binary
took 3.08–3.12 s, and running the course directly took 1.98 s. BPR-0863 saw
the same first-run-after-rebuild slowness. All of those runs passed.

- The contract's second run still fails at once in `awaiting_file_projection`,
  with the Qt selection wire cut: the gesture registers without a dispatch.
  That path does not wait, and it passes in every row run above.
- `/usr/bin/clang-format` 22 leaves the file unchanged.
- clang-tidy 21 on `github_application.cpp` passes (`bpr0864-tidy3.log`). Its
  two warnings are on untouched lines: `dif_runtime.hpp:1` and
  `github_application.cpp:8171`. The first draft of the facts added
  `bugprone-misplaced-widening-cast` warnings (`static_cast<qulonglong>(x +
  1U)`). They are now widened before the add. That also clears the two
  warnings of this kind that BPR-0860's width line had.
- `conditional_branch_identifiers` stays red, as tracked on [[BPR-0866]], with
  the same 9 findings BPR-0863 recorded, none of them in this file
  (`bpr0864-condbranch-candidate.log`).
- The known-red rows are no worse (`bpr0864-known-red-candidate.log`):
  - `application_uiforetw_selected_diff_contract` fails on the same two lines
    BPR-0863 recorded, `UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions=28` and
    `UIFORETW_CAPTURE_LAYOUT_FAILED:987:842`. It prints
    `UIFORETW_RESOURCE_GESTURE_OK`, and no stall or `COURSE_MODALITY_*` line.
  - `msvc_secure_crt_contract` fails on [[BPR-1026]]'s four `std::getenv`
    findings in the release-update end-to-end tests.

### Sibling probes in the same file

Checked, not widened:

| Probe | Unbounded waits | Named at expiry? |
|---|---|---|
| `exercise_appearance_theme_customization` | all 18 `pending` returns | No |
| `exercise_review_ledger_composition` | all 7 | No: the marker block gives the evidence bits reached, not the condition waited on |
| `exercise_review_round_trip_composition` | all 10 | No, same |
| `exercise_defect_composition` | all 14 | No, same |
| `exercise_native_appearance_selected_diff` | all 7 | Partly: each `pending` records a reason that `main.cpp` prints (`NATIVE_APPEARANCE_SELECTED_DIFF_REASON:`), but no facts |
| `exercise_threaded_reply` | all 17 | Partly: a per-phase note of its latest facts, printed as `THREADED_REPLY_DIAGNOSTIC:` |
| `exercise_uiforetw_selected_diff` | `awaiting_observation`; the first wait in the resource and rc diff phases while the gesture has not registered | Partly: `UIFORETW_REVIEW_PANES_HIDDEN`, `…_ROLE_PAINT_STALLED` and `UIFORETW_FOCUSED_LAYOUT_STALLED` are named, but the `512U` caps in the support-diff, resource-return and rc-diff phases fail silently |

Four probes share this card's defect exactly: `appearance_theme_customization`,
`review_ledger_composition`, `review_round_trip_composition` and
`defect_composition`. The UIforETW probe has both halves of it. Follow-up cards
are requested from the coordinator rather than allocated here. The same
`wait`/`advance`/`stall` shape would fit each one. The course also waits
before this probe starts, in `main.cpp`'s shared `open` stage
(`fixture_presentation_passed`). That wait belongs to every fixture course, not
to this probe's phases.

### Pinned-code review and reseal

The only pinned file changed is `src/app/src/github_application.cpp` (the
full-application digest and the production manifest). `git log
7c6ba1a5..HEAD -- src/app/` is empty, so nothing else has touched the pinned
scope since BPR-0863's reseal, and the reviewed range is exactly this change.
Every edit is inside `exercise_workbench_composition`, its probe state or its
anonymous-namespace bound. All three are in `BPR_BUILD_TESTING` regions: the
state at lines ~12727–13009, and the function and bound in the fixture-access
region that begins at ~14308. The edits read window, host, diff-host and
epoch counters and state, and write only to standard output. Every product
action the probe takes is unchanged: the file gesture, the change action, the
modality press, the drag, the invalidations and the unavailable observation.
No product, credential, provider, vault, transport or mutation path changes.
The mutation-branch pin is unchanged at `a7e2081e…d1e8a`. New values:

- full application
  `a789ce258d07eeb77fc1ea9156080b337da85ee171c51b8b81aa8f8bf6d77c15`;
- production manifest
  `7a340ea912667450c4405bcf6a400c25514ca18e3265e2ac784ee49705ebd1ae`.

**Documentation impact: none.** The change is test-only diagnostics.
`src/app/AGENTS.md` already requires a waiting stage to name its condition,
and the terms are recorded here, next to the negative controls that print
them.

### Acceptance state

- **No phase can reach `RUN_BUDGET_EXPIRED` before naming why:** pass. Each
  of the eight waiting phases returns through the 5 s `wait`. The modality
  step bounds itself. Each break named its phase at 6.4–6.7 s, where HEAD
  expired at 40.6 s.
- **Each named term is shown to fire:** pass on Linux. There are nine breaks,
  one per term. Break 5 is an equivalent forced check, for the reason given
  above.
- **The row still passes on macOS and Linux:** Linux passes. **macOS is
  pending.**

### Still needed: the macOS run (Mac integrator)

On macOS, with `build/dev` at the BPR-0864 commit or later, one serial run:

```sh
ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_workbench_composition_contract|application_review_round_trip_composition_contract)$'
```

Observe:

- both rows pass;
- nothing ending in `_STALLED` appears, and neither does `COURSE_MODALITY_*`
  or `RUN_BUDGET_EXPIRED`;
- the row's time and the load average. The 5 s phase bound is the one
  assumption a slow Mac could test. BPR-0860 recorded this row at about 21 s
  at load 2.2–4.5. A spurious stall prints `waited_ms=` against
  `wait_cap_ms=5000`, with the phase's own counters.

If both hold, the card can finish. Windows is not part of this acceptance. The
row is registered `WINDOWS_UNCONTAINED` and was last red there on the separate
`cleanup_failed=1` fault that BPR-0860 recorded.
