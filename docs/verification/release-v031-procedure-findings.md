# v0.3.1 release procedure findings

BPR-1663 records the completed workflow before any later optimization. The
owner asked to finish the current release unchanged and preserve timings,
retries, waits, handoffs, approval latency and reused evidence for a subsequent
retrospective. This record changes no executable workflow, gate or threshold.
The durable external session retains raw receipts and earlier attempts; values
below distinguish test duration from wrapper/build and human wait time.

| Measured phase, 2026-10-20 UTC | Duration / result |
|---|---|
| Final macOS normal suite | 3150.742 s; 814 pass + 1 configured Debug skip |
| Final Windows normal suite | 9563.164 s; 783 pass + 1 configured Debug skip |
| Final Ubuntu normal suite | 3275.897 s; 792 pass + 1 configured Debug skip |
| Final macOS normal gate plus signed/notarized producer | 10:21:50–11:25:45; 3835.368 s |
| Optimized macOS configure/build/course | 121.976 s total; course 81.656 s |
| Optimized Windows wrapper | 14:28:13–14:37:13; course 282.294 s |
| Optimized Ubuntu wrapper | 14:46:06–14:53:59; course FAIL 60.085 s |
| Public dry run | 4.941 s; exit 0 |
| PC material handoff to final human hand-back | 15:53:03–18:28:11; about 2 h 35 min |
| Authorized publisher, one invocation | 18:32:25–18:32:59; 34.129 s |
| All 28 anonymous downloads and offline source binding | 18:33:19–18:33:52; 33.778 s |
| Downloaded macOS native audit | 18:34:18–18:34:41; 23.246 s |
| Native-input transfer, 27 files | 18:35:07–18:35:25; 18.471 s |
| Windows and Ubuntu downloaded-byte trust wrapper | 18:35:27–18:35:52; 25.029 s |
| Independent downloaded Windows five-file trust | 18:40:02–18:40:03; 1.326 s |
| Downloaded MSI extraction and 29-file equality | 18:40:24–18:40:26; 1.806 s |

The failed earlier macOS integration suite consumed 3604.92 seconds and Windows
10129.385 seconds. They exposed the reader/menu race and unfinished navigation
fixture command; those were repaired or diagnosed before the final common
freeze. Their raw failures remain, rather than being folded into final success.
All three exact final normal suites were explicitly requested by the owner.
Windows and WSL performance work remained serialized on their shared host.

Other retained delays and retries include the initial CPM canonical-cache-path
refusal, an unstarted rejected Ubuntu candidate, PowerShell treatment of native
stderr, a stalled Start-Process wait wrapper, warning/exit-code controls, a
collector's null serialization and incorrect JUnit path, and the Ubuntu RSS
diagnostic. Each has its own raw result; collector/dispatch failures are not
product test failures or evidence of a test pass. The initial postpublication
attempt to append a tag seal into a read-only retention directory was refused;
a new separate seal preserves that directory unchanged. Global plan discovery
also encountered unrelated invalid plans; explicit selected-plan binding passes
without editing them.

Evidence reuse was bounded: unchanged source-scan classifications and fuzz
inputs, exact native package/source material, current passing normal gates and
final artifact hashes survived between phases. Publication used the same
approved notes and manifests, with no rebuild or refreeze after material review.
Fresh download hashes, source checks and native trust were still performed.

The next procedure review can evaluate these concrete improvements:

1. Run the small changed public interaction courses before the expensive normal
   suites, so an active-reader/menu regression is found sooner. Retain full
   requested suites as acceptance, rather than replacing them with the prepass.
2. Reuse the proved native-process bridge and configured login/tool PATH.
   Validate native zero/nonzero exit propagation and actual receipt locations
   before long work. Avoid repeating signing or tests to repair collection only.
3. Prepare current source/material comparisons, native download scripts and
   independent-review packets while eligible native suites run. Start human
   review as soon as its exact immutable material is ready; do independent work
   during the wait and preserve the attended context boundary.
4. Keep one final source freeze, stable signed asset set and guarded publisher.
   Separate administrative latest-link/ledger updates from the tagged producer
   revision. Continue refusing uncertain mutation retries until remote state
   is read, and never overwrite source assets or retag a release.
5. Preserve phase timing and owner/command state in one durable continuation.
   Diagnose wrapper, collector, build, test and human wait costs separately;
   do not add broad reruns merely because a handoff occurred.

These are recorded follow-up proposals, not newly implemented automation or a
claimed speedup. BPR-1653's memory repair is a separate next engineering scope.
