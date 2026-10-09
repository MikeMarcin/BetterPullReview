# BPR-0967 Mac input and close during outstanding work — 2026-09-25

Assignment `/root/bpr0967_active_work`, mandate `r4-standalone-mac-20260925`,
starts at `b104c7bcf02dd1c0f3eaf4506a1f9a83faac8bcb` on `main` in
`/Users/jaeger/Documents/GitHub/BetterPullReview`. This is a bounded partial
AC3 evidence slice. No product code changes, product repair, relaxed input target
or full BPR-0967 acceptance is claimed. The exact benchmark candidate is
`31a33afb903e7ca9ed539591552269ec948b98f5`; later commits are documentation only.

## Workload and observation boundaries

Apple M4 Pro, 48 GiB (51,539,607,552 bytes), 14 logical processors, macOS
26.6.2 build 25G83, arm64, Debug, Qt 6.11.2, offscreen Fusion, 1600×1000 at
DPR 1. The real composition uses the recorded 515-file/1,000-thread workload,
installed Git, disposable private SQLite/cache roots and enforced OS network
denial with zero loopback ports. Process-scoped `caffeinate -i -s` protects the
courses from the previously observed system-sleep interruption. No live account,
private target, remote mutation or release is involved.

The unchanged baseline responsiveness course passed its 20 existing gates in
24.52 s. It sampled 31 selections with outstanding diff work, with a 939.463 ms
last-selection-to-drain duration. Its initial-load input gap was **113.072 ms**.
The startup predecessor's **108.139042 ms** miss and load predecessor's
**108.991709 ms** miss remain true at their original candidates. The target
remains **100 ms**.

The new course reaches the large exact comparison and its native geometry
batching, opens the visible Files tree, selects Flat using its public combo,
then posts 31 Down keys through Qt's queue. The initial file selection starts
work; subsequent delivered keys drive tree selection, the existing semantic
intent, retained epoch actor and diff host. Each observation retains the exact
expected path and job counts at insertion and delivery. Queue-to-application is
an upper bound through the next timer tick's exact current-path assertion;
queue-to-delivery ends at the event filter before Qt handles the key. Neither
clock starts at the OS input queue.

`OutstandingJobs` counts admitted diff work through its decision terminal,
plus deferred ingress and local decision completions. It does not reveal worker
execution state. A positive sample is actual outstanding host work, but neither
paired samples nor the 5 ms phase timer proves continuous occupancy. The course
requires a delivered key with both pending diff state and outstanding work; it
records all keys, including the ones whose work finished before delivery.

When the final key is delivered, source-change observers begin checking the
held file against the expected complete comparison key and synthetic source.
They do not arm at queue insertion, since prior work may still legitimately
publish before the key arrives. At full drain the source's eight lines and
trailing block, manifest blob OIDs, authority, epoch/anchor identities and
comparison revisions must match. No transient stale final-file publication is
permitted by the observed document-change path.

Amended 2026-10-08 by [[BPR-1344]]: "full drain" here, and the
`active_supersession_final_drain` budget, now end when the reviewer's epoch
jobs reach zero. Preparing files the reviewer has not opened
([[BPR-1327]]) keeps its own epoch jobs running after that; the course waits for
them as a separate phase, reports its length without a limit, and checks the
held source again after it. A second budget, `active_supersession_shown`,
holds the time from the final key until the final file is shown with the diff
quiet to the same 1,250 ms.

Next, a cold file request starts real work. A 1 ms observer waits for pending
diff state plus outstanding work and calls the public window close immediately.
The normal last-window-close handoff begins shutdown. The close clock includes
synchronous dispatch and ends at whole-application shutdown completion, with
zero diff/workbench/epoch work, no pending diff, a quiescent diff host, empty
diff cache, no visible window and no cleanup failure. Exact retained Git refs
and catalog OIDs remain, with zero prune/delete intents. This is an aggregate
close-to-drain observation, not an isolated cancellation acknowledgement or
proof that a worker was executing when its stop token was requested.

## Measured limits and controls

The first corrected course passed in 19.82 s. All 31 keys were posted with
outstanding work; **16/31** still had pending diff work at delivery and **15/31**
did not. Its maxima were 4.374 ms to delivery, 22.369 ms to verified selection,
and 25.379 ms for the complete active-selection phase's input interval.
Final-source observation fired eight times. Close began with one diff and one
epoch job, dispatched in 9.759 ms and drained in 16.568 ms. The gated repeat
passed in 19.78 s, with a 16.837 ms drain.

The eight gates retain the 100 ms target for full active selection/close input
intervals, queued delivery, verified key selection and close dispatch. Rapid
selection completion and final drain reuse 10,000/1,250 ms from the existing
Mac course (first observations 4,513.222/935.478 ms). A new local 100 ms
close-to-quiescence bound is about six times the two measured 16.6–16.8 ms
drains and catches a newly introduced 100 ms synchronous stall. It is a
regression allowance for this declared fixture and machine, not a universal
storage/cancellation deadline. Watchdogs at 1,000 ms for undelivered input and
2,000 ms for missing close admission terminate invalid courses; they do not
replace the stricter measured acceptance limits.

An initial exploratory course incorrectly required the hidden legacy list to
be visible and timed out in 91.56 s. A one-second process stack sample identified
the waiting predicate; that sample perturbs its timing. The actual public Files
tree corrected this harness mistake. It is not a product performance failure.

