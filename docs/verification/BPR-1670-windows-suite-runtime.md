# Native Windows suite runtime investigation

[[BPR-1670]] implements owner mandate `windows-suite-speedup-r1`:
“Have an implementer look into why the Windows suite takes almost 3 hours and
speed it up without weakening the tests.” The bounded correction is verified
by matched native controls, one complete native course, and focused recovery
of a missing portable tooling prerequisite. The raw complete course remains
failed evidence; it is not presented as a zero-exit full invocation.

## Final native result and qualification boundary

| Measurement | Retained release | Bounded candidate |
|---|---:|---:|
| Full CTest wall | 9563.164 s / 159.39 min | 8161.116 s / 136.02 min |
| Summed row durations | 11649.085 s | 11462.178 s |
| Serial rows / summed duration | 408 / 8866.900 s | 374 / 6889.284 s |
| Complete inventory | 784 | 784, no missing or extra rows |
| Raw pass / fail / configured Debug skip | 783 / 0 / 1 | 780 / 3 / 1 |
| Raw CTest exit | 0 | 8, retained unchanged |

The observed full-course difference is 1402.048 seconds (23.37 minutes, 14.66%).
It is consistent with the same-binary scheduling discriminator below, but it is
not an exact same-source causal estimate: the release and candidate sources,
cost history and warm nested build differ. The four-row matched control is the
causal scheduling evidence. No complete release-time saving or 30-minute
release is claimed.

The complete course ran at `d989efb5cebabbb180bed7faaf8fecee66d66ca5`, four jobs,
Qt 6.12 Debug, in the approved native interactive context. Source stayed clean.
Both prior real release regressions, review navigation and diff zoom, passed.
The only skip remains `application_large_review_cache_bytes`, configured for
Debug in both manifests. All eight composition methods passed in 118.855 seconds
(119.00 seconds CTest); the transcript records `COMPOSITION_BUILD_JOBS: 8`.
This reused the existing nested compiler outputs; it does not measure cold
speedup. No assertion, workload, timeout, security boundary or test was removed.

The three failures were `documentation_structure`,
`documentation_issue_graph_contract` and `issue_id_allocation_contract`:
the isolated clone omitted the declared, ignored implement-rfc test tooling.
Their combined failed duration was 0.41625 seconds. Native MASK's ordinary
offline preview resolved every package to the committed lock, including
implement-rfc commit `5285995cf454d806a549817b0f3934d9c4c34292`, payload
`728ef9c6f6cd3061b1bce99147cc0d0c348f61e9207b1c191789860ea6a3f3c3`.
Initial role preparation refused the bundle-clone origin spelling. Correcting
only this owned clone's origin metadata to the canonical project URL, followed
by matching-plan repair, passed native `status --check`. There was no fetch,
upgrade, global credential change or producer mutation. Agent host loading
remains explicitly unverified and is not a test prerequisite claim.

At readiness candidate `6757e91b0c4d59eb2ac42e8509071c4f3d525e05`, the actual
three checks passed as prerequisite preflight in 23.826 seconds, then all four
selected rows (the three checks and updated runner contract) passed in 5.939
seconds; total runner wall was 29.866 seconds. The runner contract retains its
original nine methods and adds three prerequisite controls, including a real
CTest failure/success sentinel. Failed, skipped, incomplete or unexpected
preflight results cannot start the expensive graph. Passing preflight still
runs every row in the selected full inventory, including the prerequisite rows
again; report-only and graphs without these rows keep their existing behavior.
The lock declaration is recorded; deployment verification remains MASK's job.

The recovery's exact source delta is two Python files and two operator documents.
No CMake registration, runtime payload or fixture changed. Its actual default
784-row manifest equals the completed run's manifest. All 472 maintained
executable/DLL identities match before and after the complete course and focused
recovery, including Qt dependencies. The earlier configure-only helper exception
is precisely scoped below; recovery performed no configure or build. Optional
bpr-cycle/pr-tour deployment-copy rows are absent in both 784-row configurations,
so no missing optional branch is being treated as equivalent coverage.

