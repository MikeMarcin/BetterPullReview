---
schema: "implement-rfc.card.v1"
id: "BPR-0967"
title: "Measure and reconcile native large-review resource budgets"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0967-mac-responsiveness","path":"docs/rfcs/plans/bpr0967-mac-responsiveness/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9aa3df3bb9b39fe7e1a76f9e359fe57706b5c2b2","evidence":{"kind":"observation","detail":"Mac growth attributed (BPR-0893; repaired by BPR-1032). Windows and Linux growth measured by native counters on the stations and within limits set from those measurements; no failure left to attribute. docs/verification/BPR-0967-station-budgets-2026-10-01.md"}},{"criterion":"AC2","outcome":"pass","revision":"9aa3df3bb9b39fe7e1a76f9e359fe57706b5c2b2","evidence":{"kind":"command","detail":"Cache ceilings, exact source/anchors, eviction/revisit and close/prune ownership enforced on all three platforms. Each changed limit has a measured basis and stated rule; failing-before controls for reporting, enforcement, named phases and the every-course gate. Release gates at 9aa3df3bb: macOS 16 rows + architecture + replay pass; Windows 17/17 (440.84 s), Linux 17/17 (95.92 s), all enforced. No limit raised to absorb a miss; misses named under BPR-1264."}},{"criterion":"AC3","outcome":"pending","revision":"6a573ac7fce5f84716f2439c2c9de1c198a58fd2","evidence":{"kind":"freeform","detail":"Fixture-composition budgets (metadata, first render, navigation, drawers, theme, fallback, active work, input/cancellation, memory/cache, process-to-Home launch) measured and enforced on macOS, Windows and Linux. Pending human/native: packaged launch, compositor presentation and physical-input latency per platform, attended items BPR-0967-{packaged-launch,presentation,physical-input}-{mac,win,linux}-v001 in session cbd48b91000b00f1e72e1ad62c7c384c."}}]
---

# Measure and reconcile native large-review resource budgets

## Transferred obligation

Original RFC0013 performance-budget paragraph and acceptance rows 2–3, including full active-work responsiveness and sampled/peak memory on declared target hardware.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

The historical macOS Qt 6.10.3 courses passed provisional 100 ms input and 160/96 MiB sampled growth/cleanup budgets at their recorded candidates. The r4 closeout at source 3efe61dfcf6874d35400bf777a7e86dbbf0a4693, Debug Qt 6.11.2 arm64, freshly runs the current public benchmark successfully, but both application_large_review_loaded_close and application_large_review_resources fail at the unchanged 167,772,160-byte growth bound before close/prune. The first loaded-close sample is 197,149,656 bytes versus 4,932,232 baseline (192,217,424 bytes growth). The cause is unproved; a changed Qt version is context, not attribution. Later cleanup assertions were not reached. No threshold was changed or failed run called passing. The fixed-path repeat is retained in build/rfc0013-owner-closeout/memory-recheck.log; the RFC ledger records its result.

## Acceptance

The unchanged repeat fails 2/2 in 26.69 seconds. Loaded-close growth is
192,217,376 bytes (197,231,528 minus 5,014,152); resources growth is
191,136,128 bytes (196,133,872 minus 4,997,744). Each cache is one entry at
3,517,688 bytes with no diff/epoch job at that sample, so the complete process
footprint is distinct from the bounded diff cache. No source attribution or
leak diagnosis follows from these counters. The repeat log SHA-256 is
`a6634710efb5129ff8b262a11b7b040b9bdb4dee3dbe387d9bebd5a148f8f3ee`.

The passing ordinary benchmark records a 321.700 ms initial metadata/load
timer gap, 98.723 ms large-source and 32.004 ms rapid-selection maxima. Its
pass is not a blanket 100 ms responsiveness observation for every phase.
The large-review render currently shows Overview while its source-document
assertions are exact; BPR-0940 owns that fixture/presentation reconciliation.

