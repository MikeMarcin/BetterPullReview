# BPR-0967 Mac complete-phase responsiveness — 2026-09-25

Code candidate `9d1cfa62`, continuing baseline
`aba15afe8238a9dc33a07915eb83fb24b3153762` under
`r4-standalone-mac-20260925`. Apple M4 Pro, 48 GiB RAM, macOS 26.6.2,
arm64 Debug, Qt 6.11.2, Fusion/offscreen, 1600 × 1000 at scale 1.
The network-denied `large-review-v1` fixture uses the real application
composition, provider replay, installed Git, private SQLite state and selected
diff host: 515 files, 1,000 threads/comments, 20,000 source lines per large
pane and the stored exact base/head. No live account or remote request is used.

## Timing correction and falsification

`measure_wait` previously counted delivered timer callbacks alone. An action
that completed synchronously could report zero maximum delay, and work between
the last callback and readiness was omitted. The baseline public Read transition
took 82.438 ms but reported zero. The corrected maximum includes the final
interval through readiness; `ActionMs` and `FinalIntervalMs` expose its parts.
P95 continues to describe delivered pulses only, with the unchanged pulse count.

A controlled test performs a 120 ms synchronous action and a 120 ms final
readiness operation after timers have run. Against the old calculation the
reported maxima were **0 ms and 5.788 ms**, and the test failed. With the repair
both stalls exceed the unchanged 100 ms threshold and the control passes.
This is deliberately a failing-before measurement control, not product delay
injection. It runs only in the test executable. The failure log is local at
`build/bpr0967-responsiveness/timing-control-before.log`.

## Mac fixture completion budgets

Two uninstrumented preparation runs measured composition startup at
535–614 ms, metadata/Git readiness at 10,451–10,560 ms, first large-source
render at 1,965–2,004 ms, binary fallback at 970–983 ms, large-source revisit
at 645–671 ms and final supersession drain at 954–959 ms. The new completion
limits below round above those measurements: about 14% headroom for metadata,
25–30% for first source, fallback and final drain, and more for startup/revisit.
The metadata limit matches the existing 12-second history-refresh allowance.
The shorter interaction/input rows retain the existing 100 ms target.
These are local fixture regression limits, not universal hardware or live-network
latency guarantees. No memory threshold or existing input threshold was raised.

The exact candidate passes all 18 completion/dispatch limits:

| Measured boundary | Observed ms | Limit ms |
| --- | ---: | ---: |
| `composition_startup_render` | 511.095 | 1000 |
| `paged_review_and_git_load` | 10232.914 | 12000 |
| `large_file_selection` | 1940.211 | 2500 |
| `binary_fallback_render` | 950.883 | 1250 |
| `large_source_revisit_render` | 658.545 | 1000 |
| `rapid_selection_supersession` | 7759.686 | 10000 |
| `supersession_final_drain` | 935.705 | 1250 |
| `read_modality` | 77.901 | 100 |
| `files_drawer_open` | 49.533 | 100 |
| `files_drawer_close` | 30.568 | 100 |
| `conversations_drawer_open` | 33.267 | 100 |
| `conversations_drawer_close` | 29.482 | 100 |
| `appearance_settings_open` | 19.891 | 100 |
| `theme_light` | 21.172 | 100 |
| `theme_dark` | 30.371 | 100 |
| `next_change_dispatch` | 9.442 | 100 |
| `wheel_dispatch` | 1.110 | 100 |
| `measurement_input_gap` | 4.269 | 100 |

The course enters Read, requires both exact source panes, opens/closes Files
and Conversations through the existing actions, opens Appearance, chooses light
and dark through the existing theme gesture, waits for durable accepted mode,
and verifies unchanged comparison identity and syntax classification count.
Binary fallback must show its reason with empty panes, then recover every exact
large-source line and original anchors. It continues the 32-selection workload
with outstanding diff work and checks the final exact source. Nine interaction
frames, plus Home and the ordinary comparison, are captured locally. The Files,
dark-theme and binary-fallback full-window captures were inspected.

`CompositionStartupMs` starts at fixture application composition and ends at the
first Home widget render. It excludes process creation and `QApplication`
startup; native packaged launch is still unmeasured. Interaction readiness includes
synchronous `QWidget::grab` rendering but excludes PNG encoding and file writes.
It is neither compositor presentation time nor measured OS input delivery.
The 5 ms pulse samples host job overlap, not continuous CPU occupancy. Wheel
and change dispatch timings retain their settled-work scope; measurement-time
wheel delivery and supersession do not prove every active-work cancellation path.

## Unmet 100 ms phases

The report explicitly lists these in `InputBudgetFailures`, although the
measurement course and its separately named completion limits pass:

| Phase | Complete maximum interval ms | Target ms |
| --- | ---: | ---: |
| Initial metadata/Git load | 223.799 | 100 |
| First large-source selection/render | 109.703 | 100 |
| Large-source revisit/render | 110.688 | 100 |

The corrected synchronous Read interval is 77.901 ms (formerly omitted).
The worst newly measured drawer/theme interval is 25.859 ms. The large-source
first/revisit intervals include native document publication and the course's
explicit render; this work does not attribute the stall to one owner or prove
a repair. The original baseline already observed 230.946 ms metadata and
110.717 ms first-source gaps without the extra render course. A successful
CTest run therefore does **not** accept full 100 ms responsiveness.

## Verification and remaining scope

The focused exact-candidate CTest command is:

```sh
ctest --preset dev -R '^(application_large_review_(timing_control|responsiveness|benchmark|history|loaded_close)|application_(unavailable|approximate)_diff_public_path|test_network_isolation_manifest)$' --output-on-failure
```

All eight final courses pass in 406.43 seconds, including the 32-epoch history
course (309.88 seconds, zero model resets). Its maximum refresh interval is
70.653 ms, refresh completion 8,731.370 ms and source selection 973.947 ms.
Loaded-close passes at 138.344 MiB lifetime footprint growth against 160 MiB.
The ordinary course still records 231.771 ms initial-load and 112.820 ms
first-source gaps; the new course has not hidden them. Both documentation
gates pass, including 64 documentation tests; the selected C++ format and
scoped board validation also pass. Commands and report digests are in the durable
[handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff.json).
The report `build/dev/tests/end_to_end/large-review-responsiveness.json` has
SHA-256 `23a94557142bcd5aab6d9ad23b512e3e8701048211a20cb96b2c95f22eb0521e`.
Transient JSON, PNG and logs remain ignored build artifacts.

The earlier [Mac lifetime-peak evidence](BPR-0967-mac-lifetime-peaks-2026-09-25.md)
remains valid for its exact four resource courses. This slice changes only the
test harness and documentation: no production core, host, Qt behavior,
authority, persistence schema, package or release policy changes. The loaded-close
course rechecks the shared harness against exact owners and unchanged peak/heap
limits. Broad resource-course reruns are unnecessary for the untouched sampler.

BPR-0967 remains unfinished. The three input gaps, full active-work input and
cancellation coverage, native process-to-first-frame launch, initial metadata
apart from total fixture/Git readiness, generated/pathological fallback coverage,
peak live heap and every Windows/Linux measurement remain open. The stations
are explicitly deferred. No human, independent, native-style desktop, packaged
or release acceptance is implied. The protected RFC0052 attended session was
not touched and this slice creates no human decision or review server.