Qualification is combined evidence: the complete native runtime course plus
all three repaired prerequisite checks and the new orchestration controls.
The raw full invocation still has exit 8 and its three failures. No second
136-minute invocation was spent solely on restored ignored tooling and this
Python-only early guard. Pre-build commands invoke the actual owning checks;
the runner also retains separate preflight, full CTest and total wall timings.

Raw full tar SHA-256:
`f71c5896e7727eeb742063c7c7fdbf05c35c2f6222d3e4da0a8311c3c0b6fb6e`.
Focused recovery, bootstrap/status, source/binary/graph bridge and exact
16-task cleanup tar SHA-256:
`12fdd8f1a92f4442eefb85968ca07c4b3feacffd7963fa3733f5c2ebf5c12562`.
Both native and collected local hashes match. The source/build roots remain
retained; all task-owned native commands and collectors finished, no owned
process remained, and the PC lease was returned. No human acceptance is pending.

## Release target and future portfolio work

The owner subsequently set a 30-minute target from release request through
verified publication, including builds, native qualification, signing, audit,
source assets, scans, review and public downloads. The owner also requested an
Astra Max read-only architecture/portfolio peer to examine necessity, overlap
and cost of tests. This is a user-authorized design peer, not an additional
engineering implementer. This bounded milestone preserves the existing graph;
it does not adopt a reduced portfolio or silently waive any acceptance.

The retained authoritative future design is external at
`/Users/jaeger/Documents/Codex/releases/release-30min-design-20261020/architecture.md`,
with `analysis.json`, `updater-patch-impact.json` and `retention.json` alongside.
Report SHA-256:
`ad0157d3693bf0ffda63d71504e7ee76c59a90c4fcb9185cfbff6ee0764812d1`.
The remaining 114.82 minutes of exclusive work and 191.04 minutes of summed
Windows work cannot fit 30 minutes merely by selecting four same-host jobs.
The design proposes a regression-owned portfolio, exact-input qualification
reuse, better build invalidation and independent native capacity with explicit
proof requirements. These are future decisions and engineering, not delivered
savings. For the simulated updater patch, broad diagnostic-stamp invalidation
adds only one target / 1.68 seconds beyond its already-large direct target
closure; it does not explain away that patch's actual qualification cost.

Cold work is not hidden outside the clock. This investigation's isolated Debug
remainder alone took 515.489 seconds (744 objects already existed); the failed
cold composition attempt took 1302.970 seconds, including 1132.090 seconds of
Ninja commands. Adding just those two known stages to the final full course is
9979.574 seconds / 166.33 minutes, before earlier bootstrap, focused diagnosis,
guards and recovery. This is a lower bound on the investigation's actual work,
not an uninterrupted clean release benchmark. A clean bootstrap-to-publication
course was not measured here, and no cold compiler saving is claimed. The
bounded changes address overbroad scheduling, early invalid-candidate feedback,
and the existing compiler-job setting; they do not meet the new release target.

## Owner amendment: systemic diagnosis

The owner redirected the work to `windows-suite-speedup-r2` after the local
checker/Qt experiments: diagnose the systemic build, suite and release critical
path before further implementation. Blanket GUI serialization may be corrected
only with source and native causal proof of equivalent isolation. Performance
and global-resource courses remain exclusive; the graph, assertions, workloads,
thresholds, diagnostics and security boundaries remain unchanged.

The final native Windows wrapper ran 10:22:14.814–13:11:06.450 UTC
(10131.636 seconds). CTest consumed 9563.164 seconds; approximately 568.472
seconds (9.47 minutes) were configure/build/bridge overhead. These receipts
do not attribute that overhead among compiler, linker and configuration phases.
Calling the entire interval compilation hides the principal cost.

`tests/CMakeLists.txt` applies `RUN_SERIAL` to every row under `adapters_qt` or
`end_to_end`, or whose environment names `QT_QPA_PLATFORM`. The complete-suite
runner separately enforces the same heuristic. It classifies process-isolated
widget correctness, resource-scoped lifecycle courses, build/graph contracts
and genuine performance courses together.

