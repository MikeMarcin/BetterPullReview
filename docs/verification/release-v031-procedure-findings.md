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

## Retrospective and adopted workflow, 2026-10-20

After publication, the owner requested this retrospective and an improvement to
`publish-release` while the separate implementer works on BPR-1653. The original
release record above remains historical. The skill now routes coordinated and
resumed runs through its [execution and recovery reference](../../.codex/skills/publish-release/references/execution.md).
This is a procedure change; it does not change the published source, assets,
test selection, security policy or acceptance limits.

### What consumed the time

| Cost | What the evidence supports | Adopted response |
|---|---|---|
| Final Windows full suite: 159.39 min; Mac: 52.51 min; Ubuntu: 54.60 min | Windows is the longest measured normal gate. Windows and WSL share one host, so their suites cannot safely overlap with performance-sensitive work. | Start the independent Mac lane alongside the PC lane; retain existing native scheduling and caches. |
| Earlier failed Mac and Windows suites: 60.08 and 168.82 min | About 228.91 min of aggregate suite work preceded the final freeze. The runs overlap; this is not an elapsed delay estimate. They exposed a real read/menu race and a navigation fixture ordering error. | Exercise small changed public-composition courses on relevant hosts before final gates. Reject obsolete candidates before unstarted downstream work. |
| Native dispatch and collection repairs | Canonical CPM paths, SSH GUI context, WSL PATH, PowerShell stderr handling, an outer wait stall and collector mistakes required recovery. A native pass survived the wrapper repair. | Validate the actual invocation route early; distinguish native completion from wrapper/collection state and recover only the failed layer. |
| PC material handoff to final handback: about 155 min | This is a handoff interval, not measured human effort or wholly idle time. Technical work continued during part of it. Mac review already overlapped PC work. | Prepare packets during eligible work, hand over each exact ready criterion promptly, and preserve accepted partial decisions and authorization across handoffs. |
| Publisher: 34.129 s; anonymous download/source checks: 33.778 s | Publication/upload was a small measured phase compared with suites and the review interval. | Keep the guarded single publisher; optimize readiness and recovery before considering upload changes. |

The retained original suite timings are authoritative. Windows' four largest
rows were architecture boundary (621.838 s), large-review history (428.245 s),
profile/window restoration (412.091 s) and application composition (226.244 s).
These name future profiling targets; OS, hardware, filesystem and workload
causes have not been separated. They do not establish permission to omit rows,
reduce fixtures or relax timing bounds. The recorded phase data also does not
provide a complete, non-overlapping end-to-end critical path from the initial
Qt upgrade request; no total time or projected speedup is inferred.

### What changed for the next release

The skill now requires a shared continuation record, reconciliation before
redispatch, early changed-interaction checks, common committed source before
final gates, physical-host scheduling, and repair of collection without
repeating valid native work. It explicitly identifies the Mac producer's own
normal gate so operators do not add a redundant full prepass. It routes review
preparation alongside eligible work and keeps exact native trust, source/SPDX,
security dispositions and required human decisions intact.

The signing-context wording now recognizes v0.3.1's observed approved Windows/
WSL production without granting readiness to another logon or station. No new
orchestration executable, background job, test optimization, release or version
change is introduced. Improvement in elapsed time must be measured on a future
run. BPR-1653 and any engineering of slow test rows remain separate work.

### BPR-1670 scheduling follow-up

The [Windows runtime investigation](BPR-1670-windows-suite-runtime.md) separates
159.39 minutes of suite wall time from 9.47 minutes of combined final Windows
configure/build/bridge overhead. Blanket serialization accounted for 147.78
minutes of exclusive work. A matched four-row native correctness pilot with
identical executable/dependency hashes passed in 169.36 seconds versus 443.07
seconds serially. The complete native follow-up exercised all 784 rows in
136.02 minutes versus the retained 159.39 minutes (14.66% observed reduction);
its three missing-tooling failures remain raw failed evidence, with exact-pin
focused recovery passing separately. The report binds that combined evidence
and the unchanged graph/binaries. This does not meet the owner’s subsequent
30-minute end-to-end release target; the external architecture/portfolio design
is linked from the investigation.
The maintained policy permits only explicitly audited process-private correctness
courses to overlap, preserving locks and exclusive timing/native/global-resource
courses. This refines the scheduling guidance above; it changes no v0.3.1 asset
or historical acceptance and establishes no measured end-to-end release saving.
