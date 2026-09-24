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

## One shared bounded wait for all five probes (bpr-main-mac, Mac batch 5, 2026-09-23)

Station `bpr-main-mac` (Apple Silicon, AppleClang, Qt 6.11.2, `build/dev`
Debug, offscreen), claimed at `5e73f83e`. [[BPR-1036]] is still Ready and
unclaimed, so this card provides the helper. The UIforETW probe's own caps
are left to BPR-1036.

### Re-check at HEAD: fewer waits than phases

The counts of `pending` returns hold: 19 in the appearance theme probe (the
card says 18), 7 in the review ledger, 10 in the round trip and 14 in the
defect probe. But most phases do not wait. They check their condition once
and fail at once, or they change phase and return `pending` for one tick.
The waits, where a phase returns `pending` without changing phase, are
these:

| Probe | Waits (term) | Phases that check once or only change phase |
| --- | --- | --- |
| appearance theme | the readiness gate at the top, which runs in every phase and before the probe state existed (`APPEARANCE_THEME_COMPARISON_STALLED`); `awaiting_stable_comparison` (`…_STABLE_COMPARISON_STALLED`); `awaiting_first_preview` (`…_FIRST_PREVIEW_STALLED`); `awaiting_second_preview` (`…_SECOND_PREVIEW_STALLED`); `awaiting_store` (`…_STORE_STALLED`) | manual preview, both adoptions, both restorations, stored preview, equal stored preview and commit, export, export refusal |
| review ledger | `awaiting_observation` (`REVIEW_LEDGER_OBSERVATION_STALLED`), `awaiting_diff` (`…_SELECTED_DIFF_STALLED`), `awaiting_actionability` (`…_ACTIONABILITY_STALLED`), `awaiting_material_actionability` (`…_MATERIAL_ACTIONABILITY_STALLED`) | none |
| round trip | two gates before the editor's draft exists (`REVIEW_ROUND_TRIP_DRAFT_STALLED`); `awaiting_editor`'s two selection waits (`…_EDITOR_SELECTION_STALLED`); `awaiting_lost_effect` (`…_LOST_EFFECT_STALLED`); the publication gate in `awaiting_deferred_terminal` and `awaiting_retry` (`…_DEFERRED_TERMINAL_STALLED`, `…_RETRY_STALLED`) | `awaiting_stale_refusal` |
| defect | the composition gate (`DEFECT_COMPOSITION_STALLED`); the reopen course (`…_REOPEN_STALLED`); the conversations gate in four phases (`…_CONVERSATIONS_STALLED`); `awaiting_ready` (`…_READY_STALLED`); `awaiting_diff` (`…_SELECTED_DIFF_STALLED`); `awaiting_reverse_right`'s two waits (`…_REVERSE_RIGHT_STALLED`); `awaiting_epoch_change`'s two waits (`…_EPOCH_CHANGE_STALLED`); `awaiting_shared_ready` (`…_SHARED_READY_STALLED`) | none |

That makes 22 terms. The theme's readiness gate had the widest hole. It
returned `pending` in every phase, even before the probe had state to hold
a clock.

### The helper

In the fixture-access region of `src/app/src/github_application.cpp`
(~15330-15397), inside `BPR_BUILD_TESTING`:

- `detail::fixture_probe_wait` (`github_application_fixture_access.hpp`,
  beside the modality step) is the clock and count. Each probe's state owns
  one, `Wait`.
- `keep_waiting(application, wait, term, facts)` starts the clock at a
  phase's first wait and counts calls. After `fixture_probe_wait_cap` (5 s of
  wall clock) and `fixture_probe_wait_floor` (16 calls) it prints
  `TERM:waited_ms=… wait_cap_ms=5000 waits=… <facts>`, ending with
  [[BPR-0863]]'s `course_modality_facts`, and answers `false`.
- `enter_phase(phase, wait, next)` is the only thing that clears the clock,
  so a wait cannot restart itself on a tick ([[BPR-0859]]).

Each probe binds them in two small lambdas, as BPR-0864 did:
`wait(term, facts)` returns `pending` or the probe's own `fail()`, and
`advance(phase)` enters the phase and returns `pending`. The workbench probe
moved onto the same helper. Its `stall`, `wait` and `advance` lambdas and its
`Waits` and `WaitingSince` members are gone, and its stall line is
byte-for-byte the same format. Every phase change in the five probes now
goes through `enter_phase`. Where the old code set the phase before a side
effect (an import request, a click), the call stays in the same place.