- AC1: Attribute the current Mac growth failure with exact artifact, hardware, toolkit and representative workload evidence; distinguish retained live owners, transient peaks, allocator retention and leak evidence before changing policy or thresholds.
- AC2: Preserve existing cache ceilings, exact source/anchors, eviction/revisit, close/prune ownership and timing assertions. A repair needs a failing-before control and the complete affected public courses; a budget change requires measured justification and an explicit documented amendment.
- AC3: Establish justified target-specific launch, metadata, first-render, navigation, drawers, theme/fallback, active-work input/cancellation and memory/cache budgets on actual eligible Windows/Linux/macOS hardware. Report unsampled peaks and unavailable platforms explicitly.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## macOS attribution and Mac budget amendment, from BPR-0893 — 2026-09-23

[[BPR-0893]] attributed the Mac failure the transfer records. This is AC1
evidence for macOS only. AC2's amendment is recorded there, and AC3 is
untouched:

- **Retained live owners, and a toolkit change.** The growth past 160 MiB at
  `large_file_ready` is Qt 6.11.2 keeping a HarfBuzz buffer in every shaped
  line's layout (qtbase `8209078e0e`): 70.4 MiB live across the 40,000
  measured lines, and none under Qt 6.10.3. The pair `92c4e159^` /
  `92c4e159` isolates it, 121.6–122.6 MiB against 184.2–186.2 MiB.
  [[BPR-1032]] owns the product side.
- **Allocator retention, not a leak.** After cleanup the live heap returns to
  60–68 MiB with the application open and 13–14 MiB once it is destroyed, in
  every run. The footprint stays at 128–178 MiB because the default zone keeps
  freed pages dirty (87% of it free at `application_destroyed`), and
  `malloc_zone_pressure_relief` does not return them.
- **Amendment, with its measured basis.** On macOS the sampled footprint budget
  is 224 MiB (highest observed peak 202.9 MiB), and cleanup is bounded on the
  live heap at 96 MiB with the application open and 32 MiB once it is
  destroyed, in `tests/end_to_end/large_review_benchmark.cpp`. All four
  resource courses pass.

Unsampled peaks, responsiveness budgets, and Windows and Linux budgets on
eligible hardware remain this card's.

## The Mac memory budgets after BPR-1032 — 2026-09-23

[[BPR-1032]] (`27c35729`) removes the per-line HarfBuzz retention that
[[BPR-0893]]'s 224 MiB bound accepted. On the same M4 Pro, Debug build,
`large_file_ready` live growth falls from 164.2 to 101.1 MiB. The Mac bounds
in `tests/end_to_end/large_review_benchmark.cpp` are re-derived from the new
measurement:

- **Footprint, resources / loaded-close / cache-entry courses:** 160 MiB.
  The highest observed peak is 138.7 MiB.
- **Footprint, cache-byte course:** 184 MiB. Its peak is 166.7 MiB, because
  the course fills the diff cache to its byte budget.
- **Live heap after cleanup:** 84 MiB with the application open (observed
  67.6) and 32 MiB once it is destroyed (observed 13.8).

That is AC1/AC2 evidence for macOS only. AC3 is untouched: launch,
metadata, first render, navigation, drawers, theme and cancellation budgets,
and every Windows and Linux measurement, remain this card's.

While measuring, the Mac station found `application_large_review_history`
red. Every epoch refresh holds the GUI thread for about 167 ms against the
100 ms input budget, with or without BPR-1032. That is filed as
[[BPR-1043]], and fixed there (`7b5d78be`): the conversation delegate no
longer reshapes every post on each relayout, and refreshes are back to a
median worst gap of 68.8 ms. The same change lowers the history course's
initial-load gap from 292–309 ms to 231–243 ms. That gap is still over
100 ms, and it remains this card's responsiveness question.

## Mac unsampled footprint high-water gate — 2026-09-25

At code candidate `8b6407fd`, the native Mac resource sampler now records the
process-lifetime physical-footprint peak and checks its growth against the
existing course-specific 160/184 MiB bounds. The four affected composed
courses pass 4/4 with the gate enabled. The resources course reveals a
139.22 MiB lifetime-peak growth where its largest checkpoint was 124.27 MiB;
the cache-byte course peaks at 171.41 MiB against 184 MiB. Exact hardware,
toolkit, report digests and the other two rows are in the
[Mac peak evidence](../../verification/BPR-0967-mac-lifetime-peaks-2026-09-25.md).
The counter bounds unsampled physical-footprint peaks on this Mac, but it does
not time their phases or measure peak live heap. The remaining AC3 platforms
and responsiveness dimensions above are still unobserved; this card stays
Backlog.

