---
schema: "implement-rfc.card.v1"
id: "BPR-1094"
title: "Measure counter costs and verify the actual compact native UI"
status: "Parked"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0050", "RFC-BPR-0051"]
depends_on: ["BPR-1083", "BPR-1089", "BPR-1092"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-20","milestone":"verification","acceptance":["R29","R31","R32","R33","R44"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"tests/application/feedback_counter_cost_tests.cpp","command":"ctest --test-dir build/app-release-tests -j1 -R '^(application_feedback_counter_costs|application_large_review_responsiveness|application_first_tab_comparison)$'","detail":"i9-10900KF 10C/20T, 62 GiB, WSL2 Ubuntu 26.04, GCC 15.2.0 Release -O3 (and Debug). 1k/10k synthetic conversations within github_read_limits, 0/3/12 chips, both modes: work (parsed, scopes, units), C++ allocations and medians recorded on the card, e.g. 10k twelve chips 3.2 ms / 33,940 allocs, 10k root edit 92.6 ms / 1 parse / 5 scopes; composed large-review-v1 chip changes 25-28 ms. No threshold invented; the 100 ms input rows pass."}},{"criterion":"AC2","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"tests/end_to_end/setup_destination_public_path_tests.cpp","command":"ctest --test-dir build/dev -R '^(application_feedback_counter_costs|application_overview_feedback_public_path|application_large_review_responsiveness)$'","detail":"Chip and mode changes: 0 provider reads, 0 observations, 0 parses, 0 units tallied (application row at 1k/10k; composed course; 23 Settings changes over 1,000 threads). Root edit: 1 body parsed, exactly PR + file + folders re-tallied and changed (5 scopes synthetic, 4 composed); reply edit 1 parse, no count change; resolve 0 parses. Controls C1-C8 fail."}},{"criterion":"AC3","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"tests/adapters_qt/pull_request_overview_page_tests.cpp","command":"ctest --test-dir build/dev -R '^(qt_pull_request_overview_page|qt_pull_request_overview_chips_scale_125|qt_pull_request_overview_chips_scale_200|application_overview_labels_public_path)$'","detail":"Offscreen: 4 built-in themes x Icons/Letters x 100/125/200%: distinct marks through feedback_mark_theme, accessible names, ? and Open/All, whole 1-3 digit counts, +n named in full, Tab ring/Right/Enter, focus and scroll kept across refresh, End/wheel scroll. Composed overview-labels-v1 captures at 1.0/1.25/2.0 inspected. Native visible window not used (owed under R33). Finding: Files heading clipped at a 1,000 px page."}},{"criterion":"R29","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"tests/application/feedback_counter_cost_tests.cpp","detail":"This card's share: projections and sorts read kept tallies only (0 parses, 0 units), unchanged bodies reused, bounded re-tally sets, no per-label fetch (composed provider reads 0). Recorded limit: each observation is linear in the snapshot (10k about 91 ms, 52-56 MB allocated, even with 0-1 parses)."}},{"criterion":"R31","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"tests/adapters_qt/pull_request_overview_page_tests.cpp","detail":"This card's share: all twelve chip marks distinct and theme-styled (Icons and Letters) in four themes at 100/125/200% offscreen, each named in accessible text and the menu. The owner accepted the look in BPR-1089."}},{"criterion":"R32","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"tests/adapters_qt/feedback_label_threads_tests.cpp","command":"ctest --test-dir build/dev -R '^(qt_feedback_label_threads|application_label_authoring_contract|core_feedback_private_promotion|core_feedback_suggestion_preservation)$'","detail":"Reused unchanged evidence (BPR-1083/1084/1086), rerun here at this revision; the Overview chips added here change no body: chip changes parse nothing and publish counts only."}},{"criterion":"R33","outcome":"pending","revision":"269f7ccff","evidence":{"kind":"observation","detail":"Offscreen and composed renders pass at 100/125/200% in four themes with pointer and keyboard. Pending the native visible-window look (125/200% on a real display, larger text, VoiceOver/Narrator, native style), owed to the owner or the Mac station; never launched on the desktop here."}},{"criterion":"R44","outcome":"pass","revision":"269f7ccff","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","detail":"Inherited large-review-v1 (515 files, 1,000 threads, composed) and a synthetic 1k/10k core workload at the read model's own ceiling, both builds, hardware and build recorded; timings reported, never turned into a guarantee."}}]
parked_on: "human-review"
---

# BPR-1094: Measure counter costs and verify the actual compact native UI

Pack alias: **PACK-CC-20** (live `BPR-1094`, allocated at import).

## Outcome and work

Measure the inherited large-review workload and a justified larger synthetic core workload, including 0/3/12 columns and 1k/10k normalized conversations where admitted. Inspect actual composed native states at selected scales/themes/input modes; reuse unchanged coverage.

## Acceptance

- Record hardware/build/workload and observed work/allocation/latency, not invented universal thresholds.
- A column toggle makes no network request or per-body parse; a root edit updates bounded relevant state.
- Rendered application evidence demonstrates label identity, state/coverage clarity, full content and usable keyboard/scroll behavior.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Measured and verified on Linux (bpr-main-linux, 2026-10-08)

Product commit `269f7ccff` (built and run as `af568b324` on `a5cba14fb`; the landing rebase over `d38b72965` brought only an ID allocation, and `git range-diff` shows the patch identical). Engineering owner
`rfc-implementer` (`claude-opus-5-5`, effort high). No live GitHub, no model
call. The frontend-design skill is not installed on this station; this card
adds no visual design, only tests and evidence counters.

**Machine and builds.** Intel Core i9-10900KF at 3.70 GHz, 10 cores and 20
threads, 62 GiB RAM; WSL2 (kernel 6.18.33.2) on Ubuntu 26.04 LTS; GCC 15.2.0;
Qt 6.11.2, offscreen, Windows style. Debug `-Werror` in `build/dev`. The
optimized tree was `app-release` with `BUILD_TESTING=ON` (`-O3 -Werror`,
Release), built for this card's rows, run with the load average under 1.0 and
`ctest -j1`, then deleted. Times are this machine's; nothing below is a
threshold.

**What was added.**

- `feedback_counter_work` on `feedback_counter_model`: observations, bodies
  parsed and reused, units compared, scopes re-tallied by an observation,
  scopes tallied on request (a listed file with no conversation), and units
  handed to the core. The window also counts its own observations and parses
  (so a model made afresh would still show), and `scaling_evidence` reports
  both with the Overview count publications.
- `application_feedback_counter_costs` (new, contained, serial): a synthetic
  pull request of 1,000 and of 10,000 native threads (root and one reply each,
  one in a hundred not on a file, one in three resolved, every label, plain,
  an unknown label word, a bold label, every fiftieth root 16 KiB), on 500 and
  2,500 files three folders deep. The workload is checked against
  `github_read_limits`: 10,000 review threads, 3,000 files, 50,000 comments and
  16 MiB of text are the read model's ceiling, which is why 10k is the larger
  workload. A scoped counting `operator new` counts the C++ allocations.
- `application_overview_feedback_public_path` (composed): Settings' chips,
  drill-downs and a read receipt parse no body and tally no conversation; a
  website resolve and a root edit each re-tally exactly four scopes.
- `application_large_review_responsiveness` (composed, the inherited 515
  files and 1,000 threads): three Settings chip changes, to 3, 12 and 0, each
  one input against the existing 100 ms target, with no provider read, no
  observation, no parse and no conversation tallied.
- `qt_pull_request_overview_page` runs a chip matrix in the four built-in
  appearance profiles, with Icons and Letters; the new
  `qt_pull_request_overview_chips_scale_125` and `_200` rows run it at their
  scale. Both are contained, so no Windows manifest entry is needed.

### Measurements

Optimized tree, median of the runs (5 for a first observation, 11 for an
observation, 21 for a projection). Alloc is C++ `new` calls and bytes of one
run. Parsed is bodies parsed; scopes is scopes re-tallied (or tallied on
request for a projection); units is units handed to the core.

| Phase | 1k (2,015 posts, 622 scopes) | 10k (20,015 posts, 2,602 scopes) |
|---|---|---|
| First observation | 8.5 ms; 45,909 / 5.4 MB; parsed 2,015; scopes 622; units 5,000 | 90.9 ms; 415,215 / 59.3 MB; parsed 20,015; scopes 2,602; units 49,730 |
| Unchanged snapshot | 8.2 ms; 37,626 / 4.5 MB; parsed 0; scopes 1 | 91.1 ms; 360,280 / 52.5 MB; parsed 0; scopes 1 |
| No chips, Open / All | 0.44 / 0.44 ms; 4,710 / 1.1 MB; parsed 0; units 0 | 2.6 / 2.7 ms; 20,930 / 5.4 MB; parsed 0; units 0 |
| Three chips, Open / All | 0.49 / 0.50 ms; 6,516 / 1.3 MB; parsed 0; units 0 | 3.1 / 2.9 ms; 28,736 / 6.3 MB; parsed 0; units 0 |
| Twelve chips, Open / All | 0.55 / 0.55 ms; 7,720 / 2.0 MB; parsed 0; units 0 | 3.2 / 3.1 ms; 33,940 / 9.3 MB; parsed 0; units 0 |
| Twelve chips, sorted | 1.3 ms; 8,324 / 2.1 MB | 6.8 ms; 36,544 / 9.5 MB |
| One chip's drill-down | 4 µs; 10 / 507 B | 22 µs; 10 / 507 B |
| Root label edit | 9.4 ms; 39,760 / 4.7 MB; parsed 1; scopes 5; units 2,057 | 92.6 ms; 381,149 / 56.2 MB; parsed 1; scopes 5; units 20,419 |
| Reply label edit | 7.7 ms; parsed 1; scopes 1; no count changes | 90.4 ms; parsed 1; scopes 1 |
| Resolve or reopen | 8.0 ms; parsed 0; scopes 5 | 90.6 ms; parsed 0; scopes 5 |

A projection over 602 rows (1k) or 2,602 rows (10k) tallied 5 or 25 listed
files with no conversation on request, each with no unit. Debug is about ten
times slower (first observation 93 ms and 992 ms; twelve chips 6.4 ms and
31 ms) with the same counts.

Composed, the inherited `large-review-v1` (515 files, 1,000 REST review
comments, unlabelled, so each is Other with unknown resolution):

| Phase (one input) | Release, ms (largest input gap) | Debug, ms |
|---|---|---|
| `overview_counts_all` / `_open` | 5.0 / 5.9 | 9.5 / 9.4 |
| `overview_sort_by_comments` / `overview_file_order` | 6.0 / 1.2 | 11.9 / 8.1 |
| `overview_columns_3` (Settings tick) | 25.0 (24.5) | 43.1 |
| `overview_columns_12` | 28.2 (27.7) | 23.4 |
| `overview_columns_0` | 26.8 (26.3) | 24.0 |

Across the 23 chip changes: 0 provider reads, 0 observations, 0 bodies parsed,
0 scopes re-tallied, 0 units tallied, 24 count publications, and 96 on-request
tallies of the four listed files with no conversation. The chip phases include
the profile's settings round trip. `paged_review_and_git_load`'s largest input
gap was 61 ms in Release. `application_first_tab_comparison` passes; no input
budget failure.

**Findings.**

1. *Every observation is linear in the snapshot.* A root edit parses one body
   and re-tallies five scopes, but at 10k it still takes about as long as the
   first observation (92.6 against 90.9 ms) and allocates 56 MB, about seven
   times the 8 MB of text. The per-snapshot pass copies every body into the
   adapter's posts, rebuilds the annotation set post by post (reusing each
   annotation), rebuilds the population, compares every unit and re-tallies
   the pull request scope over every unit. It runs on the window's thread in
   `publish_overview`. At 1k it is 8 to 9 ms. The parse saving is real; the
   remaining cost is the observation, not the tallies. This is a fact for a
   follow-up if 10k-conversation pull requests matter, not a failure.