Each term prints the inputs to its own condition. The theme gate prints
`phase=` and which of the comparison, its context, selection and spans is
present, and the appearance host's outstanding requests. The stable
comparison prints `unchanged=` as 11 bits, in the order
`APPEARANCE_THEME_PROBE_IDENTITY_FACTS` uses. The previews print
`previewing`, the panel preview, both generations and the panel's status
text. The store prints persisted against expected, `stored` and the preview
state. The ledger terms print the snapshot and host facts and the ledger's
counters. The round-trip terms print the draft's state (through
`core::name`), the decision gate, and the mutation host's queue and
terminals. The defect terms print their components, the three
conversations, the epoch core's status (as its number, since
`epoch_status` has no name function) against the rows asked for, and the
comparison digests. All of them are in the negative controls below.

Two changes beyond moving to the helper:

- **The theme probe gained a first phase, `awaiting_comparison`.** The state
  now exists from the first call, so the readiness gate can wait on its
  clock. The baselines the state used to be built with are assigned in that
  phase, at the same moment: the first call on which the comparison is
  ready. The values are the same.
- **A failed round trip releases the decision gate.** The first run of the
  lost-effect break stalled by name at 5.0 s and then hung. The gate was
  still holding the lost publication, so shutdown waited for it. The run
  budget does not apply during shutdown, so the run ended at the contract's
  70 s timeout with the verdict unprinted. That timeout killed only the
  `cmake -E env` wrapper. The application was orphaned and kept running
  until this station stopped it by hand 48 minutes later. The round trip's `fail()` now calls
  `release()`, which is idempotent and posts whatever it held. The same
  break then ended in 17 s with `PUBLIC_PATH_FAILED` and
  `REVIEW_ROUND_TRIP_COMPOSITION_FAILED`. This also covers the phase's
  existing immediate failures, which would have hung the same way.

Every existing assertion, marker and marker order is unchanged, and so is
every failure that was already immediate. The contracts' source scans still
hold. The defect probe contains none of its forbidden calls, and the
workbench probe does not mention `request_fixture_diff`.

### Negative controls (AC2): one forced break per term

All breaks were in one temporary diagnostic build of the candidate. A
`bpr1037_break(term)` check, selected by `BPR1037_BREAK=<term>`, was added
with `||` to the condition that controls each wait, so the phase takes its
own waiting branch with its real facts. These are equivalent forced checks,
which AC2 allows. Each run was the row's own ctest command with the
variable set. The round-trip row checks its pins against the source it
reads, so it ran the same command with its source arguments pointed at a
pristine copy of the candidate. The diagnostic edits were then removed, and
the file matches the candidate hash (`grep -c bpr1037_break` gives 0).
**Every term named itself at 5.00-5.01 s. No run printed
`RUN_BUDGET_EXPIRED`, and none printed a different term.**