| Native Windows serial category | Rows | Summed/exclusive seconds | Minutes |
|---|---:|---:|---:|
| Qt binaries | 187 | 3015.846 | 50.26 |
| End-to-end binaries | 177 | 3383.492 | 56.39 |
| End-to-end Python drivers | 41 | 2353.743 | 39.23 |
| Other serial rows | 3 | 113.819 | 1.90 |
| Total | 408 | 8866.900 | 147.78 |

For the same Windows-selected names, Qt totals are 619.06 seconds on Mac and
615.25 seconds on Ubuntu, versus 3064.19 seconds on Windows (including the
Qt Python driver). Different hardware/OS/Debug libraries and diagnostics prevent
a causal compiler/configuration claim. No Debug Qt or diagnostic switch is
proposed. The slow Windows work multiplies the cost of overbroad exclusivity.

Restoration phases must actually restart, and some later phases depend on
earlier catalog/vault bytes. An earlier binary-wide audit incorrectly attributed
a 100 ms drawer assertion and fixed PNG names to the default workbench scale
commands. The assertion belongs to `--conversation-reveal`; the fixed ribbon
render belongs to `--center-ribbon-width`. Default render helpers prefix their
scale-specific output basename. No collision among those three actual default
commands has been established. They remain conservatively exclusive in this
bounded cohort; a future exact-argv/reachable-output audit may justify more.
This correction does not change the retained policy or raw native evidence. Candidate scheduling must audit outputs, private
profile/store/vault ownership, native desktop use, metrics and deadlines,
and preserve existing resource locks and fixture dependencies.

The coordinator authorized a bounded explicit correctness allowlist after this
decomposition. Source audit narrowed the original 45-row proposal to 34 rows
accounting for 1885.674 seconds (31.43 minutes) of retained exclusive work.
Nine Markdown scale rows retain active 1000/1500 ms ceilings; the diagnostic
wrapper retains its child 10 ms cancellation course and shared Qt syntax-canary
location. The read-only symbol inspector stays serial because its retained
23.46 seconds leaves little margin beneath its unchanged 30-second backstop.
The ideal four-slot work bound is 7.86 minutes, and the largest resource group
is 7.89 minutes; these are lower bounds, not a speedup prediction. All other
courses remain exclusive by default. Other platforms retain their runtime
schedule. No full-suite saving is inferred from those bounds.

The first same-binary discriminator uses four independent executable families:
conversation model properties (`QCoreApplication`, no GUI or effect port),
pending states, device launch renewal, and overview summary at 2.0 scale. The
three desktop correctness fixtures each use a distinct owner-private temporary
root and fixture providers; credential material stays in memory fixture vaults.
Their elapsed clocks are unchanged readiness deadlines/settles, not performance
acceptance. Overview renders have a scale-specific output root. Locks name the
executable/output resource; they append to existing locks and retain fixtures.
The maintained Windows AppContainer helper already serializes its shared test
profile creation/check under its existing named mutex; that boundary is
unchanged. No owner profile, keychain, account or live network is selected.

The configuration/runner policy fails closed for missing declarations, wrong
platforms, platform overrides, missing locks, unknown modes and explicit
exclusivity. Self-review also rejects differently cased environment aliases,
which Windows treats as the same name: a retained negative control shows the
previous guard admitted `QT_QPA_PLATFORM=offscreen` plus
`qt_qpa_platform=windows`, while the corrected guard refuses it. No maintained
row uses such an alias; its metadata and native course are unchanged.
The initial nine runner/configuration contract methods pass locally, including
positive graph checks for preserved locks, fixtures, labels and deadlines.
The four-row native causal control and expanded 34-row course pass; final
complete-course and focused recovery evidence is recorded above. The uncommitted path experiment was discarded without
native compilation. The unchanged history course already in flight is retained.
No full gate is justified by the checker gain alone.
The unchanged history control passed in 425.723 seconds (170.949 seconds of
benchmark-target build separately), with 68 preparation tails totaling 267.557
seconds and 961 account requests. It confirms the retained course is reproducible;
no path or preparation optimization was adopted.