2. *The Files heading is clipped at a narrow Overview.* With the page at
   1,000 px (its files column about 680 px) and the ? chip showing, the
   heading reads "…2 not rea" under the Open button, with no ellipsis and no
   full text in its accessible name. The composed 1,400 px window showed its
   shorter heading ("Files: 17 in this pull request, 17 not read") whole at
   100%, 125% and 200%. The chips themselves are whole (below).
3. One Settings chip change published the counts twice (24 for 23); counts
   and focus were unaffected. Not investigated further.

### Structural results

- **A column toggle makes no network request and no per-body parse.**
  Application row: every chip set and mode, and a sort, at 1k and 10k:
  observations 0, parsed 0, re-tallied 0, units tallied 0, generation
  unchanged. Composed: Settings' two chips, the pointer and keyboard
  drill-downs and a read receipt read nothing from the provider, observe
  nothing, parse nothing and tally no conversation; the large-review course's
  23 chip changes the same, with `AccountReadCount` unchanged.
- **A root edit updates bounded, relevant state.** Application row: a root
  label edit parses exactly that body (20,014 of 20,015 reused at 10k) and
  re-tallies exactly the pull request, the file and its three folders, which
  are exactly what changed; a reply's label parses one body and changes no
  count; a resolve parses none and re-tallies the same five scopes; an
  unchanged snapshot parses none and re-tallies only the pull request.
  Composed: a website resolve is one observation, 0 parsed, 4 scopes; a
  website root edit is one observation, 1 parsed, 14 reused, 4 scopes.