| Term | waited_ms, waits | The facts that name the cause (abridged; each line ends with the modality tail) |
| --- | --- | --- |
| `APPEARANCE_THEME_COMPARISON_STALLED` | 5000, 470 | `phase=awaiting_comparison comparison_result=present … right_spans=present appearance_outstanding=0` |
| `APPEARANCE_THEME_STABLE_COMPARISON_STALLED` | 5006, 473 | `unchanged=11111111111` |
| `APPEARANCE_THEME_FIRST_PREVIEW_STALLED` | 5007, 464 | `previewing=true panel_preview=present profile_generation=5 baseline_generation=4 status="Imported theme is previewed into Light only; Keep changes saves it."` |
| `APPEARANCE_THEME_SECOND_PREVIEW_STALLED` | 5003, 468 | the same, `profile_generation=7` |
| `APPEARANCE_THEME_STORE_STALLED` | 5007, 470 | `persisted_settings=1 expected_persisted_settings=1 stored=true previewing=false panel_preview=absent` |
| `REVIEW_LEDGER_OBSERVATION_STALLED` | 5010, 465 | `snapshot=present snapshot_files=1 window_read_quiescent=true actionable=1 navigation=present ledger_commands=0 …` |
| `REVIEW_LEDGER_SELECTED_DIFF_STALLED` | 5008, 466 | `file_selections=1 selected_diff_begins=1 selected_diff_publications=1 selected_diff_failures=0 last_selected_diff=present comparison_result=present …` |
| `REVIEW_LEDGER_ACTIONABILITY_STALLED` | 5005, 466 | `actionable=0 host_selected=present ledger_commands=7 ledger_persistence_requests=7 ledger_completions=7 …` |
| `REVIEW_LEDGER_MATERIAL_ACTIONABILITY_STALLED` | 5006, 465 | `actionable=1 host_selected=present receipt_material_reopened=true ledger_persistence_requests=8 …` |
| `REVIEW_ROUND_TRIP_DRAFT_STALLED` | 5006, 470 | `phase=awaiting_editor … review_draft=drafting … file_rows=1 current_row=0 selected_diff_publications=1` |
| `REVIEW_ROUND_TRIP_EDITOR_SELECTION_STALLED` | 5001, 469 | `comparison=present selection=present selection_side=left right_pane=present shown_draft=present` |
| `REVIEW_ROUND_TRIP_LOST_EFFECT_STALLED` | 5007, 474 | `lost_result=true stored_comments=1 submissions=1 decision_gate=present gate_pending=1 review_draft=publishing pending_mutation=present mutation_outstanding=1` |
| `REVIEW_ROUND_TRIP_DEFERRED_TERMINAL_STALLED` | 5001, 477 | `phase=awaiting_deferred_terminal mutation_outstanding=0 review_draft=drafting effects_queued=2 terminals_published=2 …` |
| `REVIEW_ROUND_TRIP_RETRY_STALLED` | 5008, 476 | `phase=awaiting_retry review_draft=published effects_queued=3 terminals_published=3 submissions=2 stored_comments=1` |
| `DEFECT_COMPOSITION_STALLED` | 5001, 477 | `phase=awaiting_ready fixture=present … defect_card=present` |
| `DEFECT_REOPEN_STALLED` | 5005, 475 | `durable_defects=2 first=present second=present ledger_private_loads=10 ledger_private_writes=0 shown_defects=2` (the reopen run, after the seed run passed) |
| `DEFECT_CONVERSATIONS_STALLED` | 5003, 475 | `phase=awaiting_ready first_conversation=present … conversation_rows=6` |
| `DEFECT_READY_STALLED` | 5004, 478 | `workbench_host=present actionable=1 files_view=present file_rows=1` |
| `DEFECT_SELECTED_DIFF_STALLED` | 5001, 474 | `selected_diff_publication=present comparison_result=present selected_diff_publications=1 …` |
| `DEFECT_REVERSE_RIGHT_STALLED` | 5007, 473 | `epoch_status=6 resolving=false left_row=1 expected_left_row=1 right_row=1 expected_right_row=0` |
| `DEFECT_EPOCH_CHANGE_STALLED` | 5010, 476 | `comparison_changed=true result_matches_publication=true selected_diff_publications=2 selected_diff_failures=1 refused_persistence_attempts=2 …` |
| `DEFECT_SHARED_READY_STALLED` | 5004, 477 | `review_editor=present shown_draft=present review_draft=present submissions=0` |
| `WORKBENCH_SELECTED_DIFF_STALLED` (the moved probe, one check) | 5004, 470 | the same facts BPR-0864 recorded for its break 3 |

