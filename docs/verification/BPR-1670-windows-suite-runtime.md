# Native Windows suite runtime investigation

[[BPR-1670]] implements owner mandate `windows-suite-speedup-r1`:
“Have an implementer look into why the Windows suite takes almost 3 hours and
speed it up without weakening the tests.” This investigation remains in
progress. No speedup or complete candidate acceptance is claimed yet.

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

Some apparent sharing opportunities are unsafe: workbench scale rows write
some common PNG names; timed composed interaction cases carry 100 ms limits;
restoration phases must actually restart and some later phases depend on
earlier catalog/vault bytes. Candidate scheduling must audit outputs, private
profile/store/vault ownership, native desktop use, metrics and deadlines,
and preserve existing resource locks and fixture dependencies.

The coordinator authorized a bounded explicit correctness allowlist after this
decomposition. The proposed 45-row cohort accounts for 2246.997 seconds
(37.45 minutes) of retained exclusive work. Its ideal four-slot work bound is
9.36 minutes; that is a lower bound, not a speedup prediction. Resource locks
and contention can reduce the benefit. All other courses remain exclusive by
default. The pilot applies to Windows; other platforms retain their runtime
schedule.

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
exclusivity. Eight runner/configuration contract methods pass locally, including
positive graph checks for preserved locks, fixtures, labels and deadlines.
Native serial-versus-concurrent timing, remaining-cohort audit and full native
acceptance are still pending. The uncommitted path experiment was discarded without native
compilation. The unchanged history course already in flight is retained.
No full gate is justified by the checker gain alone.
The unchanged history control passed in 425.723 seconds (170.949 seconds of
benchmark-target build separately), with 68 preparation tails totaling 267.557
seconds and 961 account requests. It confirms the retained course is reproducible;
no path or preparation optimization was adopted.

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

## Bounded experiments and pending acceptance

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
course remains pending; the direct-checker saving alone is insufficient to
claim a material reduction of the full suite.

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

The card and authored milestone remain In Progress. Full native acceptance,
final evidence hashes, self-review and integration upkeep remain open.
RFC-BPR-0038 native/security qualification, the release's known limitations and
publication authority are outside this mandate and remain unchanged.
