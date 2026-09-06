---
schema: "implement-rfc.card.v1"
id: "BPR-0629"
title: "Prepare the selected semantic index outside the UI publication turn"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0627"]
acceptance: [{"criterion":"Validated preparation and exact adoption","outcome":"pass","revision":"fb55c7976715abb54dfda3ac65a551b1bbe05fa1","evidence":{"kind":"observation","detail":"Factory validation/index construction occurs in the existing effect job. Host and Qt bind the typed immutable index to the exact current result/context; Qt missing, cloned-result mismatch and stale-input negatives pass."}},{"criterion":"Bounded cache and cancellation","outcome":"pass","revision":"fb55c7976715abb54dfda3ac65a551b1bbe05fa1","evidence":{"kind":"observation","detail":"Host tests pass exact index reuse and byte total, result-only budget refusal, current-generation publication, cancellation, shutdown and reordered worker/cache completions. Index object/vector capacity accounting preserves existing ceilings."}},{"criterion":"Direct semantic and application behavior","outcome":"pass","revision":"fb55c7976715abb54dfda3ac65a551b1bbe05fa1","evidence":{"kind":"observation","detail":"Dev core/host 2/2, Qt/large/unavailable courses 4/4, fixture/boundary/composition 5/5, and core-only semantic/boundary 3/3 pass; full-window render, exact 20,000-line source, navigation, 1,000 idle wheel frames and 32-selection final precedence are verified."}},{"criterion":"Measured local publication target","outcome":"pass","revision":"fb55c7976715abb54dfda3ac65a551b1bbe05fa1","evidence":{"kind":"observation","detail":"Clean large-file max/p95 interval 65.308/5.786ms and rapid 28.196/5.679ms meet provisional recorded-Mac 100ms target. Separate temporary profile measures native text conversion/install at 64.730ms, removed and clean target rebuilt; no universal/platform or RFC-wide claim."}}]
---

# Prepare the selected semantic index outside the UI publication turn

BPR-0627's temporary phase instrumentation on source `d698b338` measured about
111.14 ms in `build_diff_semantic_index` during real 20,000-line publication.
Installing both native source documents separately took 62.12 ms. BPR-0628 owns
native pane measurement; neither step should be hidden by a larger UI budget.

## Bounded acceptance

- Inspect the host/Qt publication ownership and immutable-index lifetime before
  choosing the smallest design. Keep deterministic validation/index semantics
  in core and effect scheduling/cancellation in the host; no worker may touch Qt.
- Reuse or prepare the exact immutable semantic index off the UI thread, bind it
  to the accepted result and full current request identity, and prevent stale or
  mismatched index/result adoption. Do not skip validation to improve a timing.
- Preserve one semantic index per accepted immutable diff and bounded cache/job
  retention. Verify cache hits, cancellation, replacement and rejected late
  completion without extra whole-result work on routine interaction.
- Run focused core/host/Qt checks and the real stored large-review course. Record
  publication and event-loop measurements on the same Mac and verify exact
  source, navigation, scrolling and final-selection precedence.
- Confirm remaining indivisible native text-install cost separately. Define a
  measured local publication target without claiming universal timing or native
  Windows/Linux evidence.

Expected scope is the existing diff host/publication port and Qt workbench;
final exclusive ownership must be recorded after inspection. RFC base
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617. Use local synthetic
fixtures only; no Actions, push, paid CI, credential use or production app launch.


## Selected implementation boundary

Inspection after BPR-0628 confirms the existing diff effect job already produces
an immutable result. Its worker validates the full result, the UI decision-lane
`finish` validates it again, and Qt independently validates/builds its semantic
index. Cache lookup also repeats full validation. Core's `diff_semantic_index`
has a private constructor, factory validation, and shared ownership of its exact
result, making that existing type the prepared proof; no second proof subsystem
is needed.

Own `src/core/{include/bpr/core,src}/diff_semantics.*` for exact retained index
allocation accounting, `src/application/include/bpr/application/diff_runtime_ports.hpp`
and `src/application/src/diff_application_host.cpp` for prepared publication,
worker construction, identity checks, bounded cache retention and cancellation,
and Qt `diff_workbench.*` for adoption. Update direct fixture publishers and
focused core/host/Qt/application tests as required by the stronger publication
contract. The narrow composition/main fixture statistics consumers and their source
manifest bookkeeping are also owned when changing the counter from construction
to adoption. No Qt API runs on the worker. Full current request context and exact
index/result pointer binding remain mandatory on acceptance; typed index
construction preserves full validation while eliminating repeated UI scans.
Account for index allocation within existing cache byte limits, including cache
hits; do not raise limits or keep a second index per result.

BPR-0628's final same-Mac course measures initial-selection maximum 297.119 ms
while native measurement input gaps are maximum/p95 5.356/4.488 ms. Its final
source is `49aefc1c`; phase/publication work remains the next latency boundary.
Keep initial native text installation measured separately when establishing a
local target. Required checks include focused semantic-index, diff host,
workbench, architecture-boundary, and composed large/unavailable/recovery lanes.


## Implementation and acceptance