## Mac complete-phase responsiveness — 2026-09-25

The standalone continuation at code `9d1cfa62` corrects two timing blind spots:
synchronous-ready work and the interval after the final timer now contribute
to the phase maximum. A controlled pair of 120 ms stalls failed before the
repair (reported as 0 and 5.788 ms) and passes afterward. It adds a Mac public
course for rendered drawer toggles, persisted light/dark changes, binary fallback
and exact large-source recovery, with 18 justified fixture completion/dispatch
limits and an explicit `InputBudgetFailures` list. The
[responsiveness evidence](../../verification/BPR-0967-mac-responsiveness-2026-09-25.md)
binds the hardware, course, limits and direct observations; the
[handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff.json) records checks,
ownership and continuation.

AC3 remains partial. The exact-candidate course still exceeds the unchanged
100 ms target during metadata/Git load (223.799 ms), first large-source
selection/render (109.703 ms), and large-source revisit/render (110.688 ms).
The remaining completion/dispatch limits pass; that does not accept those
input failures. The final ordinary, unavailable/approximate, history and
loaded-close results remain separately identified in the handoff. AC1/AC2's
prior native attribution and four-course peak evidence are reused unchanged.

Next work is to attribute and repair those three full-phase gaps and expand
true process/Qt startup, separate initial-metadata, active-work input/cancellation,
generated/pathological fallback and peak-live-heap coverage. Windows/Linux
stations are explicitly deferred. No native-style, human, independent, package
or release acceptance is supplied, and RFC0052's protected review is untouched.
The card stays unfinished after this bounded Mac slice.

## Mac owning-boundary stall repairs — 2026-09-25

The next standalone milestone at code
`f3f2c6eed52c364e716f3157d1dba6f05af3ba35` reproduces the same public-course
gaps at 231.222 / 110.085 / 104.895 ms and repairs two Qt owners. Conversation
detail hides unrelated roots before layout measures them, preserving the entire
incremental model and exact selected post. Two-pane source publication no longer
builds a hidden third unified document; one-pane toggles build or release it
after retaining the exact semantic reading anchor. Both new structural controls
failed with the original product and pass with the repair.

The first uninstrumented repaired course measures 148.039 / 75.378 / 74.000 ms.
First source and revisit now have complete-interval gates at the original 100 ms
target. Load still exceeds it; attribution identifies synchronous conversation,
Overview and host-handoff work, with overlapping timings rather than a proven
single remaining cause. The [stall repair evidence](../../verification/BPR-0967-mac-stall-repair-2026-09-25.md)
and [new handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-stall-repair.json)
bind exact final checks, report identities and the separate remaining load
obligation. All temporary timers are removed. No threshold was raised.
The final uninstrumented run passes 20/20 named fixture gates, with first source
79.443 ms and revisit 83.142 ms; the load gap remains **142.388 ms** and is
explicitly listed in `InputBudgetFailures`.

AC3 remains partial and the card returns to Backlog. Earlier evidence is retained
at its historical candidate; the new candidate rechecks affected source/cache,
eviction/revisit, history and cleanup courses. Further work needs a separate
bounded load repair plus the other Mac dimensions above. Windows/Linux are
explicitly deferred; no full BPR-0967, human, native-style or release acceptance
is claimed, and the RFC0052 attended session remains protected.

## Mac first Overview column sizing — 2026-09-25

The bounded successor at code `cdbb208df52cf4478d2807b8aaedb20e3170356b`
reproduces the remaining initial-load gap at 145.381 ms. Nested public-course
traces isolate about 40 ms of native column sizing while Overview becomes
visible. Its header now sizes from visible rows, retaining all exact model
rows; a thousand-file delegate control fails before and passes after, including
scrolling to the last file. A candidate read-state identity-index optimization
showed no useful composed benefit and was removed. Host, persistence and
event-loop scheduling behavior are unchanged.

The final uninstrumented load gap is **108.992 ms**: improved, still above the
unchanged 100 ms target. First source **84.000 ms** and revisit **86.241 ms**
retain their passing gates; the course passes its 20 existing fixture limits
while `InputBudgetFailures` still names initial load. Every accepted thread/post's
initial unread identity is now checked on the same composition. The
[load-gap evidence](../../verification/BPR-0967-mac-load-gap-2026-09-25.md) and
[handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-load-gap.json)
retain exact attribution, rejected experiment, controls and affected verification.