### Rendered evidence

Offscreen only, never on the desktop. Renders went to the session's scratch
directory and are not committed.

- **Qt chip matrix** (`qt_pull_request_overview_page` at 100%, the new rows at
  125% and 200%), in light, dark, high-contrast light and high-contrast dark,
  each with Icons and Letters (24 looks):
  - *Label identity*: issue, question and nitpick chips paint three different
    marks; each of the twelve chip marks is the label's mark in the page's
    `feedback_mark_theme` and all twelve differ; Letters paints the issue chip
    differently from Icons in every theme; the cell's accessible text names
    each label and count.
  - *State and coverage*: Open is checked with its mark and an accessible name,
    All is offered; a file with only an unknown-state issue shows "? 1" with
    "whose state is unknown" in words; the ? chip beside the switch counts it;
    All reads "issue 192 (128 open, 64 resolved), question 12 (12 open),
    nitpick 1 (1 open), Other 3 (3 open) conversations".
  - *Full content*: 1-, 2- and 3-digit counts (1, 12, 128, 192) each sit whole
    in their chip, every chip inside its cell, none overlapping; all twelve and
    Other fold into "+n" with none lost, and the cell's accessible text and its
    menu name every label in full.
  - *Keyboard and scroll*: Tab steps onto the issue chip with its ring, Right
    and Enter list exactly that file's questions, a refresh keeps the cell and
    the chip; End scrolls the last row fully into view, Tab reaches its chip
    there, a refresh keeps the scroll position, and the wheel scrolls without
    asking the host anything.
