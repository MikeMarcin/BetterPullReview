# BPR-0967 remaining Mac measurements — 2026-09-25

Assignment `/root/bpr0967_full_mac`; mandate
`r4-standalone-mac-20260925-full-remaining-release`. This is one continuing
engineering milestone across the remaining Mac dimensions. Windows and Linux
are expressly deferred; the complete card and AC3 remain unfinished.

## Configuration and acceptance boundary

The user corrected the earlier performance methodology during this milestone.
**Debug runs are diagnostic and correctness evidence only.** Their timings and
memory observations do not justify user-facing budgets or Mac performance
acceptance. Historical reports are preserved at their original candidates.

The optimized candidate is `5a26d6b1b33a90d05072e953479561859d7ac7b3`. Its fixture composition uses
Release (`-O3 -DNDEBUG`, `QT_NO_DEBUG`), native arm64, minimum macOS 13.0,
Apple clang and Qt 6.11.2. Those compiler optimization and deployment settings
match the arm64 slice of the project's `macos-release` policy. That distribution
preset builds a universal package with tests disabled. This measurement instead
requires `BUILD_TESTING=ON` and `BPR_DISTRIBUTION_BUILD=OFF` to select safe stored
fixtures. It is an optimized real composition, not the packaged executable.
No production fixture selector was added and no signing/release work was done.

Station: Apple M4 Pro, 48 GiB, 14 logical processors, macOS 26.6.2 (25G83).
The GUI courses use offscreen/Fusion, 1600 × 1000, DPR 1. Courses run serially
under the repository's network-denial runner and a process-scoped `caffeinate`
assertion. File-system caches are not purged; no cold-boot launch is claimed.
Inherited MASK preflight is incomplete/state.drift and host load is
`not_verified`; this work does not claim or repair skill readiness.

## Owning change and controls

Compact conversation cards reuse label and action geometry within one layout.
The key includes the complete label plus anchor, draft, resolved, unread and
inline-thread flags. Width, font, feed and action policy are fixed within that
layout. Each card retains its exact source, identity and state. Nothing changes
core decisions, host request ordering, cancellation, cache policy or persistence.
The Qt check compares grouped cards with independently composed single cards at
three widths, including action rectangles, wrapping and different bodies.

Before that repair, the new unchanged 100 ms initial-load gate failed at
108.858 ms in Debug; the first label-height repair passed at 87.301 ms. These
are diagnostic controls, **not optimized acceptance or a Release speedup claim**.
The final geometry reuse also avoids repeated action-label shaping. Optimized
results below independently establish the selected candidate's performance.

The existing timing control still executes real 120 ms synchronous and terminal
stalls. A new native heap control holds and releases a touched 32 MiB allocation
while the GUI thread sleeps; the independent sampler must detect its peak and
release. The launch observer passes six controls: valid, missing marker, reordered
markers, failed child, late main and late Home. A temporary fixture-only change
from 4,096 to 16,384 allowed lines makes the pathological course fail its first
semantic refusal assertion; the source is restored byte-for-byte before the
positive rerun. The control driver expected a later assertion and itself failed,
but its recorded child failure and restoration hashes establish the control. Exact source and fallback
assertions cannot pass on a success label alone.

## Measured boundaries and budgets

The parent clock begins immediately before process creation and ends when a
flushed main-entry or Home-render marker is received. It includes loader work,
pipe delivery and parent scheduling. The child separately measures QApplication
construction, main-to-Home widget capture, public Open-to-provisional metadata,
Open-to-accepted metadata, and main-to-first-source capture. Provisional metadata
must have zero file rows and disabled entry; acceptance must have all 515 rows
and the same exact fixture title. Capture costs are recorded. Main-to-first-source
includes the fixture Connect/load/Read workflow and evidence image writes.

These are widget-render and queued-event boundaries. They do not establish
native compositor presentation, physical input latency, a production account
launch, or continuous CPU occupancy. The public application still does not
accept `--github-fixture large-review-v1`; its supported-fixture/refusal control
is recorded separately from the benchmark's process-start observation.

New local regression bounds, derived from optimized observations with headroom:
process-to-main marker 100 ms; process/main-to-Home render 1,000 ms;
QApplication 50 ms; provisional metadata 100 ms; accepted metadata 250 ms;
main-to-first-source 15,000 ms. The last includes a roughly ten-second fixture
Git-ready course and is not a first-paint promise. Generated-file completion
reuses the 2,500 ms first-source ceiling. Materialization refusal, binary
recovery and supported-source recovery each use 1,500 ms. Provider cancellation
acknowledgement, exit and input retain 100 ms. All observed responsiveness,
active-work and pathological-course phase gaps must now be at most 100 ms.
No previous ceiling is raised. Existing completion, history, cache and cleanup
ceilings are revalidated in Release, independently of their historical Debug
rationale. These station/fixture regression bounds are not universal promises.

Final optimized responsiveness and launch-course measurements (milliseconds):