## Same-binary scheduling discriminator

The isolated native baseline at `28887de9912476b17f00269787b1b8ab299255a1`
built only the four target families in 80.368 seconds, separately from test time.
Candidate `757a8795cf8661e3a4dcc996ba62685437b9288d` reconfigured in 28.105
seconds and rebuilt no executable. SHA-256 guards match all four executables,
three Qt Debug DLLs and `bpr_xdiff.dll` before/after both courses. Both use the
same isolated paths, interactive Windows context, Debug Qt/toolchain and four
CTest jobs. No competing PC work ran during the controls.

| Course | Original serial seconds | Declared concurrent seconds |
|---|---:|---:|
| Conversation model properties | 84.96 | 99.71 |
| Device launch renewal | 119.72 | 122.52 |
| Overview summary 2.0 | 155.24 | 169.29 |
| Pending states | 83.07 | 83.13 |
| Summed row time | 442.99 | 474.65 |
| CTest wall time | 443.07 | 169.36 |
| Result | 4 pass | 4 pass |

The measured focused wall reduction is 273.71 seconds (61.8%), while summed work
rose 7.1% under contention. Every original limit and assertion passes. This
establishes the scheduling cause for these four rows, not a full-suite outcome.
A 784-row graph/property comparison permits only the four scheduling changes
and provenance labels on already-explicit serial rows; commands, configuration,
environment, timeouts, fixtures, verdict expressions, existing locks and
containment remain identical. Both manifests pass the production runner policy.

Native/local tar SHA-256 receipts:

- Original serial: `1eb0bd60d754373cf52e6c4ca3d619b03f52dc73b46538d4b47bd72faa6edf78`.
- Concurrent pilot: `bb40e1a40227053c1edec99c6acf10cbee85781246404218eb0a9019d9e9a412`.

A later unrelated CTest started at 21:34:48 UTC, after the pilot completed at
21:34:00.603 UTC. The collector refused its quiescence guard, so the failed
collection attempt is retained. Collection then succeeded after observation
confirmed quiescence; neither native course was rerun and the unrelated process
was untouched.

Candidate `33b828bb3557463eff6a3ba03fa81467bed97e36` passes all 34 audited
rows in 648.88 seconds of native CTest wall time (1952.73 seconds summed),
versus 1885.674 seconds of retained exclusive work for those names. This is a
larger passing discriminator, not a matched 34-row A/B or full-suite result.
Its 784-row property audit permits exactly 34 scheduling changes and 40
provenance labels on previously serial rows; all other properties are identical.
The preceding remaining Debug build took 515.489 seconds and created/updated
879 objects (744 already existed, 1623 afterward). This is incremental bootstrap
evidence, not a clean-zero build measurement.

## Bounded ownership audit

The explicit declarations enumerate every admitted row in `tests/CMakeLists.txt`.
The table groups identical ownership evidence; resource locks extend over the
entire CTest command, including every ordered child-process phase.