- **Composed** (`application_overview_labels_public_path`, the owner's
  `overview-labels-v1`, run with `BPR_SETUP_ARTIFACT_ROOT` at
  `QT_SCALE_FACTOR` 1.0, 1.25 and 2.0): all pass; the open, all, keyboard
  (Tab ring and tooltip), Label key, Discuss caption and Settings captures in
  the four themes were inspected at each scale. Chips, counts, ? marks and
  the ring are whole and legible; the heading fits at 1,400 px.

### Failing-before controls

Each was applied, built, run and reverted (`build/dev`).

| # | Mutation | Fails at |
|---|---|---|
| C1 | `tally_of` ignores the kept tally | `application_feedback_counter_costs`: "1k: a chip or mode change parses no body and tallies no conversation" |
| C2 | No kept annotations (`current` always empty) | same row: "1k: an unchanged snapshot parses nothing and tallies only the pull request again" |
| C3 | Every observation re-tallies every scope | same row, same check |
| C4 | A changed file's folders are not re-tallied | same row: "1k: a root edit tallies again exactly the pull request, the file and its three folders, and they are what changed" |
| C5 | A chip request re-observes the snapshot | `application_overview_feedback_public_path`: "ticking two chips in Settings parses no body and tallies no conversation" |
| C6 | the same | `application_large_review_responsiveness`: "chip changes read nothing from the provider, parse no body and tally no conversation" |
| C7 | A chip request makes a fresh model and observes into it | `application_overview_feedback_public_path`: the same check, through the window's own counts (the model's record restarts with the model, so before the window counted, the large-review course missed this) |
| C8 | the same | `application_large_review_responsiveness`: the same check |
| C9 | Chips paint marks without the theme's style | `qt_pull_request_overview_chips_scale_125`: "light: Letters draws the issue chip differently from Icons" |
| C10 | A chip's count width is a fixed 10 px | `qt_pull_request_overview_chips_scale_200`: "light icons at 200%: a chip is wide enough for all its digits" |
| C11 | A counts refresh scrolls the files to the top | `qt_pull_request_overview_page`: "light icons at 100%: a refresh keeps the scroll position and the chip" |

