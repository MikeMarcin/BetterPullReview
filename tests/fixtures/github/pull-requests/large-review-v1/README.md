# Synthetic large-review workload, version 1

All content and identities are synthetic. Nothing was fetched from GitHub and
none of the source files is executed. GitHub-shaped URLs identify fixture
comments only; they are not designated live test targets.

`recipe.json` defines 512 source files of eight lines, a 20,000-line comparison,
an 8,192-line generated table, a 4,096-byte binary file, 32 head revisions and
1,000 source-line review comments in ten pages of 100. The final comparison
changes all 515 files. The large file changes 20 separated lines; generated
content changes its first line. Every head after the first changes one distinct
source file. This gives a stable base, real nonempty history and heterogeneous
file content without embedding private data or a buildable third-party project.

The base is `878e9d7043aa7dcbee8ce1fecd6633e7c3eee4ff` and the latest head is
`3a8e16042ce05cae93fa7782aa9d013e49c71cbb`. `manifest.json` records every retained head, exact
base/head blob IDs and sizes, Git additions/deletions, pack bytes/hash and
comment page count/hash/continuation. Each comment anchors line one of a changed
source file at the latest head. No replies, credentials or mutation requests
are included. The comment pages have REST review-comment shapes; the manifest
supplies their explicit pagination sequence for the test-only composed replay.

Regenerate with the repository helper and an explicit installed Git path:

```sh
python3 scripts/build_large_review_fixture.py --git /absolute/path/to/git
```

The helper uses a private disposable bare repository, fixed authors/timestamps,
empty templates/hooks, disabled credential helpers and forbidden network
protocols. It creates objects via `fast-import`, validates them with Git fsck,
and writes a pack. Object identities are deterministic; compressed pack bytes
may differ between Git versions and are therefore pinned independently.
`--output` can generate a separate reviewable fixture directory.

The network-isolated `large_review_fixture_contract` imports the **stored** pack,
runs strict fsck, independently checks the linear revision graph and every
file/tree/blob/diff claim, and checks comment pagination and anchors. It also
rejects a wrong anchor, duplicate identity and corrupted pack even when their
manifest hashes are updated. The check does not use the generator to recreate
its acceptance observations.

`application_large_review_benchmark` serves the latest head through the ordinary
provider, installed Git and application selection path. Its test-only Glaze
replay converts the stored comments to 1,000 source threads and serves six REST
file pages plus GraphQL file/thread pages. All 29 account requests must match
exact compiled requests in order; production paging validates continuations.
`large_review_replay_contract` covers cursor/order/body refusal and preserves
the legacy strict single-page transport boundary.

The composed course verifies all files/comments and both exact 20,000-line
source panes, renders the full window, navigates changes, sends 1,000 native
wheel events after work settles, and checks final-selection precedence across
32 rapid selections with outstanding work. Run it locally with:

```sh
ctest --test-dir build/dev --output-on-failure -R '^application_large_review_benchmark$'
```

The network-denied test writes `large-review-measurement.json` and
`large-review-window.png` under the build's `tests/end_to_end/` directory. The
report includes platform/Qt/build/scale context, elapsed phases, 5 ms event-loop
intervals, work-overlap samples and cache/scroll counters. Timings are local
observations. BPR-0624 records the initial Mac measurements; BPR-0627 tracks
measured UI stalls. This is one application epoch, not 32 epoch refreshes.
Memory/pruning ceilings, scrolling during active work, native Windows/Linux
runtime evidence and justified responsiveness thresholds remain open. Smaller
retained workbench and epoch-model fixtures remain useful for focused checks.


The separate `application_unavailable_diff_public_path` selector exercises
resource-limit presentation and recovery using the same real composition. Since
BPR-0719 a work limit only degrades an alignment, so its test-only limits cap
the alignment at 2 segments instead: the 20,000-line file needs 40 and is
refused with `compute_limit`, while the one-line recovery file needs 2
(BPR-0858). The course enters Read, then checks the unavailable message and
its selectable label, the binary status and supported-file recovery, writing
`unavailable-diff-measurement.json` and `unavailable-diff-window.png` beside
the ordinary benchmark artifacts.

`application_approximate_diff_public_path` covers BPR-0719's approximate
alignment through the same composition. Its test-only limits disable the
ordered line index, so the 20,000-line file exceeds the production matrix
ceiling and the host publishes a text comparison marked `AlignmentApproximate`.
The course enters Read and checks that marking, the exact
`Diff ready — N semantic changes (approximate alignment)` status and both exact
source panes, writing `approximate-diff-measurement.json` and
`approximate-diff-window.png`. Neither selector alters shipped limits or uses a
fixture-produced diff result.