The wrong-key control replaces the posted Down press with Shift. The original
control correctly failed exact selection but threw through a Qt callback and
aborted; that result is retained. The harness was corrected to leave the Qt loop
normally and produce its partial report. A separate close control replaces the
public close with direct host shutdown: work drains but the visible-window and
public-close assertions fail. Temporary controls are removed before verification.

## Acceptance boundaries and documentation impact

This slice changes the benchmark and its registration only. Core policy,
application scheduling/cancellation/authority, Qt product behavior, provider
protocol, persistence schemas, credentials, package selection and security
boundaries have no runtime change. Existing source/cache/eviction/history,
cleanup and lifetime-peak evidence remains bound to its recorded candidates;
focused checks cover the changed benchmark composition.

The new evidence covers queued file navigation/supersession and public close at
sampled outstanding local diff/materialization work in the large-review context.
It does not prove continuous worker occupancy, isolated cancellation latency,
Git/network/syntax/persistence/AI cancellation at every stage, or long-running
large-file semantic compute cancellation. True process/QApplication launch,
separate metadata/first-native-frame timing, generated/pathological fallback,
peak live heap and broader active-work evidence remain pending. Windows/Linux
stations are explicitly deferred. No independent, attended, native-style,
packaged or release acceptance is supplied. RFC0052's protected 14-question
session and all other worktrees remain untouched.

## Exact candidate verification

Candidate `31a33afb903e7ca9ed539591552269ec948b98f5` built
`build/dev/tests/end_to_end/better_pull_review_large_review` with SHA-256
`4145c13a30de4f9bd66a3102ef9dd7dbc89879e0f6dce1b6303d868f79490fcd`.
The final active course passes in **18.72 s**, with all 31 exact keys applied,
**16/31** delivered with one pending diff job and **15/31** delivered with zero
diff jobs. There are eight final-source observations and two retained diff
cache entries (3,521,024 bytes) before close; close clears that cache.

| Gate | Final measured ms | Unchanged or justified limit ms |
|---|---:|---:|
| `active_selection_input_gap` | 26.270459 | 100 |
| `active_close_input_gap` | 26.272500 | 100 |
| `active_queued_key_delivery` | 4.112625 | 100 |
| `active_queued_key_applied` | 21.516250 | 100 |
| `active_close_dispatch` | 9.466333 | 100 |
| `active_selection_completion` | 4551.101584 | 10000 |
| `active_supersession_final_drain` | 957.060791 | 1250 |
| `active_close_quiescence` | 16.670792 | 100 |

The complete active-close phase is 225.529542 ms, including cold-file admission;
the 16.670792 ms close-to-quiescence timer begins immediately before the public
close after admission. It begins with one diff job and one epoch job. The
active-selection phase has 823 delivered 5 ms timer pulses, 143 with sampled
diff work and 662 with workbench work. These remain samples, not occupied time.

The final active course still reports a **108.424500 ms** initial-load miss.
The companion responsiveness course still reports **110.075125 ms** and passes
its 20 existing gates. Neither replaces or solves the predecessor's 108.139042 ms
miss. There is no new initial-load threshold or product performance repair.

The final focused pass is **6/6 in 86.42 s**: ordinary large-review benchmark,
timing control, responsiveness, active work, loaded close and network-isolation
manifest. Loaded close retains passing ownership/cache/epoch checks, including
released diff/index owners, destroyed window/documents and retained exact epoch
storage. Formatting and `git diff --check` pass. Documentation regression tests
pass **64/64 in 1.895 s**; printed ERROR cases in that suite are expected negative
fixtures. The structure gate and scoped board check are recorded in the handoff.

Commands, executed in the assigned source root:

```sh
cmake --build --preset dev --target bench_better_pull_review_large_review
caffeinate -i -s ctest --preset dev -R '^(application_large_review_(active_work|timing_control|responsiveness|benchmark|loaded_close)|test_network_isolation_manifest)$' -V
python3 _coding-standard/scripts/format_check.py --root . tests/end_to_end/large_review_benchmark.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

The clean wrong-key control exits 1 in 15.18 s with `queued key did not select
the exact next file`; its partial report records delivery but no selected target.
The exact final close-bypass control exits 1 in 19.80 s with
`active close retained jobs, cache, window or cleanup failure`; its partial
report records all 31 keys but no public window close. The positive benchmark
source SHA-256 is `708fac63101f6bec50c5dbb17e0db6670dc59dc379201fd0942812c1d17440c7`.
Both final controls change only their named event/close call against those bytes.
No control or callback exception remains in the committed positive course.

Self-review checked the actual visible-tree event route, pre-handler delivery
samples, the final-gesture boundary for stale-source monitoring, conservative
next-tick selection timing, exact line/blob/authority/anchor checks, explicit
public-close handoff, normal negative-control returns, cache release and retained
storage. Product sources and all previous thresholds remain unchanged. The
corrected course's full-window capture was visually inspected, showing Flat,
selected `src/file_0031.cpp` and both exact source panes; final-candidate source
assertions and capture pass on the same rendering path.

Runtime reports, partial controls and exact log digests are retained under
`build/bpr0967-active-work/` and indexed by the
[durable handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff-active-work.json).
No runtime output, screenshot or build artifact is committed.
