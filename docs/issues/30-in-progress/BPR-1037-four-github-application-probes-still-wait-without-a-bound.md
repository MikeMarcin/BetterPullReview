---
schema: "implement-rfc.card.v1"
id: "BPR-1037"
title: "Four github_application.cpp probes (appearance theme, review ledger, round trip, defect) still wait without a bound and expire the run budget on a stage number"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0864's sibling-probe check","found_at":"3dca2d7a","rechecked_at":"4a4c1e40","related_cards":["BPR-0860","BPR-0863","BPR-0864","BPR-1036"],"reservation":"docs/issues/_reservations/BPR-1037--BPR-1037--aba615d53659.json"}
---

# BPR-1037: Four github_application.cpp probes (appearance theme, review ledger, round trip, defect) still wait without a bound and expire the run budget on a stage number

## Defect

[[BPR-0860]] and [[BPR-0864]] made every waiting phase of the workbench
composition probe bounded and named. Before that change, a phase whose effect
never arrived ran until the run budget expired and reported
`RUN_BUDGET_EXPIRED … stage=2` and nothing else. BPR-0864's broken selected-diff
run shows the cost. Against the probe as it was before BPR-0864, it expired at
40.56 s with every later marker FAILED. With the bounded wait, the same fault
named itself in 6.6 s
(`WORKBENCH_SELECTED_DIFF_STALLED`, with the counters it waited on).

BPR-0864 checked the sibling probes in `src/app/src/github_application.cpp`.
Four of them have exactly the defect it fixed. Every waiting phase returns
`pending` with no bound and no term. Rechecked at `4a4c1e40`:

| Probe | Row, budget | Phases that wait | `pending` returns |
|---|---|---|---|
| `exercise_appearance_theme_customization` | `application_appearance_theme_customization_contract`, 50 s | `awaiting_stable_comparison`, `…_manual_preview`, `…_preview_adoption`, `…_preview_restoration`, `…_first_preview`, `…_second_preview`, `…_store`, `…_stored_preview`, `…_stored_preview_adoption`, `…_stored_preview_restoration`, `…_equal_stored_preview`, `…_equal_stored_commit`, `…_export`, `…_export_refusal` | 18 |
| `exercise_review_ledger_composition` | `application_review_ledger_composition_contract`, 40 s | `awaiting_observation`, `…_diff`, `…_actionability`, `…_material_actionability` | 7 |
| `exercise_review_round_trip_composition` | `application_review_round_trip_composition_contract`, 40 s | `awaiting_editor`, `…_stale_refusal`, `…_lost_effect`, `…_deferred_terminal`, `…_retry` | 10 |
| `exercise_defect_composition` | `application_defect_composition_contract`, 40 s | `awaiting_ready`, `…_diff`, `…_reverse_right`, `…_epoch_change`, `…_shared_ready` | 14 |

None of them names its condition when the budget expires. The review ledger,
round trip and defect courses print their evidence bits afterwards. Those say
which evidence was reached, but not which condition the phase was waiting on.
This is diagnosability debt, not a live failure. All four rows pass on Linux
at `4a4c1e40`: review ledger 4.78 s, round trip 1.62 s, defect 6.25 s and
appearance theme 5.53 s (`build/r2-logs/bpr1037-four-rows-head-4a4c1e40.log`
on bpr-main-linux, not committed).

`src/app/AGENTS.md` asks for exactly this: "a stage that waits must record
what it is waiting for so an expiry names the condition rather than the
stage".

## Scope

- Give each waiting phase of the four probes a bounded wait and a named term.
  The term prints the inputs that phase's condition reads, the way
  `WORKBENCH_*_STALLED` prints its counters, host state and expectations.
- Reuse BPR-0864's shape, not a new mechanism:
  - `wait(term, facts)` keeps the phase pending until the bound, then stalls
    by name;
  - `advance(phase)` is the only thing that restarts the clock, so a wait
    cannot reset itself on a tick ([[BPR-0859]]'s trap);
  - the bound is 5 s of wall clock with a floor of 16 calls;
  - the line ends with [[BPR-0863]]'s `course_modality_facts`.

  Today those are lambdas inside `exercise_workbench_composition`. Five
  copies of them would be the second mechanism BPR-0864 avoided. Extract them
  once into the fixture-access region as a small helper that each probe's
  state owns (clock, count), and move the workbench probe onto it in the same
  change. [[BPR-1036]] folds in the same work for the UIforETW probe. Whichever
  card lands first provides the helper.
- Keep every existing assertion, marker and marker order, and every failure
  that is already immediate.
- Check the bound against each row's budget. The appearance theme course
  runs 14 phases in a 50 s budget, and the bound only has to fire before the
  budget does.
- `src/app/src/github_application.cpp` is pinned by the round-trip contract.
  Reseal with a recorded review, and run `application_composition_contract`
  so the new terms stay out of the tests-off Release graph.

## Acceptance

- AC1 No waiting phase of the four probes can reach `RUN_BUDGET_EXPIRED`
  before naming why. Each phase stalls with its own term and facts.
- AC2 Each new term is shown to fire, by one deliberately broken run per
  term, or by an equivalent forced check recorded per term. One diagnostic
  build selecting the break by environment, as BPR-0864 did, is enough. Remove
  it afterwards and show that nothing of it remains.
- AC3 The four rows still pass on Linux and macOS, and the round-trip
  contract passes with the resealed pins. `application_workbench_composition_contract`
  still passes if its probe moves onto the shared helper.
- AC4 `application_composition_contract` passes.

Windows is not part of this acceptance. The change is portable test code,
and this card does not change any row's Windows registration.
