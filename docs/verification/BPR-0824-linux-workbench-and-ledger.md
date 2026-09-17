# BPR-0824 Linux workbench position rule and review ledger reopen

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-3`), mandate r2, task `task-20260917-lnx-verify`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority
  `lane-commit`. Base `c19fa196`; BPR-0824 claimed at `4af7ed00`.
- **State.** Both fixes are committed and pushed: the ledger in `5875ce4f`,
  the workbench in `3eff1b10`. All four courses pass on Linux; see "Final run".
- **Card.** Parked on `human-review`. One question about the seam course is
  the owner's (see "Owner question"). The macOS run of all four courses is
  also still owed.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-verify/`
  (`ledger/`, `workbench/`, `step3-*.ctest.log`, `courses.summary`). None is
  committed.

## At the base

`c19fa196` was built stock: `cmake --preset dev --fresh`, then a
`--clean-first` build. It exited 0 with 1241 of 1241 steps and no diagnostic.
BPR-0818's drawer change is included. The Secret Service `login` collection
read `(<false>,)`.

The four courses still failed as the card says (`step3-0824-base.ctest.log`,
09:17:47Z):

- `qt_workbench_window_scale_100`, `_125` and `_200` failed at
  `workbench_window_tests.cpp:1531`.
- `application_review_ledger_composition_contract` passed its seed, then
  failed its reopen in 2.99 s. It printed
  `REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED`,
  `REVIEW_LEDGER_COMPOSITION_EVIDENCE:111111010011011` (expected
  `111111111111011`) and `REVIEW_LEDGER_PRESENT_ON_ENTRY:0`.

## The ledger reopen: a course race, not durability

**The receipt reached disk and the reopen loaded it.** An `rfc-worker` ran the
seed and reopen phases by hand in scratch roots (`ledger/`):

- **The failing bit.** Decoded against the probe's evidence order
  (`main.cpp:4632`), only bit 7, `ReceiptLoadedOnEntry`, really fails. Bit 9,
  `DurableReceipt`, requires bit 7. The actionability check after them is
  never reached. `REVIEW_LEDGER_PRESENT_ON_ENTRY` is a copy of bit 7.
- **The receipt was loaded.** Bit 6 is set, and it needs the carry-over write
  that only follows a successful load. A read-only copy of the database after
  the seed holds the receipt row. After the reopen the same row carries the
  carried-over material.
- **Negative control.** With the receipt row deleted after the seed, the reopen
  prints `111110000001011`, a different pattern.

**Mechanism.**

1. On reopen, `observe_selected_material` calls
   `review_ledger_application_host::observe_material`. It loads the seed's
   receipt (`ReceiptLoadHits` = 1), carries it over and commits it.
2. `observing_review_ledger_store::commit` notifies the profile runtime's ledger
   observer, which queues `refresh_shared_ledger`.
