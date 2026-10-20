# Native Windows suite runtime investigation

[[BPR-1670]] implements owner mandate `windows-suite-speedup-r1`:
“Have an implementer look into why the Windows suite takes almost 3 hours and
speed it up without weakening the tests.” This investigation remains in
progress. No speedup or complete candidate acceptance is claimed yet.

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

The first candidate constructs the same nine Settings page roots beneath their
existing final page parents, avoiding later reparenting of populated subtrees.
It retains every widget, layout insertion, dispatch, property and explicit
show operation. Frontend-design and its desktop guidance were read before the
Qt change. Native timing, rendered output, ownership, focus and interaction
acceptance remain pending.

A separate checker experiment reads each source afresh once per invocation and
shares those bytes among the existing effect-owner rules. It caches no verdict
between invocations and preserves every rule, allowed owner and diagnostic.
All 159 architecture adversarial methods pass on the Mac (236.325 seconds).
This is supporting correctness evidence; its actual native impact and native
adversarial acceptance remain pending.

The native baseline and candidate build trees are isolated from the frozen
published producer. The cold isolated build must first produce its own
prerequisites; incremental candidate builds then change only the selected
source. Matched focused controls precede one full four-job candidate suite.
The graph, GUI/performance serialization, locks, fixtures, workloads, assertions,
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