## Earlier-head replay inputs

`epoch-deltas.json` records the 31 transitions after the first review head. Each
entry names its exact parent/head and the changed file's before/after Git blob
OID and byte count. The 14.6 KB sidecar avoids repeating all 515 file records
for every epoch. `scripts/build_large_review_fixture.py` derives it from the
same local Git stream; the stored pack, manifest and comment pages are unchanged.
The independent fixture contract compares each transition against installed Git
and refuses altered heads, missing transitions, substituted blob OIDs or sizes.

The test-only Glaze compiler can project an exact stored head by reversing later
file deltas from the final manifest. It binds the PR, commit list, GraphQL head
and check request to that head. It reuses the synthetic thread text/IDs/line
positions as a workload projection at each head; these are not historical
GitHub captures. The default latest-head 29-request script remains unchanged.
Unknown, abbreviated, base-only and empty head identities refuse. Each compiled
head is checked through the production pager, with complete 515-file/1,000-thread
snapshots and independent ordinal-to-file-blob expectations.

BPR-0631 establishes these inputs and provider-boundary checks. BPR-0632 owns
32 actual application refreshes, retained epoch/catalog identity, historical
selection and measured UI behavior. Having these inputs does not itself prove
many-epoch application acceptance or a performance budget.

## Complete-phase Mac responsiveness

BPR-0967 adds `application_large_review_timing_control`, which demonstrates
that synchronous-ready and terminal work count toward the maximum event-loop
interval, and the Mac-only `application_large_review_responsiveness` course.
The latter measures composition-to-Home rendering, the visible first large
source, Files/Conversations toggles, persisted light/dark themes, binary
fallback, exact source recovery and existing navigation/supersession inputs.
PNG encoding and file writes happen outside the measured phases.

The report's `CompletionBudgets` holds measured values, limits and verdicts for
20 local fixture completion/dispatch and source-input boundaries. Six of them
(Qt startup, Home, provisional and accepted metadata, first source and
generated source) were set from optimized measurements alone; a Debug build
reports them with `Enforced: false`, and any optimized build enforces them
([BPR-1247](../../../../../docs/issues/40-parked/BPR-1247-large-review-accepted-metadata-render-exceeds-its-budget.md)). The two source
boundaries gate the complete first-selection and revisit intervals at the
unchanged 100 ms target after the Qt hidden-document repair.
`InputBudgetFailures` separately
names every completed phase exceeding the retained 100 ms input target; a
successful measurement course does not accept those failures. `ActionMs` and
`FinalIntervalMs` explain the maximum, while P95 uses delivered timer callbacks
only. The [Mac evidence](../../../../../docs/verification/BPR-0967-mac-responsiveness-2026-09-25.md)
records hardware, exact candidate, observations and the remaining gaps.
Composition startup excludes process and `QApplication` launch, rendering is an
explicit widget capture, and other platforms remain unmeasured.

The [Mac stall repair evidence](../../../../../docs/verification/BPR-0967-mac-stall-repair-2026-09-25.md)
records the source and conversation-layout controls. Initial metadata/Git load
still exceeds 100 ms and remains a separate unfinished obligation. Every exact
source, anchor, cache, eviction/revisit and cleanup assertion remains in force.

The [Mac load-gap evidence](../../../../../docs/verification/BPR-0967-mac-load-gap-2026-09-25.md)
isolates the first Overview table's full-row column sizing and bounds it to
visible rows. The course now also verifies every accepted thread/post's initial
unread identity. Initial-load acceptance stays open at 100 ms; the report retains
that failure separately from the unchanged source-input and completion gates.

The [Mac startup inventory](../../../../../docs/verification/BPR-0967-mac-startup-inventory-2026-09-25.md)
verifies that `large-review-v1` is not accepted by the public executable's
`--github-fixture` selector. Its separate benchmark uses the real composition
but starts its first timer after `QApplication` construction. Neither its
`fixture_connect` phase nor its composition-to-Home widget capture measures
process launch or native compositor presentation. First provisional metadata
is also distinct from the later metadata/Git-ready aggregate and currently has
no separate benchmark clock. Those budgets remain pending.

## Mac queued input and close during outstanding work

`application_large_review_active_work` uses the visible Files tree in Flat
order. It posts 31 Down keys while moving through cold synthetic source files,
records admitted diff work both at queue insertion and at event-filter delivery,
and checks each selected path on the next timer tick. Counts at insertion can
differ from counts at delivery. Host outstanding work includes admitted jobs
through decision delivery, deferred ingress and local terminals; these samples
do not prove continuous worker or CPU occupancy.