| Audited family/resource | Rows | Retained serial seconds | Independent ownership and preserved acceptance |
|---|---:|---:|---|
| Conversation model | 1 | 84.050 | `QCoreApplication`, in-memory model/property permutations and persistent indexes; no GUI/effect/file port or performance bound. |
| Qt render directory | 20 | 470.961 | Overview selectors, Settings, Home list, surface capture, Discuss-only and cycle view use process-private widgets/fake ports, offscreen screens/focus/clipboard, and no timed acceptance. One conservative common render-directory lock serializes these families with each other and protects optional/shared PNG names. |
| Portable epoch catalogs | 1 | 81.202 | Driver creates/narrows independent temporary catalogs. Fixture root is passed into composition; Git observes only its cache with global/system config disabled. All twelve ordered launches and catalog/security assertions remain. |
| Binary dependency inspection | 1 | 0.250 | Read-only dependency tool against immutable built executable; no application launch/profile/provider. |
| Device launch renewal | 1 | 119.398 | Per-case private roots and fresh in-memory fixture vault; original launch/renewal/refusal assertions and readiness bounds. Pilot passes concurrently. |
| Profile layout | 1 | 42.953 | Private root, fixture vault/transport, process-local offscreen geometry; ordered seed/restore/verify/broken-store phases and exact SQLite rows remain. |
| Overview summary | 3 | 422.712 | Private per-invocation root and fixture server state; all scales retain assertions, readiness bounds and scale-specific renders. One executable/output lock; 2.0 pilot passes. |
| Review restoration/tabs | 2 | 473.393 | Independent private roots, pipe-only synthetic vault, exact private catalog/cache state. Shared executable lock; every real restart and phase dependency remains. |
| Offline inventory/account | 2 | 107.646 | Private roots and fixture credential slots travel only through bounded pipes; shared executable/output lock; original identity, corrupt/missing-cache and offline assertions remain. |
| Diagnostic contract controls | 1 | 0.184 | Python unit controls mock subprocess calls; they launch no diagnostic child or shared canary course. |
| Pending states | 1 | 82.925 | Private root, in-memory fixture providers/vault and process-local accessibility handler; unchanged readiness/settle bounds. Pilot passes concurrently. |