AC3 stays partial. Further initial-load work requires a separately bounded
owning analysis or a justified scheduling design with ordering and stale-state
controls. True process/Qt launch, separate metadata, active-work input/cancellation,
generated/pathological fallback, peak live heap and explicitly deferred
Windows/Linux evidence remain open. The protected RFC0052 review is untouched.

All 18 selected checks have passing observations on the exact code candidate.
The first pass is 15/18 with three system-sleep interruptions; their unchanged
repeat passes 3/3. The evidence retains both results, exact sleep intervals and
resource/history invariants. The card returns to Backlog for the remaining
acceptance; this milestone supplies no full BPR-0967 closeout.

## Mac public startup measurement inventory — 2026-09-25

The bounded startup assignment at unchanged source
`6c2d531c63b55626bfaebde3876f3ac67904989a` reaches its explicit partial-result
boundary. The rebuilt public executable refuses `--github-fixture large-review-v1`
with exit 1 and `PUBLIC_PATH_FAILED`; the same network-denied executable renders
its supported `github-com` control and exits 0. The large-review composition is
available in the separate benchmark, whose clocks begin after `QApplication`
and whose captures do not observe compositor presentation.

The [startup inventory](../../verification/BPR-0967-mac-startup-inventory-2026-09-25.md)
records each boundary, the existing provisional metadata path, exact hardware,
executable identities and both public controls. No process launch, separate
first metadata or native first-frame duration is claimed, and no new budget is
set. The unchanged responsiveness course passes its 20 existing gates while
still recording a **108.139 ms** initial-load input gap against **100 ms**.
The earlier **108.992 ms** observation remains bound to its original run.

No product/test code, timing boundary, cache ceiling or resource threshold
changes. Source/cache/history/peak evidence is reused at its recorded candidate.
A successor needs an explicitly scoped safe public entry for the representative
workload and actual launch/metadata/native-frame observation boundaries.
The [startup handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-startup.json)
preserves the pending obligations and quiescent return. BPR-0967 stays unfinished;
Windows/Linux are deferred and RFC0052's protected review is untouched.

## Mac queued input and close during outstanding work — 2026-09-25

The bounded active-work successor at benchmark candidate
`31a33afb903e7ca9ed539591552269ec948b98f5` adds a safe network-denied composed
course through the visible Files tree in Flat order. All 31 queued keys are
posted with outstanding diff work; **16/31** still observe a pending diff job
at event delivery, and **15/31** observe zero. Samples do not establish continuous
worker occupancy. The final exact file remains held through all work, with
source-change observations plus complete line/blob/authority/anchor checks.

All eight local timing gates pass: maximum queue-to-delivery **4.113 ms**,
queue-to-verified-selection **21.516 ms**, and complete active-selection input
interval **26.270 ms**, all against **100 ms**. Public window close starts with
one outstanding diff and one epoch job, dispatches in **9.466 ms** and reaches
whole-application quiescence in **16.671 ms**. That aggregate drain is not an
isolated cancellation acknowledgement. Cache release and exact retained
Git/catalog identities pass; existing limits remain unchanged. The new 100 ms
local close-drain bound is justified by two 16.6–16.8 ms preliminary observations.

The [active-work evidence](../../verification/BPR-0967-mac-active-work-2026-09-25.md)
records timing boundaries, the initial hidden-list harness mistake, two clean
negative controls and the **6/6 focused pass in 86.42 s**. No product code or
performance policy changes. The final active and responsiveness courses retain
initial-load misses of **108.424500/110.075125 ms**. The earlier **108.139042 ms**
miss remains preserved and unsolved.

AC3 remains partial: continuous activity, isolated cancellation latency and
other background-work stages are unproved; launch, separate metadata/native
first frame, generated/pathological fallback, peak live heap and deferred
Windows/Linux measurements remain open. No new human/native-style/independent
or release acceptance is supplied. RFC0052's protected review is untouched.
The [active-work handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-active-work.json)
returns the candidate and exact remaining obligations. BPR-0967 remains unfinished.

## Full remaining Mac continuation and optimized acceptance — 2026-09-25