The final file stays selected through all outstanding work. Source-change
observers begin at that final key's actual delivery and reject stale publication;
the terminal checks every source line, blob and comparison/authority anchor.
A subsequent cold-file request triggers a public window close only after the
course observes both pending diff state and outstanding work. Close-to-quiescence
includes whole-application shutdown; it does not isolate stop-token acknowledgement
or prove that a worker was still executing at the instant of close. The course
also checks cache release and unchanged exact retained Git/catalog anchors.

Eight local regression gates retain the 100 ms input target, the existing
10-second rapid-selection/1.25-second final-drain bounds, and a 100 ms
close-to-quiescence bound derived from 16.568/16.837 ms M4 Pro observations.
`InputBudgetFailures` still lists any phase that misses 100 ms, including initial
metadata/Git load. The [active-work evidence](../../../../../docs/verification/BPR-0967-mac-active-work-2026-09-25.md)
keeps each timing boundary, exact candidate, negative controls and unobserved
work categories distinct. It supplies no OS input, compositor, native-style,
all-background-work, Windows/Linux or full-card acceptance.

## Optimized Mac continuation — BPR-0967

Performance acceptance now requires Release optimization; prior Debug timings
and memory samples are diagnostic only. The [full Mac evidence](../../../../../docs/verification/BPR-0967-mac-full-remaining-2026-09-25.md)
records the production-matching arm64 compiler/deployment settings and the
fixture-versus-distribution limitation. `application_large_review_launch`
observes process creation through flushed main/Home-render markers, while the
responsiveness course separately captures provisional and accepted metadata
and checks exact generated source. All observed responsiveness phase gaps,
including initial load, must meet the unchanged 100 ms target.

`application_large_review_provider_cancel` continuously holds the fixture
provider through twenty queued inputs and observes the real host stop token,
provider exit and public-close drain. `application_large_review_pathological`
lowers the fixture materialization limit, verifies the exact unavailable reason
and blob identities, then recovers binary and exact supported source. This is
a reduced-limit refusal control, not a production-limit huge-file measurement.

The four resource courses also report an independent all-zone sampled live-heap
peak, largest sampling interval and cost. Its ceiling is 160 MiB growth, or
184 MiB for byte pressure. These sampled observations cannot exclude unsampled
heap spikes; the separate native lifetime-peak counter bounds physical footprint.
Native compositor, packaged launch, additional active-operation stages and
Windows/Linux acceptance remain explicit pending obligations.

The explicitly selected `--allocation-profile` diagnostic reuses loaded-close,
preserves source/cache/lifecycle assertions, marks `Extended.AllocationProfile`,
and emits only `LARGE_REVIEW_ALLOCATION_CAPTURE_READY` after destruction. It
allows ten seconds for a no-content external heap-history capture. Instrumented
allocation/footprint/cleanup measurements bypass their acceptance ceilings in
this mode; ordinary CTest courses cannot select it and retain every ceiling.
Its timing and physical footprint are excluded from performance acceptance.

## Windows and Linux courses — BPR-0967

Every large-review course row is registered on macOS, Windows and Linux. The
native counters live in `tests/end_to_end/large_review_native_memory.cpp`;
each report names them in `MemoryMethod`. Windows pairs private commit with its
process-lifetime peak and samples the process heap's allocated bytes; Linux
pairs `VmRSS` with `VmHWM` and samples glibc's in-use arena and mmapped bytes.
The timing control's held 32 MiB allocation checks the live-heap sampler on all
three platforms.

Each platform enforces numeric limits justified by its own optimized
measurements. The benchmark's platform table records them and their basis, in
`Extended.LimitBasis`: the M4 Pro Mac, and the Windows and Linux station runs in
the [station budget record](../../../../../docs/verification/BPR-0967-station-budgets-2026-10-01.md).
The 100 ms input target is the same everywhere. Phases measured at or over
90 ms of it on a platform are named in `Extended.KnownInputMisses`, reported
and not enforced. A Debug build off macOS enforces no numeric limit
(`BudgetsEnforced: false`) and names what it would have missed in
`UnenforcedLimitMisses`. Windows Debug doubles every course clock. Exact source,
cache, eviction, release and lifecycle assertions fail the row everywhere.
`Extended.WaitExpiry` names the wait that ended a course unready, and why. A failed
course writes its partial report, with `Failure`, to the report path.
`tests/end_to_end/large_review_summary.py BUILD_ROOT [CTEST_LOG ...]` condenses
one tree's reports, with their digests, for a station return.