| Boundary | Direct course | Parent-observed course | Ceiling |
| --- | ---: | ---: | ---: |
| QApplication construction | 2.681 | 2.812 | 50 |
| Main to Home capture | 341.935 | 333.837 | 1,000 |
| Provisional metadata capture | 10.537 | 10.090 | 100 |
| Accepted metadata capture | 67.635 | 65.605 | 250 |
| Main to first source capture | 11779.554 | 11821.038 | 15,000 |

The parent reports **14.590 ms** to the main marker and **348.428 ms** to Home. Both courses pass all 28 named bounds with empty `InputBudgetFailures`.

| Public phase | Direct elapsed ms | Direct max gap ms | Parent-course max gap ms |
| --- | ---: | ---: | ---: |
| `paged_review_and_git_load` | 10090.071 | 27.635 | 30.296 |
| `large_file_selection` | 1229.293 | 71.803 | 71.684 |
| `generated_source_render` | 961.917 | 28.074 | 31.599 |
| `binary_fallback_render` | 943.278 | 9.203 | 11.269 |
| `large_source_revisit_render` | 594.765 | 74.282 | 71.729 |

The final active course applies all 31 queued keys; 16 are delivered with pending diff work. Public close dispatch/drain: 1.821/3.958 ms. All eight active bounds pass and exact final source remains held.

The held-provider course applies 20 keys during 34.099 ms of continuous entry. Stop callback/exit after close: 0.207/0.213 ms. All 25 bounds pass.

Materialization fallback/recovery completion: unavailable_file_selection 966.087 ms, binary_file_selection 938.447 ms, supported_file_recovery 939.255 ms. Each is below 1,500 ms; no 100 ms phase-gap failure is recorded.

## Generated source, fallback and active work

The generated table checks both exact stored blobs and all 8,192 lines through
the visible two-pane composition. The pathological course lowers only the
fixture's materialization limit to 4,096 lines and selects that same table.
It requires the exact `materialization_limit` refusal and complete visible,
accessible, copyable reason; both source panes and selection must be empty.
It then distinguishes binary content and recovers all eight lines, final empty
blocks, exact blob identities and comparison anchors of a supported file.
This tests the real bounded refusal at a reduced fixture limit; it does not
measure a production-default 200,000-line refusal or an extreme long-line file.

The continuous-work course holds the existing fixture provider callback using
the real host stop token, with a ten-second fail-safe. Twenty queued key inputs
must apply to a visible Home field while the operation is still entered and
neither exited nor cancelled. Public window close must reach the entry-point's
last-window-close shutdown relay, the stop callback, provider exit and complete
application drain without accepted snapshot or retained cache. The relay was
initially missing in the new harness; that diagnostic failure is preserved and
fixed before the optimized passing course. This models a waiting provider
operation. The separate real diff supersession course retains exact final source,
request/anchor authority and public close during sampled outstanding work.
Other active Git, persistence, syntax and agent operations remain unmeasured.

## Memory observation and limitations

All four resource courses sample all malloc zones on an independent thread at
a nominal 1 ms interval, from after QApplication construction/before fixture
composition until after application destruction. The independent heap baseline
precedes manifest loading; physical-footprint baselines remain at the existing
`before_application` checkpoint. Reports retain baseline, largest live byte count,
post-destruction bytes, sample count, largest actual interval and largest sample
cost. The sampled peak is a lower bound on the actual live-heap peak. The SDK's
`max_size_in_use` is a touched-memory high-water counter and is deliberately
not substituted for live allocated bytes.

The sampled heap ceiling reuses each course's existing 160/184 MiB footprint
allowance as a conservative separate allocation ceiling. Optimized measured
headroom is recorded below; this is a new sampled-heap check, not an assertion
that physical footprint and live heap are interchangeable. Native process-lifetime
physical-footprint growth is still checked against 160 MiB (ordinary resource,
loaded-close and entry pressure) or 184 MiB (byte pressure). Live cleanup remains
84 MiB with the application open and 32 MiB after destruction. Cache ceilings,
weak-owner expiry, exact eviction/revisit and retained Git/catalog history are
unchanged.

| Optimized course | Sampled heap growth MiB | Native lifetime footprint growth MiB | Destruction live growth MiB | Max sampler interval ms | Peak ceiling MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| resources | 106.136 | 129.282 | 14.171 | 7.649 | 160 |
| loaded-close | 106.136 | 125.297 | 14.110 | 5.900 | 160 |
| cache-entries | 70.924 | 86.985 | 13.330 | 5.753 | 160 |
| cache-bytes | 164.048 | 158.188 | 13.336 | 26.604 | 184 |

All four uninstrumented courses finish shutdown and release the expected owners.
Their sampled heap headroom is 19.952 MiB or more; native footprint headroom is
25.812 MiB or more. The sampler can miss peaks between observations. The byte
course retains all 32 epochs, peaks at 66,839,408 cache bytes and verifies exact
eviction, revisit and supersession. The separate history course retains all 32
epochs, applies 31 selections, peaks at 64 model rows without resets, and records
a 29.669 ms maximum gap. These are optimized fixture regression observations,
not a production memory guarantee.