3. In the composing window, that calls `refresh_receipt`, which re-reads the same
   row (`ReceiptLoadHits` = 2). This re-read came in with `78d4fa5c` ("Share
   accepted private-ledger projections across review windows").
4. The probe runs on a 10 ms timer and required `ReceiptLoadHits == 1U`. If step
   3 ran first, the probe failed.

An lldb trace of a failing run shows the probe sampling
`ReceiptLoadHits=2, MaterialObservations=1, PersistenceRequests=1, Commands=0`
after `refresh_receipt` had run (`r4-reopen-lldb.log`).

**Why Linux.** This Qt 6.10.3 is built with glib, so the offscreen plugin uses
the glib event dispatcher, and that dispatcher usually runs the queued refresh
first.

| Dispatcher | Failing runs | Logs |
|---|---|---|
| glib (default) | 12 of 13 | `contrast-10x.log` and `c-glib-*` |
| Plain Unix (`QT_NO_GLIB=1`) | 2 of 13 | `contrast-10x.log` and `c-noglib-*` |

Every failure had the same bit string. macOS can therefore reach it as an
occasional flake; that is inferred, not observed.

**The fix (`5875ce4f`).**

- **Host.** `review_ledger_host_statistics` gains `MaterialReceiptHits`, the
  subset of `ReceiptLoadHits` that `observe_material` finds.
  `ReceiptLoadHits` keeps its meaning.
- **Probe.** It requires `MaterialObservations == 1U &&
  MaterialReceiptHits == 1U`.
- **Host test.** It asserts the new counter on a material hit. It also asserts
  that a `refresh_receipt` hit does not count as one.
- **RFC evidence.** The RFC-BPR-0007 verification row names the counter the
  course now owns.

**Why the claim is still honest.** The course still requires that the first
material observation after reopen finds the seed's receipt, and it still
requires the carried-over write. The re-read can no longer change the result in
either direction. A missing receipt still fails bit 7, as the negative control
shows.

Worker checks on the fix (`ledger/fix-*`):

- `application_review_ledger_host` passes.
- The course passes 20 of 20 with glib (119.88 s) and 20 of 20 with
  `QT_NO_GLIB=1` (89.10 s).
- Under lldb it passes in both forced orders, including the order that used
  to fail.
- The negative control still fails with bit 7 at 0.

## The workbench position rule

**Offscreen by design.** These courses register `QT_QPA_PLATFORM=offscreen`
and `QT_STYLE_OVERRIDE=Windows`. They are layout evidence, not native evidence.
Offscreen Linux resolves "Sans Serif" 9 pt to DejaVu Sans Book at 12 px,
96 DPI, the same at all three scales.

**The failing row.** `expect` exits on the first failure, and that failure was
the 960 px case at every scale. The course expected the identity hidden and the
positions shown. Linux showed the identity (`terminal  #20607`) and hid the
positions. The row pinned macOS geometry:

- macOS starts the strip with a 78 px inset for the window buttons;
- it sets text through Core Text, and its shortcut labels are the command glyph
  and a digit.

Linux has no inset, uses fontconfig, and labels the shortcuts "Ctrl+1". The
worker measured the window width at which the number still fits on Linux:

| Strip state | Number fits from |
|---|---|
| Full modality labels, positions shown | 1165 |
| Full modality labels, positions hidden | 1117 |
| Short modality labels, positions shown | 978 |
| Short modality labels, positions hidden | 930 |

At 960 on Linux the positions have yielded and the number still fits. That is
the declared order: title, owner, modality shortcuts, positions, and the
number last.

**Two defects behind it, on every platform (`workbench_window.cpp`,
`apply_strip_width`).**

1. **The result depended on the previous resize.**
   `Modality->set_compact(available < 1'100 || identityShort())` measured the
   segments in whatever form the last resize had left.
   - In a 1 px sweep between 1100 and 1165 the layout alternated on every
     resize.
   - Between 1100 and 1117 the number went while the positions stayed.
   - Linux exposes it because its full labels are 187 px wider than its
     short ones.
2. **The toolbar's size hint can lag `set_compact`.** Jumping from 1920 to
   any width from 978 to 1164 hid the positions wrongly: 187 mismatches against
   the pixel sweep. With an explicit layout invalidate there were none, at all
   three scales.

**The fix (`3eff1b10`).**

- **Product.** The modality form now comes from the width (`< 1100`). The
  segments compact only if the number is still short, and the toolbar layout
  is invalidated after each change. The 960 positions rule, the 1100
  threshold, `StripInset`, the home state and the identity yielding whole at
  the floor are unchanged.
- **Course.** It keeps the 1920 and 1280 rows, with their title, owner and
  tooltip checks, and the floor. The fixed 960 row is replaced by a sweep: 20
  px steps down to 1300, then 1 px. At every width it asserts:
  - no overflow;
  - a visible identity carries the number, and the identity yields once;
  - the shortcuts yield before the positions;
  - positions that yielded beside the number stay gone while it shows;
  - with the number gone, the positions follow their 960 rule, and they never
    show below 960.
  After the sweep, the widest width with the positions gone beside the number
  must be wider than the last width showing the number (977 and 930 on Linux).
  A single resize from 1920 to one pixel above that width must lay out both,
  which pins the invalidate.

**Why the claim is still honest on Linux.** The course asserts the declared
order at every width instead of at one width taken from macOS. The only
constant left is the product's own 960 rule. The independent review checked
that the assertions cannot pass vacuously on Linux or macOS. It raised one
risk: a platform whose strip is at least 18 px narrower would yield the
positions at their rule rather than for room. The final comparison was
therefore relaxed so that it does not require the yield above 960.

**Negative controls** (worker, `workbench/phase2-*`):

| Reversion | Scales that fail | Failing claim |
|---|---|---|
| Old state-dependent compaction | 100, 125, 200 | "the modality shortcuts yield before the positions do" |
| The fix without the invalidate | 100, 125, 200 | "one large resize lays out the positions beside the number as a gradual one does" |

**A masked third failure: the seam check.** With the strip fixed, the 1.0 and
2.0 runs stopped in `test_the_split_cursor_stays_over_a_resting_handle` with
"the seam is repainted when the pointer arrives on it and again when the
pointer leaves" and `WA_Hover=0`. The 1.25 run skips that check because it is
fractional.

- **Cause.** The style sets `Qt::WA_Hover` on the splitter handle. The basic
  Windows style, which `tests/adapters_qt/CMakeLists.txt:64` forces for
  `UNIX AND NOT APPLE`, does not. Without the attribute Qt sends the handle no
  hover events.
- **Not an offscreen limitation.** With the attribute set, or with Fusion on
  the handle, arrival and departure give 1 and 2 repaints, the counts BPR-0739
  recorded.
- **Not a product defect.** The handle draws its rule at one weight and never
  reads hover.
- **The candidate change.** Under a forced style (`QT_STYLE_OVERRIDE`
  non-empty), the check grants `WA_Hover` itself. It still measures the
  repaints and `underMouse()`.
- **Negative controls.** Without the grant, 100 and 200 fail. Forcing the style
  with `-style windows` and no environment variable still fails at 1.0.
- **What changes.** The premise "the style sets `WA_Hover`" is still asserted
  wherever the application runs its own style, which includes macOS. On the
  Linux lane it is no longer checked.

## Owner question

Under the headless Linux lane's forced basic Windows style, which of these
should `test_the_split_cursor_stays_over_a_resting_handle` do?

- **(a)** Grant `Qt::WA_Hover` to the handle itself, as the committed candidate
  does. The premise "the style sets `WA_Hover`" is then asserted only where the
  application's own style runs, such as macOS.
- **(b)** Re-polish the handle with a Fusion style instance at whole-number
  scales, so that the Linux lane still sees a real style set the attribute.

Two related points were left as they are and are open to the owner:

- Should the premise hold for other Linux platform themes (gtk, Breeze), or
  only for the macOS and Fusion styles the test comment names?
- The strip course still asserts that the identity has yielded whole at the
  floor, as it did before. It is not pinned as a rule, because that choice is
  already the owner's on BPR-0722.

## Independent reviews

**Ledger (`5875ce4f`).** No blocking finding.

- The reviewer confirmed the diagnosis from the lldb trace. `PersistenceRequests
  == 1` is reachable only through `observe_material`'s successful-load branch.
- The new predicate is a subset of the old passing conditions, with the order
  dependence removed.
- It found no other consumer whose meaning changes.
- It reran the course 3 of 3 (indicative, since the tree carried other work).
- **Follow-up candidate, not changed.** `review_ledger_round_trip`
  (`github_application.cpp`, about line 18890) still takes "present on entry"
  from `ReceiptLoadHits > 0U`, which the same echo can satisfy.

**Workbench (strip and seam diff).** No blocking finding.

- The product change narrows only the state-dependent band. The invalidate
  does not re-enter `apply_strip_width`.
- The reviewer's own run passed at all three scales.
- Its portability finding (a narrower strip) was corrected as described above,
  and the floor assertion was kept.
- Its seam questions are the owner question above.

## Final run

**Revision.** `de528ce38a914d47b0e62a031f3eef38602704a8`, the pushed tip that
carries both BPR-0824 commits and BPR-0822's.

**Builds.** Both presets were configured with `--fresh` (empty
`CMAKE_CXX_FLAGS`) and built with `--clean-first` under GCC 15.2 `-Werror`.

| Preset | Result | Steps | Duration | Diagnostics |
|---|---|---|---|---|
| `app-release` | rc 0 | 429 of 429 | 100 s | none from first-party code; the vendored SQLite amalgamation's one `-Wstringop-overread` |
| `dev` | rc 0 | 1241 of 1241 | 159 s | none |

**Courses.** The Secret Service `login` collection read `(<false>,)`, and
`ninja -n` reported no pending work.

```sh
ctest --preset dev -j1 -V -R '^(qt_workbench_window_scale_100|qt_workbench_window_scale_125|qt_workbench_window_scale_200|application_review_ledger_composition_contract|application_review_ledger_host)$'
```

It started at 10:34:31Z and exited with ctest rc 0 after 69 s.

| Test | Result | Duration |
|---|---|---|
| `qt_workbench_window_scale_100` | **Pass** (was: fail at line 1531) | 20.85 s |
| `qt_workbench_window_scale_125` | **Pass** (was: fail at line 1531) | 21.49 s |
| `qt_workbench_window_scale_200` | **Pass** (was: fail at line 1531) | 22.62 s |
| `application_review_ledger_composition_contract` | **Pass** (was: `REOPEN_FAILED`) | 4.36 s |
| `application_review_ledger_host` | Pass | 0.00 s |

**Repeat.** `ctest --preset dev -j1 --repeat until-fail:10 -R
'^application_review_ledger_composition_contract$'` passed 10 of 10 in
43.63 s, on the default glib dispatcher.

**Other failures seen on the way, not caused here.** The ledger worker ran
three more courses whose sources read the ledger statistics. Each fails the
same way with this change reverted, and none was diagnosed:

- `application_agent_invocation_composition_contract` fails at its seed step.
  Evidence line `1:1:1:1:1:1:1:1:163:163`.
- `application_diff_workbench_public_path` fails with
  `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`, then a 60 s timeout.
- `application_appearance_store_fallback_contract` already failed in
  BPR-0816's baseline.

**Tidy.** `tidy.py --warnings-as-errors` over the touched translation units
reports only findings that predate this card:

- `#pragma once` in `dif_runtime.hpp`;
- `exact_current` at `github_application.cpp:7725` (`df458d30`);
- `wait_for_frame` in `workbench_window_tests.cpp`.

No finding is on a changed line.

**Format.** `clang-format` 22 `--dry-run --Werror` passes, and so does
`git diff --check`.

**clang 22 proxy.** The touched translation units compile clean with
`clang++-22` and the project's clang warning set (`-Werror`, `-Wshadow`,
`-Wshadow-uncaptured-local`):