Source `5a26d6b1b33a90d05072e953479561859d7ac7b3` completes the one-owner remaining Mac assignment through
its recorded limitations. The user requires optimized measurements: all earlier
Debug timing and resource observations are diagnostic/correctness history only,
not current performance acceptance or budget justification. Release uses the
production arm64 optimization/deployment policy with safe fixture composition.

Compact conversation geometry reuse repairs the remaining measured load miss;
the unchanged 100 ms gate now passes. Process-entry/Qt, provisional/accepted
metadata, widget render, generated exact source, materialization fallback and
recovery, held-provider input/stop/exit, diff supersession, all four resource
courses and retained history have optimized evidence. Existing source/cache/
cleanup limits remain unchanged; new local bounds and their measured headroom
are in the [full Mac report](../../verification/BPR-0967-mac-full-remaining-2026-09-25.md).
Independent live-heap sampling records its missed-interval limitation. A calibrated
noncompacting native loaded-close capture bounds heap with a joint heap/anonymous-VM
high-water observation; it supplies no uninstrumented timing/footprint acceptance.

AC3 remains partial: Windows/Linux are expressly deferred; packaged/native
launch, compositor and physical-input endpoints, full production-limit
pathological/long-line workloads, other active-operation stages, and complete
heap-only peaks beyond the captured loaded-close bound are unmeasured. No new
human/native-style/independent or release acceptance is inferred. These concrete
limits supersede earlier suggestions to allocate another implementer per subpart.
The same implementer completed all independently eligible work through this
handoff, with no delegation or intermediate owner turnover. The protected
RFC0052 session is untouched. BPR-0967 returns to Backlog, with the
[full durable handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-full-mac.json)
preserving commands, candidates, controls, artifact identities and next actions.

## Windows and Linux station measurements — 2026-10-01

Promoted from Backlog under the user's unattended-milestone mandate, amended to
allow the Windows and Linux remote stations. The milestone plan's one milestone
keeps every Mac exit and adds the station exits
([amendment receipt](../../rfcs/plans/bpr0967-mac-responsiveness/plan-amendment-stations.json)).

**Harness, Mac side.** The resource courses no longer refuse off macOS.
`tests/end_to_end/large_review_native_memory.cpp` supplies each platform's
counters, a current figure paired with the process-lifetime peak of the same
figure: Mac physical footprint and its ledger peak (unchanged); Windows private
commit (`PrivateUsage`) and `PeakPagefileUsage`; Linux `VmRSS` and `VmHWM`. The
live-heap sampler reads every malloc zone on Mac, `HeapSummary` of the process
heap on Windows and glibc `mallinfo2` on Linux. Every report names its method in
`Extended.MemoryMethod`, and the timing control's held 32 MiB allocation now
checks the sampler on all three platforms.