An initial Allocations probe could not attach. A subsequent optimized probe
succeeded using an owner-private disposable copy of the benchmark, ad-hoc signed
with only `com.apple.security.get-task-allow`, still inside network denial. The
copy and entitlement file were removed by their owning temporary-directory
scope. The original executable was not modified. The trace's loaded-close
course exits successfully. Profiling perturbs allocation and timing behavior;
its timings are excluded from acceptance.

The available Statistics export reports final persistent and cumulative heap
bytes; its default Allocations List contains surviving allocations. Neither is
peak live heap, and no complete allocation/free event reduction was established.
The task-owned Instruments viewer was closed after inspection, with view-only
changes discarded. The trace is retained as diagnostic evidence.

A final bounded native capture uses `MallocStackLoggingNoCompact=1`, with
`MallocStackLogging` unset, and `leaks --noContent --fullStackHistory` against
an owner-private, ad-hoc signed disposable copy. An independently freed, touched
32 MiB control is recovered by `malloc_history -highWaterMark -allBySize` after
its deallocation. Earlier `leaks --atExit` variants either selected lite mode
or compacted the immediately adjacent allocation/free pair; their failed
controls are retained and do not supply peak evidence.

The explicit benchmark `--allocation-profile` mode reuses loaded-close. It
preserves every exact source, cache, Git/catalog and owner-release assertion,
marks `Extended.AllocationProfile=true`, writes no acceptance success marker,
and allows ten seconds for an external capture after application destruction.
Only measured allocation/footprint/cleanup ceilings are bypassed in this
profiler mode; ordinary course limits remain unchanged. This separation is
necessary: instrumentation made the ordinary course exceed its footprint
ceiling before close. The final profiled course completes and releases all
expected owners. Its timing and physical-footprint observations are excluded
from performance acceptance.

The native scan traverses **21,634,453** allocation/VM records and identifies
record **21,403,349** as the joint high-water mark, printed as **116.8 MB of
allocated heap plus anonymous VM**. This is a conservative upper bound for
malloc heap during that captured, instrumented loaded-close course, not the
exact heap-only peak. The stack-group byte sum is 122,999,200 bytes; the tool's
rounded summary and stack-group accounting are retained separately rather
than equated. The 673 MiB no-content memgraph and report remain local, ignored
artifacts. The temporary copy is removed and the original executable is
unchanged. No more profiler variants were run after this bounded attempt.

The other resource courses still have sampled live-heap lower bounds, with
native process-lifetime footprint bounds. Complete heap-only reconstruction
across cache pressure and maintenance remains unmeasured. The profiled result
is not a proof of the uninstrumented process's exact allocation peak.

## Verification, remaining work and ownership

The serial Release integration passes all 26 selected rows after building the
fresh tree's missing smart-transport manifest prerequisite. The first pass is
25/26 in 907.75 seconds; that one failure is retained. The manifest and supported
public application then pass 2/2 in 18.59 seconds. The unsupported large-fixture
public selector exits 1 with `PUBLIC_PATH_FAILED` and no image, as expected.

After adding the isolated profiler mode and restoring the negative control,
the exact final benchmark passes seven ordinary Release courses in 89.32 seconds:
timing/heap control, responsiveness, launch, active work, pathological fallback,
provider cancellation and loaded-close. The longer resource/cache/history rows
reuse the preceding integration against identical product code and ordinary
course behavior; only the explicitly selected profiler branch was added afterward.
Every ordinary final measurement report has profiling disabled; the timing
control reports its assertions directly. Architecture, nine owning/
composed Qt rows (including three scales), ordinary and unavailable/approximate
public paths passed in the integration. Product clang-tidy with warnings as
errors, changed C++ formatting and whitespace checks pass.

Self-review checked cache-key completeness and layout lifetime, sampler
thread/join lifetime, stop-token endpoints, exact fallback identities, source
recovery and profile-mode isolation. No threshold is raised and no product
capability is added for tests. Documentation validation passes, its 64 script tests pass, and the board
validates all 1,186 cards. The full handoff records checkpoint and quiescence
gates plus artifact digests.

Remaining Mac criteria: packaged/native launch and compositor or physical-input
endpoints; complete heap-only peak reconstruction beyond the captured loaded-close bound; full
production-limit pathological/long-line workload; and active-operation coverage
beyond the observed diff and held provider courses. No human, independent,
native-style, package or release acceptance is inferred. Windows/Linux remain
deferred. BPR-0967 returns to Backlog with these facts rather than closing AC3.
The RFC0052 candidate, 14-question session, server and waiter remain protected.

The [full handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff-full-mac.json)
binds exact source, build, report digests, commands, pending obligations and
writer release. This same implementer owns the entire constituent work through
that handoff; no delegation or intermediate owner turnover occurred.