The source files are the corresponding `tests/adapters_qt/*_tests.cpp`,
`tests/end_to_end/*_public_path_tests.cpp` and their named Python drivers.
Shared pointer input (`tests/support/window_system_mouse.hpp`) dispatches into
that process's Qt window; it sends no OS desktop input. The cursor check reads
its offscreen window's cursor. Clipboard isolation is source-backed:
[Qt 6.12 offscreen integration](https://raw.githubusercontent.com/qt/qtbase/v6.12.0/src/plugins/platforms/offscreen/qoffscreenintegration.h)
inherits the [default platform clipboard](https://raw.githubusercontent.com/qt/qtbase/v6.12.0/src/gui/kernel/qplatformintegration.cpp),
whose [storage is process-static memory](https://raw.githubusercontent.com/qt/qtbase/v6.12.0/src/gui/kernel/qplatformclipboard.cpp).
None of these admitted courses selects an owner's profile, OS vault, registry,
real account, live endpoint or global desktop. The existing AppContainer profile
mutex remains unchanged; no cross-request authorization cache is introduced.

Only offline/account had an explicit per-row `RUN_SERIAL TRUE` before the
blanket fallback. Owning commits `b3197149a` and `da3eea5c56` added that flag
alongside adjacent GUI registrations without a recorded timing/global-resource reason;
the actual courses above own their data/vault and have no elapsed acceptance.
Their Windows registration now defers classification to the audited locked
policy. Other platforms retain their explicit flag. The helper still refuses
to override every remaining explicit exclusion. Nine timed Markdown scale rows,
full workbench/gesture/performance courses, native credential/desktop courses,
shared canary and all unclassified rows stay exclusive.

## Build and release critical path

The retained source explains a separate cold cost: `application_composition_contract`
reconfigured a persistent tests-disabled Ninja Release tree, built the real
application with two jobs, and reran all eight methods plus binary/runtime
checks. It reuses compiler outputs, never a verdict. The earlier Windows row
was 754.068 seconds with a cold tree; the final baseline was 226.244 seconds
warm. The isolated candidate needs its own cold tree. Record that bootstrap
separately and rerun the unchanged row in the matched warm complete gate;
include clean bootstrap plus qualification in end-to-end accounting. Moving
compilation outside a suite timer is not a runtime improvement.

The isolated cold row at `33b828bb3` actually failed after 1302.97 seconds
(21:59:48–22:21:31 UTC). Its first method rejected a stale production-app
source-manifest pin, yet unittest continued through seven later methods,
including the cold nested Release build. Native CTest exited 8; the task
correctly propagated failure. This is retained failure evidence, not a passing
bootstrap or a bridge stall. Native/local preparation tar SHA-256 is
`d84ad4d6c90d04fcf72ab347ad9a0eb716c62cb3414bdb57424270bcf1df9fad`.

The source cause predates this milestone: from last pin edit `c5a906abe` through
`aac127fa4`, only `c0d404c2c` changes the pinned app scope, in
`release_update_desktop.cpp`. BPR-1669 adds the public release-only account and
fallback when the chosen binding is absent. Source review confirms the existing
capability, closing and acquisition-generation guards remain; review mutation
and publication composition are unchanged. Its owning card records that review.
The manifest pin changes from `4b0e7b07…` to the independently reproduced native
and local value `7f11c30f…`; the full-application and production mutation-branch
pins stay unchanged. Printing a digest alone is not acceptance.

The bounded feedback correction uses unittest's existing fail-fast runner.
Passing candidates retain all eight original method bodies and assertions.
A failed source tripwire now exits nonzero before the later expensive build and
prints `COMPOSITION_FAILED_CANDIDATE_NOT_RUN` with the remaining method count.
An external control uses the actual first method against an isolated altered
app tree: fail-fast exits 1 after one method, explicitly leaves seven unrun,
and never reaches the expensive-build sentinel. Disabling fail-fast in the
control reaches that sentinel and runs eight methods, still exiting 1. These
controls prove failure orchestration only; the complete accepted native row
remains required. Control script SHA-256 is
`d8be74359286b64d479324d02058dc87ff7df3db940d5e11a277445e0757a83a`;
retained output SHA-256 is
`433abbacdcaffe6a826217ae95dd494f50646ba32c535a7ad50d77cbf536257a`.
The resealed actual positive/adversarial method passes locally in 1.607 seconds.
The documented selected-method invocation gives early feedback before a full
gate that might schedule composition late; it reuses this owning check and
never claims to replace the other seven methods or the native build.

The cold Ninja log spans 1132.09 seconds (86.9% of the failed row), with
2258.501 seconds of summed command intervals: the hardcoded two build workers
were occupied. The literal dates to original commit `d9cec406af`; its owning
card/history records no memory, isolation or acceptance rationale. The station
has 20 logical CPUs and 137.23 GB RAM, and its existing producer bridge selects
eight build jobs. The driver now respects `CMAKE_BUILD_PARALLEL_LEVEL` with an
eight-job ceiling, defaults to two when unset/empty, and rejects invalid or
nonpositive values. The row remains exclusive; the compiler subprocess exits
before every original binary/runtime check. No build input/target/configuration,
assertion or timeout changes, and no tree is discarded for another cold build.
The final transcript records eight jobs; cold speedup remains
unmeasured and is not inferred from the worker-count ratio.
Nine maintained runner-contract methods pass locally, including the new
unset/empty/valid/clamped/invalid job-setting cases. An external control also
executes the actual build-launch statements: valid settings produce the
original Release target/flags with only the bounded job value changed; invalid
settings fail before launching CMake. Script/output SHA-256:
`48ce4145874b01f4d79787aec78101581bf519187988327120e617592585ae30` /
`155769c5d96906c179110b8316b0f7f5d95c3a12b95e6c6041916edea48bc02f`.

Two strict guard attempts are retained separately from product results.
The first refused 212 command differences because CTest had omitted commands
for then-unbuilt executables in the earlier manifest. Every difference was
absent-to-resolved; all previously resolved commands matched. A configure-only
original-policy control against the now-built files restores comparable command
inventory, then restores the exact candidate. The second graph audit passes all
784 rows and the full incremental build passes in 18.417 seconds with zero
object changes. All 472 maintained executable/DLL hashes match. Only
`CMakeFiles/bpr-appcontainer-setup.exe` changes: unchanged
`cmake/WindowsTestIsolationSetup.cmake` deliberately recompiles it with
`try_run(COPY_FILE)` from unchanged
`tests/network_isolation/windows_network_sandbox.cpp` on each configure.
Those source SHA-256 identities are respectively
`3466fbaf1269ea08b933e7c525b4eacebb8b54f56e9778d22daa0576eb5bed07` and
`3db73e1693eee0425dbd0f10ccbf032fbed97107db3783ffc7092ce74018613a`.
The final identity guard binds this exact configure helper exception and its
unchanged origin; application, test and dependency binaries remain guarded.
Raw guard-attempt tar SHA-256 values are
`c1d16753cc808af1318879e0f611a9e64d0736a04adbe0cc2cf3f9344029f2c9` and
`709d201ec92193e21372b27b9b7954f06ca13662ead0b7ea0ab5e8a7980e88a6`.

The normal Debug, tests-enabled optimized course, tests-disabled composition
contract and platform distribution graphs have different configuration/artifact
acceptance. Their canonical immutable dependency inputs may be reused after
identity checks. The composition contract does not currently bind the packaged
producer's platform policy/options to its generic Release graph, so treating
one as a valid substitute for the other would require a separate equivalence
contract. This milestone diagnoses that duplication and introduces no unchecked
binary or verdict reuse.

| Retained release phase | Elapsed evidence | Critical-path implication |
|---|---:|---|
| Final Windows normal wrapper | 168.86 min, of which suite 159.39 min | PC lane dominated by suite; combined configure/build/bridge 9.47 min cannot be split further from these receipts. |
| Final Mac gate/producer | 63.92 min, of which suite 52.51 min | Independent lane overlapped PC; do not add durations as total delay. |
| Final Ubuntu normal suite | 54.60 min | Same physical PC as Windows; performance-sensitive lanes require exclusive ownership. |
| Windows optimized wrapper | 9.00 min; build 252.669 s/course 282.294 s | Separate tests-enabled optimized graph; retains its original course. |
| Earlier failed Mac/Windows suites | 228.91 min aggregate, overlapping | Obsolete-source work had real failures; early changed public courses can expose them before expensive gates. |
| PC material handoff/handback | about 155 min | Technical work overlapped; this is neither measured human effort nor wholly idle time. |
| Publisher / anonymous downloads | 34.129 / 33.778 s | Small measured stages; not the dominant cost. |

The complete non-overlapping DAG from the original Qt upgrade request is not
retained, so no invented total release duration or saving is reported. Workflow
commit `89fde0df1` is preserved on this branch and its execution reference is
reconciled with the audited policy. It already covers early public feedback,
common freeze, independent Mac/PC lanes, continuation records, review overlap
and failed-layer collection recovery; this milestone does not take credit for
those procedural changes or claim a measured release improvement.

## Retained baseline

The exact v0.3.1 source `2ed7e5f98641d75678f861dad0edbe658b5f5d22`
completed the native Windows `windows-dev` Debug graph with Qt 6.12.0 and
four jobs. The retained final receipts distinguish elapsed time from overlapping
row durations; build/configuration time is additional.

| Measurement | Final passing baseline |
|---|---:|
| Completed rows | 784 |
| Passed / failed / configured Debug skip | 783 / 0 / 1 |
| Wall time | 9563.163747 s (159.39 min) |
| Summed test durations | 11649.0845232 s (194.15 min) |
| Serial rows / summed duration | 408 / 8866.9004852 s (147.78 min) |
| Other rows / summed duration | 376 / 2782.184038 s |

Serial application and Qt courses dominate elapsed time. Their 147.78 minutes
plus approximately 11.60 minutes for the concurrently scheduled remainder
explain the 159.39-minute wall result; summed durations cannot be read as wall
time. The earlier 10129.385-second run had two failures on an earlier candidate
and remains failed evidence, not a second passing baseline.

The largest rows are `architecture_boundary_contract` (621.838 s),
`application_large_review_history` (428.245 s),
`application_profile_window_review_restoration_public_path` (412.091 s),
`application_composition_contract` (226.244 s), and the three workbench scale
courses (186.499 / 196.024 / 215.273 s at 1.0 / 1.25 / 2.0).

## Focused causal observations

The retained history course records 424.265 seconds of timed phases, including
270.901 seconds in 68 background preparation tails. These grow with the 32
retained epochs. This is a separate lead into exact cache/preparation work;
no workload, preparation drain or security validation has been removed.

A native checker profile identifies 43,329 source reads in effect-owner rules:
10.05 seconds reopening files and 14.30 seconds matching the rules, within a
47.92-second diagnostic trace. Two selected architecture methods exercise ten
positive/adversarial checker invocations in 1.982 seconds; fixture writes and
cleanup take roughly 0.3 seconds. These focused controls do not account for
all 159 methods in the full architecture contract.

An exact SHA-256-verified copy of the retained workbench test executable passes
the same 1.0-scale course in 188.717 seconds, close to the retained 186.499
seconds. Diagnostic CPU sampling of that process attributes 48.21% inclusive
CPU to workbench construction, including 22.80% in `build_github_header` and
21.66% in `build_central_surface`. Settings panel adoption accounts for 7.05%
of sampled CPU. Inclusive percentages overlap. The raw sampler has incomplete
Qt symbols; application caller symbols come from the matching PDB. No compiler,
Debug Qt library or diagnostic configuration has been weakened.

## Bounded experiments

The Settings parent experiment was rejected and reverted in `28887de99`.
It constructed the same nine Settings roots under their final page parents.
The matched native workbench course passed but took 188.093 seconds versus
186.054 seconds for the baseline; geometry passed in 1.8851 versus 1.87132
seconds. All 53 baseline/candidate PNGs have identical decoded pixels.
Frontend-design and its desktop guidance were read before the experiment.
No Qt change remains in the engineering candidate.

A separate checker experiment reads each source afresh once per invocation and
shares those bytes among the existing effect-owner rules. It caches no verdict
between invocations and preserves every rule, allowed owner and diagnostic.
All 159 architecture adversarial methods pass on the Mac (236.325 seconds).
The matched native real checker passes in 31.7367 seconds versus 41.0335
seconds, a 9.2968-second (22.7%) saving. The native 159-method adversarial
course passes in 472.564 seconds in isolation. Its historical 621.838 seconds
in the complete graph overlapped other work and is not a matched causal
comparison; the direct-checker saving alone is insufficient to claim a material
reduction of the full suite.

The native baseline and candidate build trees are isolated from the frozen
published producer. The cold isolated build must first produce its own
prerequisites; incremental candidate builds then change only the selected
source. Initial cold configure/Qt build took 255.6854/294.9737 seconds;
candidate reconfigure/incremental Qt build took 35.5504/28.0294 seconds
(two compilation lines). These setup costs are additional to the measured
suite. Matched focused controls precede one full four-job candidate suite.
The graph, justified exclusivity/resource scheduling, locks, fixtures, workloads, assertions,
limits, timeouts, containment and network isolation remain acceptance obligations.

## Retained attempts and continuation

Raw reports, commands, profiles and receipts remain outside Git under
`/tmp/bpr-windows-suite-speedup` on the Mac and
`C:/Users/mikem/bpr1670-suite/evidence` on Windows. The original complete-run
receipts remain in the release investigation's external evidence directory.
The native source checkout is `C:/Users/mikem/bpr1670-suite/repo`.

The first checker profiling command used backslashes in `BPR_ROOT` and failed
the checker's self-file exclusion; that unsuccessful trace is retained. The
corrected command passes. The first in-task profiler creation also reported
failure despite exit zero; it supplies no accepted CPU sample. A separate
explicit profiler session supplied the target-process sample and was stopped
and collected. All actual failures retain their original meaning.

The bounded milestone uses the complete course and focused prerequisite
recovery above. Self-review found no net production/fixture change or weakened
acceptance; intentional tracker closeout records the exact evidence boundary.
Main integration belongs to the coordinator. The unrelated local MASK
declaration changes remain outside this slice.
RFC-BPR-0038 native/security qualification, the release's known limitations and
publication authority are outside this mandate and remain unchanged.