Source `fb55c7976715abb54dfda3ac65a551b1bbe05fa1` prepares `diff_semantic_index` in the existing
bounded effect job, after computing and before posting the typed completion.
Its private construction path still fully validates the immutable result. Host
finish/cache lookup and Qt adoption verify exact index/result ownership plus
the full current request identity; they no longer repeat full-result scans.
Qt requires preparation and refuses missing or mismatched indexes, even when a
substituted result has identical content and digest. It retains the same shared
index, and statistics now correctly count adoptions rather than UI builds.

The cache retains the result and its one prepared index together, within the
unchanged byte/entry ceilings. Index object and vector capacities are counted,
excluding the already counted shared result. Tests prove a result-only budget
cannot cache the pair and that a cache hit preserves the exact index pointer
and byte total. Allocator/control-block overhead is not a process-memory claim.
Cancellation is checked after preparation and every posted terminal still
traverses the serialized core transition; stale/cancelled/shutdown completions
cannot publish or populate the accepted cache. No worker touches Qt.

### Focused evidence

All checks use this source's semantic inputs; final formatting is whitespace-only.

- Dev `ctest --test-dir build/dev --output-on-failure -R '^(core_diff_semantics|application_diff_host)$'`
  passes 2/2 in 0.95s. Exact owner identity and allocation accounting are covered
  in core; host worker/decision separation, cache reuse/limits, full identity
  changes, reordered cache/worker terminals, cancellation, shutdown, bounded
  pending work and reentrant projection checks pass.
- Dev `ctest --test-dir build/dev --output-on-failure -R '^(qt_diff_workbench_scale_100|qt_workbench_window_scale_100|application_large_review_benchmark|application_unavailable_diff_public_path)$'`
  passes 4/4 in 41.75s. Qt proves missing preparation, same-content pointer
  substitution and stale requests are refused; accepted preparation is shared
  without reconstruction. Existing native selection/scroll/layout/batching and
  full-window checks pass. The real unavailable/binary/supported-recovery path
  passes in 14.65s and the real stored large-review path in 22.49s.
- Dev `ctest --test-dir build/dev --output-on-failure -R '^(qt_syntax_pane_composition|qt_appearance_profile|architecture_boundaries|architecture_boundary_contract|application_review_round_trip_composition_contract)$'`
  passes 5/5 in 144.37s. Direct fixture publishers now prepare indexes before
  entering Qt. The composition/main counter spelling changed with its meaning;
  source-manifest digests are refreshed for those reviewed edits, with no
  mutation-dispatch expansion. The application and focused Qt targets build.
- Core-only semantic target builds, and
  `ctest --test-dir build/core-only --output-on-failure -R '^(core_diff_semantics|architecture_boundaries|architecture_boundary_contract)$'`
  passes 3/3 in 93.55s. Core remains independent of Qt and effect capabilities.
- Documentation structure and `git diff --check` pass. Temporary text-install
  profiling was restored byte-for-byte to committed source and the clean
  benchmark target rebuilt; its diagnostic code/logs are not shipped.

### Same-Mac measurements and limits

The uninstrumented large course retains all 515 files, 1,000 comments/threads,
29 paged requests, exact 20,000-line source documents and 20 changes. Its full
1600x1000 application raster is inspected with the same native panes, line
numbers and semantic colors. Two next-change actions take 3.868/3.181 ms and
select exact expected source lines. 1,000 idle wheel events produce 1,000 shared
frames, zero index adoptions/axis rebuilds/pane measurements. All 32 file choices
finish on the exact final source, including 31 choices with diff work pending;
transient conversation roots remain at most two.

On the retained M4 Pro / 48 GiB / 14 logical processors, macOS 26.6.2 arm64,
Debug Qt 6.10.3 offscreen/Fusion at DPR 1:

| Phase | Elapsed ms | Maximum / p95 event-loop interval ms |
|---|---:|---:|
| Paged review/Git load | 11,034.52 | 48.369 / 5.747 |
| Large-file selection | 1,910.29 | 65.308 / 5.786 |
| Rapid file selection | 7,962.83 | 28.196 / 5.679 |

The preceding BPR-0628 clean large-file maximum was 297.119 ms. Native
measurement now has 82 batches and 81 intervening input callbacks; maximum/p95
input gap is 5.236/4.209 ms, with at most 512 lines per continuation. Idle wheel
dispatch is 312.70 ms and supersession drain 964.35 ms. Cache retention is two
results plus their indexes / 3,521,008 accounted bytes; the additional 3,496
bytes versus BPR-0628 are included within the same configured limit.

A separate temporary native text conversion/installation timer on the same
20,000-line public course measured 64.730 ms (diagnostic course passes in
22.71s). This is consistent with the remaining 65.308 ms clean UI interval;
it is not included as a clean timing baseline. A provisional 100 ms maximum
publication/input observation target on this recorded Mac gives roughly 50%
headroom over the observed native installation cost and agrees with the prior
rapid-input target. Both observed courses meet it; it is not enforced as a
portable CI threshold or claimed as a universal frame budget. Native text
installation remains indivisible here; pathological line/layout policy and
Windows/Linux evidence remain RFC-wide work.

BPR-0629 is complete. RFC-0013 stays open for its memory/pruning, many-epoch,
backoff/offline, fuzz/diagnostic, broader active-work and native-platform gates.
No GitHub Actions, workflow dispatch, push, paid CI, live account or production
app launch was used; remembered production sign-in is preserved.