- `github_application.cpp`;
- `review_ledger_application_host.cpp` and `review_ledger_host_tests.cpp`;
- `workbench_window.cpp` and `workbench_window_tests.cpp`.

Neither AppleClang nor MSVC compiled the change, and the macOS courses were not
run.

**Portable sources touched.**

- `src/application/include/bpr/application/review_ledger_application_host.hpp`
- `src/application/src/review_ledger_application_host.cpp`
- `src/app/src/github_application.cpp` (the probe line)
- `src/adapters/qt/src/workbench_window.cpp`
- `tests/application/review_ledger_host_tests.cpp`
- `tests/adapters_qt/workbench_window_tests.cpp`

None has a platform branch.

**Against the card's acceptance.**

- **All four pass on the Linux station, and the causes are named: met** at
  `de528ce3`. The seam accommodation that lets 100 and 200 pass is the owner
  question above.
- **They still pass on macOS: not run.** This station cannot run them.

**Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-verify/`:

- `final-0824.ctest.log`, `final-0824-ledger-x10.ctest.log`,
  `courses.summary` and `stock.summary`;
- `*.build-final-de528ce3.log`, `tidy-ledger.log`, `tidy-candidate.log`
  and `clang-proxy-*.log`;
- the worker directories `ledger/` and `workbench/`.

None is committed.