Every numeric limit is justified by optimized Mac measurements alone. Off Mac,
`Extended.BudgetsEnforced` is false: completion budgets are reported with
`Enforced: false`, the 100 ms input misses stay in `InputBudgetFailures`, and each
resource/history limit that would have failed is named in
`Extended.UnenforcedLimitMisses`. Exact source, cache, eviction, release and
lifecycle assertions fail the row on every platform. `large_review_launch.py`
enforces its limits only on macOS. A failed course now writes its partial
report, with `Extended.Failure`, to the report path. The eleven rows that were
Mac-only are now registered on Windows (uncontained, like BPR-0874's three) and
Linux, so all sixteen large-review rows run on every platform.
`tests/end_to_end/large_review_summary.py` condenses one tree's reports with
their SHA-256 for a station return.

Two temporary controls on Mac Release, with the source restored byte-for-byte
afterwards (SHA-256 `d433f354…f764d`): with the Mac marked unjustified and the
footprint and input limits lowered to 1 MiB and 1 ms, loaded-close and
responsiveness pass and report three and zero limit misses, sixteen input
failures and fourteen unenforced completion misses; with the Mac justified and
the same lowered limits, both fail (`sampled Mac fixture footprint growth
exceeded its budget`, `Mac fixture responsiveness budget exceeded`) and each
leaves its partial report with `Failure`.

No limit or threshold changed. Windows and Linux budgets are not yet set: they
need the station measurements this milestone requests, then a confirmation run.

At pushed code candidate `cef11b982`, all sixteen large-review rows pass on the
Mac Release tree in 120.5 s with every limit enforced, no miss and no input
failure, and `architecture_boundaries` passed on the same change before rebase. The [station handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-stations.json)
holds the exact Windows and Linux requests (optimized Release tree, then the
station's ordinary Debug tree, sixteen rows each), their expected outputs, what
to return, and the next round: per-platform limits from those measurements,
enforced in optimized builds, row timeouts sized from the Debug durations, and
one confirmation run. Packaged launch, compositor presentation and physical
input on every platform still need a person at the desktop.

## Windows: the large-review benchmark did not compile under MSVC (bpr-main-win, 2026-10-01)

The Windows station run at `cef11b982` stopped at step 2. The
`windows-app-release` configure succeeded. The build failed on
`tests/end_to_end/large_review_native_memory.cpp` with `psapi.h(132): error
C2146: syntax error: missing ';' before identifier 'WINAPI'` and a run of
`'int BOOL'` and `'int DWORD'` redefinitions.

The file included `<psapi.h>` before `<windows.h>`. That is alphabetical
order, which clang-format's `SortIncludes` keeps within one block, but
`psapi.h` needs the base types `windows.h` declares. The fix is the
repository's own idiom (`agent_canary_main.cpp`): `windows.h` in its own block,
ahead of `psapi.h`, with a comment saying why.

With the fix (MSVC 14.51), both trees build
`bench_better_pull_review_large_review` with no warning:
`build/bpr0967-release` (windows-app-release, Release) and
`build/windows-dev` (Debug). `coding-format-check` passes. The Windows
measurement is rerun at the fix commit.

## Windows and Linux limits from the station runs — 2026-10-01

Both stations ran the sixteen rows in an optimized tree and in their Debug
tree. Windows ran at `682ea1392`, which adds the station's
`windows.h`-before-`psapi.h` build fix, and Linux at `e257b236a`
([[BPR-1262]] made its tests-on Release graph build). Release passed 16/16 on
both. Debug passed 16/16 on Linux and 14/16 on Windows.

The [station budget record](../../verification/BPR-0967-station-budgets-2026-10-01.md)
derives each platform's limits from these runs. From code `f61340e17` they are
enforced in optimized builds:

- Windows byte-pressure footprint growth 208 MiB;
- Linux footprint growth 168 MiB, and 216 MiB for byte pressure;
- Windows active close-to-quiescence 150 ms;
- every other Mac limit kept, with its margin.

The 100 ms input target is unchanged. The phases at or over 90 ms of it are
named per platform, reported and not enforced, and [[BPR-1264]] owns them.

Debug off macOS reports and enforces no numeric limit. On Windows, Debug
doubles the course clocks, because the resources course outran 90 s. The Debug
byte-pressure failure ended at 323 s of an 840 s clock, so the clock was not
the cause. It stays a named failure for [[BPR-1265]], and a new
`Extended.WaitExpiry` field records which wait ended a course unready, and why.
Windows row timeouts are sized from the Debug completions and the doubled
clocks; macOS and Linux timeouts already cover three times every measured
completion.

AC3 now has optimized Windows and Linux measurements and enforced limits,
pending the confirmation run at `f61340e17`. Packaged launch, compositor
presentation and physical-input latency on every platform remain human/native
obligations.

## Windows confirmation at `f61340e17` (bpr-main-win, 2026-10-01)

Steps 1–6 of `handoff-stations-confirm.json`, with the station otherwise idle.
The tracked tree was clean, and the checkout was detached at
`f61340e17d0c7480de14b2908276166d75b47102` and then switched back to main.
The summaries are `docs/verification/BPR-0967-station/windows-confirm-release-summary.json`
and `windows-confirm-debug-summary.json`. Both name `source_revision`
`f61340e17…` with 0 tracked changes. They are LF copies; the station's CRLF
originals hash `DF08C5CA…` (Release) and `7D7305AE…` (Debug).

- **Load.** The mean `LoadPercentage` read 100 at 15:32:52-07:00. That was a
  single instantaneous sample taken as step 1 began. It read 3 at
  16:23:45-07:00.
- **Builds.** `build/bpr0967-release` and `build/windows-dev` each built the
  three targets and exited 0. There was no `warning C` or `error C`/`LNK` line
  in either. Both compiled BPR-1262's
  `src/application/src/designated_review_authority_bootstrap.cpp`, so that
  change builds clean under MSVC. The only warnings were CMake's
  "uninitialized" notices from Qt's own config files: 111 in the Release log
  and 112 in the Debug log.
- **Release.** ctest ran from 15:35:01.2281232 to 15:42:25.6332927-07:00 and
  exited 0: 17 of 17 pass, in 444.37 s. Every report has `BudgetsEnforced`
  true, an empty `Failure`, empty `UnenforcedLimitMisses` and
  `CourseClockScale` 1. `LimitBasis` names this station: "Windows 11 x64
  i9-10900KF 128 GiB, Qt 6.11.2, MSVC…". The launch report has `Enforced`
  true. No limit failed. `large_review_replay_contract` passes in 2.56 s.
  heap-control: baseline 33,146,296, peak 66,705,365, final 33,146,328,
  5 samples.
- **Debug.** ctest ran from 15:44:25.3017012 to 16:23:44.3223778-07:00 and
  exited 0: 17 of 17 pass, in 2,358.98 s, with `BudgetsEnforced` false and
  `CourseClockScale` 2 throughout.
  - `application_large_review_resources` now passes, in 110.80 s.
  - `application_large_review_cache_bytes` passes in 1,626.01 s against its
    2,280 s `TIMEOUT` (1.40x), with no wait expiry. Its unenforced misses are
    "sampled fixture footprint growth exceeded its platform budget" and
    "fixture lifetime peak footprint growth exceeded its platform budget".
  - `application_large_review_history` names "epoch source selection
    exceeded its measured platform budget" and "epoch refresh exceeded its
    measured platform budget".
  - `large_review_replay_contract` passes in 33.23 s against its 120 s
    `TIMEOUT` (3.61x).
  - heap-control: baseline 37,730,004, peak 71,294,102, final 37,730,088.
- **Search.** The step 6 `Select-String` for `large-review-benchmark-failed:`,
  `large-review wait:`, `***Failed` and `***Timeout` matched no line in either
  log.

SHA-256 of the station files:

| File | SHA-256 |
|---|---|
| `windows-confirm-debug-build.txt` | `CF03DD794DF5D98E68CC3F4054ECAA9E6A7A61C371E8E347F5F0EE2586EC6FF3` |
| `windows-confirm-debug-console.txt` | `EA23398A5D5C150F56665813BD59CD252194C091C27B4B70956403CB0FA481D0` |
| `windows-confirm-debug-ctest.log` | `C2ED89BDFDB5A675A284E8B4D330454266A3E8B79C16949990723745289D36CD` |
| `windows-confirm-debug-summary.json` | `7D7305AE6CAAF493A074A0093B9299A763FC9C17A7EE30E44F0A1EA9D3DCFAEB` |
| `windows-confirm-release-build.txt` | `114152AB0DAB3CA69CD5FE6BADE2EE18B80C37296446B195F89B3726C89C9933` |
| `windows-confirm-release-console.txt` | `DFD3958727003F72F44E137A3F5D9A8E2118191DCEE3B9EC3B116C2E386F6C7D` |
| `windows-confirm-release-ctest.log` | `B95D6464E41BF0B9447DE66F597D93FB1A76891B85864B2668F6C3548B41F426` |
| `windows-confirm-release-summary.json` | `DF08C5CA73EAD0201581093801C0AE1A0E629D0B60207B2E91BDD510BF001D42` |

## Reconciliation — 2026-10-01

Both confirmations at `f61340e17` passed: Windows Release 17/17 with every
limit enforced and Debug 17/17; Linux Release 17/17 in 89.30 s and Debug 17/17
in 189.24 s. The [station budget record](../../verification/BPR-0967-station-budgets-2026-10-01.md)
has the detail.

The Linux run exposed a gap: the approximate, unavailable, first-tab, ordinary
and history courses recorded input misses without failing. From `9aa3df3bb`
every course gates the 100 ms target in optimized builds, except the four
heap-sampled resource courses. The newly gated phases over 90 ms are named
under [[BPR-1264]]. The same commit:

- triples the Windows Debug course clocks, since the byte course had used
  1,626 of its 1,680 s;
- resizes the Windows timeouts;
- gives the Linux replay contract 60 s.

Mac Release passes at that commit. Windows and Linux Release confirmation at
`9aa3df3bb` is the last unattended step.

| Criterion | State |
|---|---|
| AC1, attribution of the Mac growth | Met on macOS: BPR-0893, then BPR-1032's repair. On Windows and Linux, growth stays within limits set from its own measurement, with no failure to attribute. |
| AC2, preserved invariants and justified changes | Met. Every changed limit has a measured basis and a documented rule, with failing-before controls for the gating mechanisms. Cache ceilings, exact source and anchors, eviction and revisit, and close/prune ownership are enforced on all three platforms. No limit was raised to absorb a miss. |
| AC3, platform budgets on eligible hardware | Met for the fixture composition on all three platforms. That covers metadata, first render, navigation, drawers, theme and fallback, active work, input and cancellation, memory and cache, and launch to Home from process creation. Pending after the `9aa3df3bb` confirmation: packaged-application launch, compositor presentation and physical-input latency, which need a person. Remaining named input-target phases go to BPR-1264; the Mac's unmeasured items (heap-only peak, production-limit pathological workloads, further active stages) are in the [full Mac handoff](../../rfcs/plans/bpr0967-mac-responsiveness/handoff-full-mac.json). |

## Native items for a person at the desktop

These can't be produced by an offscreen fixture course. For each, record the
platform, hardware, display (refresh rate and scale), package version and file
digest, how it was measured, and the measured values.

1. **Packaged-application launch.** Install the release package:
   - macOS: `macos-release` DMG;
   - Windows: `windows-app-release` MSI;
   - Linux: `linux-package`.

   Measure from activation (Finder or Dock click, Start menu entry, desktop
   launcher) until the Home window is visible and accepts input. Take a cold
   launch after a reboot and three warm launches. The fixture course's
   process-to-Home figures (Mac 348 ms, Windows 558, Linux 324) are the
   comparison, not the result.
2. **Compositor presentation.** Record the screen at 60 fps or more. Measure
   from the triggering input to the first presented frame showing the result,
   for three steps: Home appearing; opening a large review (a safe public or
   test repository, never a private target); selecting a large file in it.
3. **Physical-input latency.** For a key press and a mouse click on the Files
   drawer toggle, the theme switch and next-change navigation, measure from
   physical actuation to visible response. A high-speed camera works, or an
   input-to-photon tool. The course's queue-to-delivery figures (2–20 ms) are
   not this measurement.

When the confirmation returns clean, this card parks on `human-review` with
these items as its pending criteria. Each would be one attended question per
platform and item, asked as Accept or Reject against a stated threshold.

## Release gates at `9aa3df3bb` and the attended native items — 2026-10-01

Both stations ran the Release-only gate confirmation at `9aa3df3bb`, the commit
that makes every unsampled course gate the 100 ms target:

- **Linux** (`59b71b144`): 17/17 in 95.92 s. Four named phases, no other input
  miss, and the failure search printed nothing.
- **Windows** (`6a573ac7f`): 17/17 in 440.84 s, at load 15% → 6%. Eight named
  phases, every report enforced with no limit miss, and the failure search
  printed nothing. The byte-pressure course's selection misses are recorded but
  not gated, since it is a heap-sampled resource course.
- **macOS**: passed at the same commit before the request.

The unattended scope is complete. What remains needs a person: nine Accept or
Reject items in the existing attended-review session, one per platform for
each native measurement. Each states its threshold, the measuring method, the
package build command with absolute paths, and what to record. All are bound
to revision `6a573ac7f`, package version 0.1.3.

| Item | Threshold |
|---|---|
| `BPR-0967-packaged-launch-{mac,win,linux}-v001` | Home accepts input within 1,000 ms of activation on three warm launches, 3,000 ms on the first after a reboot |
| `BPR-0967-presentation-{mac,win,linux}-v001` | First presented frame within 100 ms for drawer, theme, next change and large-file selection; large file fully drawn within 2,500 ms |
| `BPR-0967-physical-input-{mac,win,linux}-v001` | Physical actuation to visible response at most 100 ms in each of five trials per action |

A Reject with a note either reports a miss or proposes a different threshold.
Either becomes the next engineering step. The card parks on `human-review`.