Each run took 15.7-19.4 s from the ctest command to the red row. The
defect reopen run took 31.9 s, because the seed run goes first. The first
23 ran on the first candidate (`012bf14f…`). The gate release was added
afterwards, and the lost-effect and editor-selection breaks were run again
on the final candidate (`a2652cb3…`, the file's SHA-256). Both named
themselves at 5.0 s, and the lost-effect run then ended in 17 s, as above.

### The rows on the final candidate (AC3, AC4, macOS)

Serial, `ctest --test-dir build/dev -j1 -V -R …`, on `5fbf4a16` plus the
change:

| Row | HEAD (`5e73f83e`, before the change) | Candidate |
| --- | --- | --- |
| `application_workbench_composition_contract` | Pass 24.29 s | Pass 22.91 s |
| `application_review_ledger_composition_contract` | Pass 36.27 s | Pass 36.47 s |
| `application_review_round_trip_composition_contract` (resealed) | Pass 11.21 s (at `19e8ec19`, the same `src/app`) | Pass 11.77 s |
| `application_defect_composition_contract` | Pass 52.80 s | Pass 53.46 s |
| `application_appearance_theme_customization_contract` | Pass 28.22 s | Pass 25.33 s |
| `application_composition_contract` (builds and scans the tests-off Release graph) | — | Pass 232.20 s |
| `architecture_boundaries`, `conditional_branch_identifiers`, `conditional_branch_identifier_contract` | — | Pass |

The candidate run has no `_STALLED`, `COURSE_MODALITY_*` or
`RUN_BUDGET_EXPIRED` line. Load was 24.3 at the start and 7.3 at the end.

- clang-format 23.1.1 leaves both files unchanged. HEAD's file is stable
  under 23, so every changed hunk is this card's.
- `_coding-standard/scripts/tidy.py` on `github_application.cpp`: no finding
  on a changed line. The three it reports are older than this card:
  `exact_current` at line 8194, `home_view_state.hpp:35` and
  `dif_runtime.hpp:1`.

### Pinned-code review and reseal

The pins last matched at `8e080bbc` (BPR-0882). `git log 8e080bbc..HEAD --
src/app/` shows one other commit, `3727a762` (BPR-0902). It changed
`main.cpp`, which is in the production manifest, and did not reseal. So
**`application_review_round_trip_composition_contract` is red at
`5fbf4a16` on every platform**, at the manifest pin. BPR-0902's card says
the new line "is pinned by no row", which is true of its output but not of
the source digest. This reseal covers it:

- `3727a762`: `main.cpp` reads one more fixture counter
  (`RefreshRetrieveAttempts`) inside `BPR_BUILD_TESTING`, and prints
  `DEVICE_FLOW_REFRESH_RETRIEVES:` after the refresh course's verdict. It is
  diagnostic output only. No credential, provider, vault, transport or
  mutation path changes.
- This card: every edit is in the `BPR_BUILD_TESTING` probe states (~12760-13010),
  the fixture-access helper and the five probe functions, plus the helper
  type in the test-only fixture-access header. The probes read window, host,
  ledger, epoch and fixture state and write only to standard output. Every
  product action they take is unchanged: the file gestures, clicks,
  imports, publications, selector changes and stores. The one new action is
  `release()` on the fixture's decision gate when a round trip fails.

The mutation-branch pin is unchanged (`a7e2081e…d1e8a`). New values:

- full application
  `a2652cb333e767df38a883be27c9caeb3281a293b3f2ba38fe59244dbf050653`;
- production manifest
  `aab9148a88b57f9f29fd4fbb4f6c00b93a7903936617394c0a53f770113d497c`.

**Documentation impact: none.** `src/app/AGENTS.md` already requires a
waiting stage to name its condition. The terms are recorded here, with the
negative controls that print them.

### Acceptance state

- **AC1** (no waiting phase can reach `RUN_BUDGET_EXPIRED` before naming
  why): pass on macOS. All 22 waits go through `keep_waiting`, and each
  forced break named its term at 5.0 s.
- **AC2** (each term shown to fire): pass. There are 22 forced checks, plus
  one for the moved workbench probe. The diagnostic build was removed, as
  above.
- **AC3** (the four rows pass on Linux and macOS, the round-trip contract
  with the resealed pins, the workbench row too): **macOS passes; Linux is
  owed.**
- **AC4** (`application_composition_contract` passes): pass on macOS.

### Still needed: the Linux run (bpr-main-linux)

On `build/dev` at this card's commit or later, rebuilt, one serial run:

```sh
ctest --test-dir build/dev -j1 -V -R '^(application_workbench_composition_contract|application_review_ledger_composition_contract|application_review_round_trip_composition_contract|application_defect_composition_contract|application_appearance_theme_customization_contract)$'
```

Expected: 5/5, with no `_STALLED`, `COURSE_MODALITY_*` or
`RUN_BUDGET_EXPIRED` line. The round-trip row verifies the new pins. When
it passes, the card can finish. Windows is not part of this acceptance.
