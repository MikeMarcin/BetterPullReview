# BPR-0967 Mac initial-load gap — 2026-09-25

Code candidate `cdbb208df52cf4478d2807b8aaedb20e3170356b`, continuing main
`e944da20b0dce838dc4a26c4e13615473ce0d06b` under assignment
`/root/bpr0967_load_gap`, mandate `r4-standalone-mac-20260925`.
Apple M4 Pro, 48 GiB, macOS 26.6.2, arm64 Debug, Qt 6.11.2,
Fusion/offscreen, 1600 × 1000 at scale 1. The network-denied public
`large-review-v1` course still opens through the Home action, accepts 515 files
and 1,000 threads, loads installed-Git/private-SQLite material, and renders
both exact 20,000-line source panes. This is a bounded partial repair; the
100 ms initial-load input requirement remains open.

## Attribution

The unchanged starting executable reproduces initial load **145.381 ms**, first
source **75.358 ms**, and revisit **78.698 ms**. These are complete phase
intervals using the previous milestone's repaired timing boundary. The historical
142.388 / 79.443 / 83.142 ms observations retain their original candidate in the
[prior evidence](BPR-0967-mac-stall-repair-2026-09-25.md).

Temporary nested timers on the same public acceptance course distinguish:

| Owner | Instrumented duration | Interpretation |
| --- | ---: | --- |
| PR window publication | 61.255 ms | Includes the conversation panel below |
| Conversation adoption / projection | 58.730 / 51.639 ms | Includes 34.869 ms of card layout |
| First Overview publication | 47.210 ms | Includes projection mapping and Qt publication |
| Overview mapping | 3.330 ms | Provider-free snapshot to Overview records |
| Overview page update | about 1 ms | Adopts the page's records; not the subsequent native show |
| Modality surface switch | 39.890 ms | Native layout while Overview becomes visible |
| Files drawer | under 4 ms | Separate later workbench row publication; not the 40 ms switch |
| Host accepted handoff | 29.163 ms | Includes ledger composition and read-state observations |
| Ledger composition | 18.236 ms | Includes read-state observation and card read-state relayout |
| Two read-state observations | 9.077 / 9.000 ms | Snapshot identities, receipt observation, and projection join |

These measurements overlap and are not independent totals. In particular,
calling the entire Overview publication expensive did not identify its page or
Files drawer as the cause. The finer trace isolates native column measurement
on showing the Overview table.

The read host applies snapshot validation/acceptance, synchronous presentation,
and the epoch handoff in one `handle_pull_request_now` decision callback. The Qt
executor posts that callback with `Qt::QueuedConnection`; it cannot deliver an
input timer in the middle of those synchronous calls. The measurements therefore
include their aggregate work, plus event delivery and other callback work, rather
than just a single widget operation. No executor scheduling, completion ordering,
reentrancy, authority, persistence or host lifecycle change is made here.

## Retained repair and controls

The Overview table now sets its native header's resize-content precision to
visible rows. The table retains every model row and continues to use native
`ResizeToContents`; rows can still be selected and scrolled to by exact path.
This removes full-table column measurement from the first native show.

A delegate wrapper observes actual size-hint requests over a 1,000-file table.
The original product fails because initial sizing visits the complete table.
The repaired product measures fewer than 100 file identities in the fixed
1200 × 800 test viewport, retains all 1,000 exact rows, and sizes the newly
visible last file after scrolling without measuring the entire review. The test
also runs the page's existing folder, identity, description, participant and
interaction checks. It measures native layout calls, rather than inspecting the
configured precision property.

The instrumented header-only experiment reduces the modality switch to
**3.011 ms**, first Overview publication to **10.669 ms**, and the complete
load interval to **107.183 ms**. That last number still fails 100 ms and is
an attribution experiment, not final uninstrumented acceptance.

A temporary additional public input gate at the unchanged 100 ms target fails
against the original product at **149.630 ms**. The final course keeps its
existing `InputBudgetFailures` reporting and the two previously accepted source
interval gates; this milestone does not adopt a passing initial-load gate.
The exact temporary control and failed reports are retained in ignored runtime
evidence. No measurement boundary or threshold is relaxed.

The composed course additionally checks the initial unread flag of every accepted
thread's exact post identity and its per-thread unread count. This assertion
passes through the existing host/ledger/Qt composition, before source selection.

## Rejected host experiment

An identity index replaced the repeated thread scan inside read-state projection
joining, preserving all matching indices and their order. It did not demonstrate
a useful composed improvement: total read-state observations measured
**9.789 / 10.604 ms**, compared with roughly 9 ms before; its join itself took
only **0.168 / 0.187 ms**, with most time spent constructing observations before
the ledger call. The combined uninstrumented load interval was **125.359 ms**.
The index and every host edit were removed. That experiment supports neither a
storage-only diagnosis nor a host performance claim. All temporary timers and
source instrumentation were removed before the final candidate build.

## Exact-candidate verification

The final uninstrumented responsiveness course retains all **20/20** existing
fixture gates and records the following complete intervals:

| Phase | This milestone baseline ms | Final ms | Unchanged input target ms |
| --- | ---: | ---: | ---: |
| Initial metadata/Git load | 145.381 | **108.992** | 100 |
| First large source | 75.358 | 84.000 | 100 |
| Large-source revisit | 78.698 | 86.241 | 100 |

`InputBudgetFailures` contains only `paged_review_and_git_load`. The header repair
removes **36.390 ms** from this baseline's maximum load gap, but the remaining
**8.992 ms** above target is not accepted. The source gates and prior improvements
remain in force; individual run timings are not a fixed hardware guarantee.

Final command results and report digests are recorded in the
[durable handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff-load-gap.json).
The checked candidate changes only Overview header sizing and two test files.
Earlier conversation-detail and inactive unified-document repairs are unchanged.

## Interrupted integration pass

The exact code candidate's first focused pass finishes **15/18 in 1,875.02 s**.
Architecture boundaries; Overview, workbench, conversation and composed Qt
checks; all three diff scales; ordinary, timing-control, unavailable,
approximate, responsiveness, loaded-close and resources courses pass.
Three longer courses fail while macOS sleeps:

| Course | Original result | Observation |
| --- | --- | --- |
| Cache bytes | Failed, 366.42 s | Epoch 31 preparation takes 100,696.532 ms; `EPOCH_CACHE_FAILURE:10`, `revision-prepare:6`; 30 epochs accepted before the interruption |
| Cache entries | Timeout, 1,011.44 s | Last completed selection is 37, at 944.393 ms; prior selections are roughly 0.9 s |
| History | Failed, 357.47 s | Epoch 4 takes 49,674.176 ms with a 41,617.518 ms input gap; epoch 5 takes 13,192.777 ms; all 32 epochs are retained, with zero model resets |

The system power log records sleep at **11:09:09–11:10:47 (98 s)**,
**11:11:32–11:27:40 (968 s)** and **11:28:27–11:29:11 (44 s)**,
on 2026-09-25, UTC−07:00. These overlap the three interrupted courses.
The initial log, partial history report and exact power transitions are retained
under `build/bpr0967-load-gap/`; the red timings are not discarded. Loaded-close
passes in 13.43 s and resources in 14.66 s.

Only these three affected courses are repeated, with a process-scoped
`caffeinate -i -s` assertion active from CTest startup. The source candidate,
measurement boundaries, cache ceilings and all timing/memory budgets are
unchanged. The assertion ends with that CTest process; no persistent power
setting changes. The repeat passes **3/3 in 730.77 s**: cache bytes **341.21 s**, cache
entries **74.05 s**, and history **315.49 s**. Both CTest and its assertion
exit; no task-owned sleep assertion remains. Thus all 18 selected checks have
passing observations, with the original 15/18 result retained separately.

## Preserved resource and identity assertions

Both cache retries pass: **341.21 s** for bytes and **74.05 s** for entries.
The byte course makes 20 selections, admits 70,353,760 bytes overall and retains
at most 66,839,408 cache bytes. Evicted diff/index owners expire; exact revisit
and all eight held-source observations pass. Its 32 epochs are observed. The
entry course makes 65 selections, peaks at 64 retained entries, and passes exact
eviction/revisit checks. Existing 64 MiB and 64-entry cache ceilings are unchanged.

All four resource courses retain their lifetime-peak and live-heap cleanup gates:

| Course | Lifetime footprint peak growth MiB | Limit MiB | Shutdown live-heap growth MiB | Destroyed live-heap growth MiB |
| --- | ---: | ---: | ---: | ---: |
| cache-bytes | 160.625 | 184 | 66.236 | 13.348 |
| cache-entries | 90.141 | 160 | 61.926 | 13.331 |
| loaded-close | 129.016 | 160 | 66.408 | 14.107 |
| resources | 128.922 | 160 | 66.485 | 14.169 |

History retains **32/32 epochs**, with **zero model resets**, 31 selections
and 30 selections overlapping work. Its worst refresh input gap is **72.396 ms**
against 100 ms; worst refresh completion is **8,725.518 ms** against 12,000 ms;
worst epoch source-selection completion is **976.450 ms** against 2,000 ms.

Cleanup limits remain 84 MiB while the application is open and 32 MiB after
its destruction. Final resource samples have zero diff/epoch jobs. Loaded-close
preserves epoch storage and releases documents/window; prune/delete clears its
projection and releases diff/index owners. Exact source/blob/anchor checks remain
unchanged. These footprint observations do not measure peak live heap.

## Remaining acceptance

The aggregate initial-load interval still needs further bounded owning analysis
or a separately justified scheduling design with ordering and stale-state
controls. This milestone establishes one native-layout cause and rejects one
unhelpful host optimization; it does not attribute all residual time to one owner.

BPR-0967 also retains true process/Qt-to-first-frame launch, separate initial
metadata, full active-work input/cancellation, generated/pathological fallback,
peak live heap and Windows/Linux native measurements. Windows/Linux stations
remain explicitly deferred. Fixture widget capture does not establish native
compositor or OS input delivery. No human, independent, packaged or release
acceptance is supplied. The coordinator's protected RFC0052 14-question review
session, state, candidate and server are untouched; this slice creates no session.