### Regressions and gates

- `build/dev` (Debug `-Werror`), at `af568b324` (landed as `269f7ccff`): 30 of 30 focused rows
  (`core_feedback_*`, `application_feedback_counters`,
  `application_overview_feedback`, the new cost row, both overview public
  paths, the three Qt overview rows, `qt_feedback_label_*`,
  `application_label_authoring_contract`, the review submission, setup
  destination and nested folders courses, `application_active_changed_scaling`,
  `application_review_round_trip_composition_contract`,
  `test_network_isolation_manifest` and `_structure_contract`); then
  `architecture_boundaries`, `architecture_boundary_contract` (219 s),
  `application_large_review_benchmark`, `application_first_tab_comparison`
  and `application_large_review_responsiveness`.
- `core-only`: 13 of 13 (`type_read_binding_contract`,
  `core_dependency_boundary`, `core_boundary_contract`, `core_feedback_*`).
- Optimized tree: the cost row, both 100 ms rows, the counter, overview,
  public-path and three Qt overview rows pass. GCC 15 at `-O3` reported
  `mismatched-new-delete` on the cost row's replaced `operator delete`
  (inlined after the replaced `operator new`, which allocates with `malloc`);
  it is suppressed around those definitions only.
- `/usr/bin/clang-format` 22.1.8 is clean on every changed file.
  `coding-format-check` fails on four files this card did not touch, all from
  other cards on `main` (`anchored_tooltip.cpp`, `workbench_window_tests.cpp`,
  `workbench_interaction_tests.cpp`, `change_arrows_across_files_public_path_tests.cpp`).
- **Pins.** origin/main's pins matched at `a5cba14fb` in a detached worktree.
  The full-application and production-manifest pins were resealed (dated
  "BPR-1094 reseal" comment); the mutation-branch pin did not move
  (`e067befe…a5df`).

### Limits

- **Native visible look owed.** Nothing here was looked at in a visible
  window. R33's native matrix (125% and 200% on a real display, larger text,
  VoiceOver and Narrator, native style) is owed to the owner or the Mac
  station, as BPR-1089 handed it here. macOS and Windows runs of the new and
  changed rows are owed.
- **The inherited workload is unlabelled REST**, so the composed 1k course
  exercises only Other and unknown resolution; the labelled distribution is in
  the application row. There is no 10k composed fixture, so 10k is measured
  at the application layer only.
- The allocation counter sees unaligned C++ `new` only, not `malloc`, Qt's or
  aligned allocations. Times include the steady-clock read; no
  frame-presentation or OS input latency is observed.
- Findings 1 to 3 above are recorded, not fixed.

## Coordinator verification (bpr-main-linux, 2026-10-08)

At `125fb6219`, in `build/dev`, 27 of 27 pass: the counter, cost and overview
rows, the network-isolation rows, the round-trip contract and
`architecture_boundaries`. Parked for R33's native visible-window look (owner
or the Mac station). Findings 1 to 3 are recorded above and go to the
coordinator.

### Correction (2026-10-09, from BPR-1351)

Finding 3 miscounted. The large-review course makes 24 Settings chip changes
(12 ticks, then 12 unticks), not 23. The "23" came from the test's lower bound
`>= 2*12-1`. The 24 publications are exactly one per change, and there is no
double publication. [[BPR-1351]] now asserts one publication per change and
none after the last.
