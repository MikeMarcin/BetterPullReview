# RFC-BPR-0013 verification ledger

## Owner-amended closeout r4 — 2026-09-21

The owner explicitly requires RFC0013 to close to unblock RFC0014 under the
same r4 direction applied to RFC0008–0012. This amendment accepts the bounded
implemented hardening scope and transfers every unproved broader obligation
below. It does not claim that the original full matrix passed. BPR-0617 is
Done at `71d6a1109926de25b363550d04ed9fb505b99e14`; RFC/index are
Auto-Verified under that explicitly amended scope.

Assignment `/root/rfc0013_owner_closeout`, run
`rfc0014-owner-closeout-20260921`, segment `rfc0013-owner-closeout-r4`;
coordinator-implementer recipe revision 2, non-delegating leaf.
Entry/product test candidate: `3efe61dfcf6874d35400bf777a7e86dbbf0a4693`.
Immutable RFC baseline: `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`.
The [mandate](../rfcs/plans/rfc-bpr-0013-hardening/mandate-owner-closeout-r4.md)
binds scope and previous handoffs. All predecessor RFCs now are Auto-Verified.
The six completed live-plan milestones and their original exits are unchanged;
only `verify-rfc-0013` and terminal acceptance are explicitly amended.

### Acceptance and follow-up mapping

The current RFC contains nine narrowed criteria; the original nine remain
immediately afterward. The original matrix and historical reconciliation below
retain their meaning. The current BPR-0617 checklist contains exact canonical
criterion text.

| Current criterion | Direct evidence | Acceptance boundary |
|---|---|---|
| AC1 | BPR-0623/0624/0631/0632 independently checked 515-file, 20,000-line, 1,000-thread, 32-epoch fixtures; current large-review benchmark. | Pass for bounded fixtures and exact source documents. Current benchmark 22.23 s; historical 32-epoch identities remain candidate-specific. |
| AC2 | Current large-review, selected-file safety, unavailable-diff and held-loading courses; retained BPR-0627..0629/0751 measurements. | Pass for named behavior; 98.723 ms source and 32.004 ms rapid-selection maxima. Initial load 321.700 ms is not a universal 100 ms pass. BPR-0967 owns broader performance; BPR-0940 owns benchmark visible-page reconciliation. |
| AC3 | Current application_diff_host ceiling/eviction/cancellation tests; historical BPR-0630..0635 ownership and pressure evidence. | Pass for host bounds and truthful historical evidence. Current loaded-close/resources fail before cleanup at 160 MiB sampled-growth; BPR-0967 retains exact failures. No current native memory or cleanup pass. |
| AC4 | Current application_review_queue_refresh, qt_review_queue, github_paged_read and github_provider; RFC0010/BPR-0161 backoff evidence. | Pass for component policy/projection. BPR-0968 retains the unobserved complete rendered throttling course. |
| AC5 | Current queue refresh offline/recovery and stale-session rejection; retained Git/catalog history. | Pass for typed policy; BPR-0969 retains the unproved cached-review offline/restart and visible freshness behavior. |
| AC6 | BPR-0616/0618/0619/0620/0621 recorded kill/restart/integrity/no-replay; BPR-0622 native ownership evidence. | Pass for recorded exact courses: Windows c94e02b1/2ba02ddb, Linux 780d0a1c/48b6422b, Mac 8a03a5be. Current publication replay remains in RFC0008 r4 evidence; no new full native kill matrix. |
| AC7 | Current core_diff_compute/core_diff_semantics/core_theme_exchange, github_paged_read/provider and application_agent_finding_protocol; original parser/property inventory. | Pass for deterministic regression evidence and explicit inventory. Complete fuzz target/corpus requirement remains wholly open on BPR-0970. |
| AC8 | Current application_logging, quill_logging and quill_logging_backpressure; historical BPR-0602/0606. | Pass for existing composed/error/backpressure checks. BPR-0971 retains the full pathological privacy audit. |
| AC9 | Current epoch/diff hosts, held loading and selected-file safety; retained scale/history/index evidence. | Pass for named paths only. BPR-0972 retains wider structural/active-work evidence, BPR-0940 retains non-green layout/composition, BPR-0945 strict tidy. |

### Durable unfinished work

| Card | State and remaining obligation |
|---|---|
| [BPR-0967](../issues/20-ready/BPR-0967-measure-and-reconcile-native-large-review-resource-budgets.md) | Standalone, unfinished. The [full remaining Mac report](BPR-0967-mac-full-remaining-2026-09-25.md) records optimized Release process/Qt and metadata/widget boundaries, the repaired 100 ms initial-load gate, generated/fallback recovery, held-provider cancellation and all resource/history courses. Debug observations remain diagnostic history. Sampled live-heap peaks and a calibrated profiled loaded-close joint heap/VM bound have explicit limits. Native/package/compositor, wider active/pathological and heap-only coverage plus deferred Windows/Linux remain open; no full AC3 or release acceptance. |
| [BPR-0968](../issues/60-done/BPR-0968-verify-rendered-primary-and-secondary-rate-limit-recovery.md) | Backlog, standalone: complete rendered primary/secondary backoff and recovery. |
| [BPR-0969](../issues/60-done/BPR-0969-verify-cached-review-offline-restart-freshness.md) | Standalone, unfinished: the [Mac inventory](BPR-0969-offline-inventory-2026-09-25.md) completes bounded AC1 and repairs the retained live review's failed-refresh freshness notice. Full offline review restart remains unavailable despite durable Git/epochs. The [Recent isolation slice](BPR-0969-recent-identity-2026-09-25.md) repairs same-profile title/destination/reading isolation with schema-five unknown-account migration. The [durable overview slice](BPR-0969-durable-overview-2026-09-25.md) reopens exact-account historical metadata after current-process authentication, with unknown freshness and unavailable source/conversations; it adds no disconnected account authority. Complete AC2/AC3 remain pending. The [file catalogue continuation](BPR-0969-file-catalogue-2026-09-25.md) retains exact historical file paths/change counts with the overview; source and conversations remain unavailable and full AC2/AC3 unfinished. |
| [BPR-0970](../issues/60-done/BPR-0970-cover-custom-parser-and-mapping-boundaries-with-fuzzing.md) | Done, standalone: eight libFuzzer targets and their corpora over every inventoried custom parser and mapping, four sanitizer campaigns and four fixed defects; see the [parser fuzzing record](BPR-0970-parser-fuzzing.md). |
| [BPR-0971](../issues/60-done/BPR-0971-audit-pathological-diagnostic-privacy-and-backpressure.md) | Backlog, standalone: complete pathological diagnostic/privacy inventory and large/error courses. |
| [BPR-0972](../issues/60-done/BPR-0972-verify-application-wide-active-set-and-changed-set-scaling.md) | Backlog, standalone: broader active/changed-set, structural and concurrent-work evidence. |
| [BPR-0817](../issues/60-done/BPR-0817-a-linux-close-exits-1-when-the-app-created-its-data-folder-group-writable.md) | Parked, RFC0033 association retained: existing writable-parent visibility and orderly-exit policy. Prior native successes and unanswered questions unchanged. |
| [BPR-0825](../issues/60-done/BPR-0825-harden-the-windows-workspace-supervisor-after-its-critical-review.md) | Parked, standalone: shared Windows private-descriptor ownership decision. Native supervisor/ACE/junction fixes remain accepted only as recorded. |
| [BPR-0940](../issues/60-done/BPR-0940-reconcile-composed-github-legacy-assertions.md) | Backlog, standalone: existing composed GitHub/window-layout failures and current benchmark visible-page mismatch. |
| [BPR-0944](../issues/10-backlog/BPR-0944-decide-whole-pr-comparison-basis.md) | Backlog, standalone: Whole PR basis, readable partial pane and missing-earlier explanation. Not fixed. |
| [BPR-0945](../issues/60-done/BPR-0945-resolve-existing-strict-tidy-header-diagnostics.md) | Done on 2026-09-25 as standalone BPR-0945: both recorded header diagnostics repaired; the four affected translation units pass canonical strict tidy at `f8b589e7`. This does not assert repository-wide tidy acceptance. |

BPR-0934 Overview folder collapse remains RFC0037 work. BPR-0966's later Dif
workshop cue mismatch remains a separate follow-up from RFC0012. No historical
Done card is reopened and no parked human question is answered by this transfer.
The scope transfer does not certify RFC0014 native, trust, vault, crash,
vulnerability, licensing or publication obligations.

### Current direct verification

All commands ran on the entry product source above. No product/test/build/package
input changed in this assignment. Builds for the public application, benchmark,
selected-file safety, logging and parser/provider targets report no work to do.
The initial guessed target `test_selected_file_safety_public_path` was rejected
before building; the registered `test_setup_destination_public_path` succeeded.

1. `ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^(application_selected_file_safety_public_path|application_large_review_benchmark|application_unavailable_diff_public_path|application_large_review_loaded_close|application_large_review_resources)$'`
   passes **3/5**, 86.50 s. Large review 22.23 s, unavailable content 15.50 s
   and selected-file safety 22.87 s pass; both memory courses fail as detailed below.
2. `ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^(application_epoch_host|application_diff_host|application_review_queue_refresh|qt_review_queue|qt_composed_loading_scale_100|application_logging|quill_logging|quill_logging_backpressure|core_diff_semantics|core_theme_exchange)$'`
   passes **10/10**, 7.28 s. Fixed ignored log:
   `build/rfc0013-owner-closeout/focused.log`.
3. `ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^(core_diff_compute|application_agent_finding_protocol|github_paged_read|github_provider)$'`
   passes **4/4**, 15.10 s. Fixed ignored log:
   `build/rfc0013-owner-closeout/parsers.log`.
4. Unchanged repeat `ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^application_large_review_(loaded_close|resources)$'`
   fails **2/2**, 26.69 s. Fixed ignored log:
   `build/rfc0013-owner-closeout/memory-recheck.log`, SHA-256
   `a6634710efb5129ff8b262a11b7b040b9bdb4dee3dbe387d9bebd5a148f8f3ee`.
   The first CTest temporary log was replaced by a subsequent test inventory;
   the repeat preserves complete failure output under this explicit path.
5. Documentation gate, its 64 behavior tests, scoped board validation and final
   checkpoint checks are recorded at terminal reconciliation below. Existing
   RFC0008–0012 current evidence is reused only where relevant inputs are unchanged;
   no new independent, installed-agent, native-package or private-target run occurs.

The ordinary benchmark checks 515 files, 1,000 exact threads/comments, 30
account reads, exact large/final source documents, 32 rapid choices (31 during
diff work), 1,000 wheel events and zero scroll index/geometry/measurement
rebuilds. It records 83 measurement batches with 82 input callbacks,
maximum 4.355 ms gap, 98.723 ms large-source and 32.004 ms rapid-selection
timer maxima. Initial paged load is 321.700 ms maximum; that is not a blanket
100 ms input pass. The 2-entry cache is 3,521,024 bytes.

Direct raster inspection shows the ordinary benchmark window on **Overview**,
not visible source; exact hidden source documents do not establish a readable
diff. BPR-0940 retains this fixture/presentation mismatch. The unavailable-diff
raster shows **Read**, cleared panes and an explicit comparison-work-limit
message. Prior signed r23 safe source/anchor evidence remains tied to its own
candidate and is not promoted to current visible-source or private-target proof.

Current resource failure is at the unchanged 167,772,160-byte (160 MiB)
sampled growth threshold, before either cleanup course:
loaded-close grows 192,217,376 bytes; resources grows 191,136,128 bytes.
Both have a one-entry 3,517,688-byte cache and zero diff/epoch jobs at that sample.
The leak/retention cause is unknown. Current Qt is 6.11.2 Debug/Fusion on arm64
macOS 26.6.2; older passing memory measurements used Qt 6.10.3. A toolkit change
is context, not demonstrated causation. Neither later close/prune correctness
nor destruction memory was reached in these failed runs. BPR-0967 owns diagnosis
and a justified correction, without suppressing tests or silently raising bounds.
Old successful JSON/raster files beside the benchmark are not current memory
evidence; the failing repeat log is authoritative for these runs.

Current app SHA-256:
`a0f9c85cd58e4c9d486ae39c554b16f30f4bb73e867ebfbb6c17ad21ca775d90`.
Current benchmark SHA-256:
`5af04df6d21e5ce3b79e935d3e73c34f30dcdd7522adc2f35070c970e05f2ec1`.
The review-server/session and signed artifacts were not created or changed.
Source-only architecture evidence from the preceding owner round remains bounded
by its exact tracked inputs; the nested unrelated checkout is preserved.

### Documentation and architecture disposition

The original planning stages are reconciled by the retained six milestones and
per-criterion history: compact core/index and cancellation work; bounded host
cache/job/recovery; incremental Qt and composed large-review fixtures; actual
native ownership and security negatives; prior recorded simplification/review.
No new runtime policy or dependency is introduced here. Core/host/Qt ownership,
typed intents/events/observations/requests/deltas, authority, concurrency,
persistence, authentication, package and security behavior have **no runtime
change**. Documentation impact is the explicit terminal-scope amendment,
follow-up discovery, current evidence and canonical ordering.

### Terminal reconciliation

The owner amendment, follow-up allocation and Ready promotion were committed at
`9991ebf64ed5b72d7e625a884424f868577ab80b`; guarded claim is
`1a039572f50203e18f16aa11a0483ab0287d6aa8`; guarded Done is
`71d6a1109926de25b363550d04ed9fb505b99e14`, with nine evidence records.
Documentation gate and all 64 documentation regressions pass; scoped validation
is valid with 964 total cards. The selected RFC scope is now 90 cards, all Done.
Plan structure/references and diff checks pass. The first six milestones remain
identical; original source-plan and historical review packets are unchanged.

Baseline-to-entry source/test inventory spans 1,047 changed files and interleaved
RFC work. It is provenance for the recorded criteria, not a new review of every
interleaved feature. Entry-to-terminal source/tests/CMake/package and historical
review/source-plan paths are byte-unchanged. The current documentation diff was
self-reviewed against the mandate and each original acceptance row.

The final
[checkpoint](../rfcs/plans/rfc-bpr-0013-hardening/checkpoint-owner-closeout-r4.json)
and [handoff](../rfcs/plans/rfc-bpr-0013-hardening/handoff-owner-closeout-r4.json)
bind terminal revision `a94a6cca2eb9b10618dc111eb540a28cf0d30dff` and command
quiescence. The checkpoint check returns `stop` at this assigned integrated
boundary; it does not evaluate engineering truth. The coordinator may assign a
fresh RFC0014 owner: the predecessor gate is clear, but BPR-0965 still has five
unfinished dependencies BPR-0958–0962. Its native and security requirements were
not amended by this task.

## Historical 2026-09-20 reconciliation

The entire record below is historical. Its Proposed predecessors and unwaived
wider-scope stopping boundary were true at r22; r4 explicitly supersedes those
requirements. No original observation or rejection is rewritten.

## Disposition

The owner-amended `selected-diff-exact-epoch-pair` milestone is complete.
BPR-0794 and BPR-0795 are Done at `4047f072` and `ef6d2d7f`; BPR-0935..0939
and BPR-0941..0943 retain their earlier accepted evidence. The unusual-target
comparison-basis and partial-pane anomaly remains unresolved in BPR-0944.

RFC-BPR-0013 remains **Proposed**, and BPR-0617 remains **Backlog**. The owner
authorized closeout, but amended only the unusual-target requirement. That
does not waive canonical order or supply missing RFC-wide acceptance evidence.
RFC-BPR-0008, RFC-BPR-0010 and RFC-BPR-0012 are still Proposed; BPR-0507 is the
remaining unfinished explicit dependency of BPR-0617. No other RFC status is
changed by this reconciliation.

## Owner amendment, distinct from observed behavior

On 2026-09-20 the owner directed that the remaining UnrealEngine#1773 issue be
filed for later investigation, that the available right file should remain
useful, and that the missing-earlier message should occupy the left pane. The
owner asked to stop spending this RFC round on that unusual target and move on.
BPR-0944 now explicitly owns all three future outcomes: partial-pane rendering,
left-pane explanation, and Whole PR base-tip versus GitHub merge-base semantics.

The new owner screenshot was inspected directly outside the checkout. It shows
the current refusal above two cleared panes. It is not a successful partial-pane
render and no private pixels/content are committed. The original source/anchor
criterion was pending at `a9492592`; that historical state and every attended
rejection remain intact. The amended criterion accepts the retained loading
observation, signed safe source/anchor proof and explicit follow-up, not a fix to
the anomaly. No new target read, owner retest or attended candidate was made.

## Identity and scope

- Engineering contract `implement-rfc.engineering.v1`, coordinator-implementer
  recipe revision 2; leaf `/root/rfc0013_postreview_r3`, no delegation.
- Mandate `r22-owner-closeout-deferral`; entry `143509c3` on local `main`.
- Immutable RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a` remains an
  ancestor of the candidate. The scoped base-to-head inventory spans core,
  application, capability adapters, Qt, composition and their tests. Interleaved
  work from other RFCs is not reclassified as RFC-0013 acceptance.
- This round changes documentation and board state only. Product candidate
  `a9492592` and signed tests-disabled r23 bytes remain unchanged. The previous
  [signed evidence](rfc-bpr-0013-selected-material-20260920.md) and
  [usable-review evidence](rfc-bpr-0013-usable-review-20260920.md) remain relevant.

## Authored milestone reconciliation

| Milestone | Actual evidence and disposition |
|---|---|
| Default cache byte pressure | Accepted BPR-0635/BPR-0751 evidence: exact 70,353,760-byte public pressure, default eviction, stale-safe revisit, ownership release, 32-epoch/929-read projection repair. BPR-0751's isolated historical outlier was explicitly closed as cannot reproduce, not retroactively passed. `4a2be1b1`, `b1743179`, `42bd1ed1`. |
| Durable agent workspace recovery | Accepted BPR-0622 evidence is complete on Windows, Linux and macOS, not still missing as older frontier notes say. Windows `c94e02b1`, integrated `2ba02ddb`; Linux `780d0a1c`, integrated `48b6422b`; macOS preserved at `8a03a5be`. BPR-0825 is a separate nonblocking hardening follow-up. |
| Retained stress PR read | Accepted historical BPR-0643 known-limit alternative at `8285cb4f`; it never claimed a readable comparison. Later target work remains separately attributed. |
| Large PR returned-commit boundary | Accepted BPR-0792 at `3cefa687` / `edc59774`: total versus returned count, exact 250-record boundary, fail-closed negatives and recorded next refusal. |
| Review-thread null anchor | Accepted BPR-0793 at `0d9f3d71` / `17d84c90`: preserved nullable LINE subjects, focused/composed checks and actual next explicit refusal. |
| Selected exact pair and owner amendments | Complete under the explicit owner amendment committed at `a38761b4`, not by inferring private-target success. BPR-0794/0795 Done; BPR-0944 retains the unresolved anomaly. |
| Verify RFC-0013 | Not complete. Canonical predecessors and the original criterion-level gaps below remain. BPR-0617 is the existing owner of this work; no duplicate closeout card is allocated. |

## Original RFC acceptance matrix

These rows supersede the stale current-state claims in BPR-0617's historical
matrix. They retain, rather than silently defer or mark passing, original gaps.

| Criterion | Direct evidence / current result | Remaining acceptance boundary |
|---|---|---|
| Large files, file counts, comments, epochs, binary/generated fixtures | BPR-0623/0624/0631/0632: independently checked 515-file, 20,000-line, 1,000-thread, 32-epoch workload; exact public sources/refs/catalog. `ee1717c2`, `e9da7abc`, `d0047177`, `75d1494e`; later 929-read course retained. | Fixture and recorded Mac public-path claim covered; not a universal performance result. |
| Responsive input, scrolling and navigation during work | BPR-0627..0629 and BPR-0751 repaired measured pauses; Mac refresh maxima 64.617–91.147 ms and source below 0.952 s in the final history course. BPR-0938/0795 and current held-read paint checks show useful Overview before terminal acquisition. | Other-platform measured performance budgets and full RFC-wide active-work coverage are not established by these Mac observations. |
| Bounded memory/cache and recovery after close/prune | BPR-0630..0635 prove public prune/delete, loaded close, 64-entry and default 64-MiB byte pressure, weak-owner release and exact revisit. Recorded Mac sampled drain/destruction growth budgets are 160/96 MiB. | Sampled footprint is not an unsampled peak or a Windows/Linux budget. Preserve those platform/measurement limits. |
| Primary/secondary limits, bounded backoff and useful UI | Current provider/pager checks pass; BPR-0161 and the RFC-0010 ledger prove capped backoff, Retry-After, identity/cancellation and explicit typed statuses. Queue checks are rerun below. | The complete rendered RFC-0013 primary/secondary backoff course is not established by scheduler or provider units alone; BPR-0617 retains it. |
| Clearly labelled cached offline data, never false freshness | BPR-0161 proves typed offline/recovery and stale-session refusal; retained Git/catalog history is independently checked. | A composed cached-review offline/restart and visible freshness-label course is still unproven. No new offline mode is claimed. |
| Process-kill Git/SQLite recovery without corruption/duplicate mutation | BPR-0616/0618/0619/0620/0621 retain actual kill, repeated restart, integrity and no-replay checks. BPR-0622 adds native durable workspace recovery on all three platforms. | Reuse the exact platform/course evidence; native workspace evidence does not silently certify every other kill course on every platform. |
| Fuzz all custom parser/mapping boundaries | Deterministic parser, malformed-input and property tests exist. This audit found no registered fuzz targets/corpora in the current CMake/source inventory. | Complete diff/mapping, Markdown, settings/theme, API, Git-output and AI-output fuzz coverage is not demonstrated. The original requirement remains open on BPR-0617. |
| Metadata-only logging under large/error conditions | Current `application_logging`, `quill_logging` and backpressure checks pass; retained large-target diagnosis writes no private material. BPR-0602/0606 cover the foundation. | Do not infer an RFC-wide pathological-input/privacy audit from those selected courses; the remaining coverage inventory stays on BPR-0617. |
| Active/changed-set scaling, serialized application, incremental Qt and stale/cancellation rejection | Retained scale/cache/history evidence and current core/host/drawer/loading tests exercise those paths with exact identities. Original prepared-index and cancellation proofs remain relevant. | A blanket application-wide structural/benchmark claim is not established; broader Qt layout/contract failures remain visible under BPR-0940. |

## Follow-ups and sequencing

- BPR-0934, Backlog under RFC-0037: Overview folder collapse; never absorbed.
- BPR-0944, Backlog: explicitly owner-deferred unusual-target comparison basis
  and partial-pane presentation. Not a required blocker for the amended selected
  milestone and not a fixed issue.
- BPR-0940, Backlog: pre-existing broader composed GitHub/window-layout contract
  failures. Focused passing courses are not a full-suite pass.
- BPR-0945, Backlog: unchanged shared-header strict-tidy diagnostics.
- BPR-0817, Parked: fresh owner-private Linux roots and Mac profile-management
  checks pass; the pre-existing writable-parent refusal visibility/exit-policy
  decision remains open. No owner decision is invented here.
- BPR-0825, Parked: Windows supervisor/ACE/junction fixes and native evidence are
  integrated; shared descriptor ownership is a separate nonblocking design
  question. BPR-0622 stays Done.
- BPR-0617 retains the original matrix gaps and BPR-0507 dependency. No arbitrary
  new card or allocation is needed to restate that existing closeout obligation.

The next canonical closure frontier is RFC-BPR-0008 / BPR-0242. Its BPR-0143
mixed-client and BPR-0144 acting-identity checks remain parked for designated
live GitHub authority; BPR-0641 separately remains human-pending. This assignment
authorizes neither those mutations nor their acceptance. RFC-0014 follows 0013
but is not the next eligible terminal closeout.

The [final selected-milestone checkpoint](../rfcs/plans/rfc-bpr-0013-hardening/checkpoint-rfc0013-selected-diff-d7-r22.json)
and [closeout handoff](../rfcs/plans/rfc-bpr-0013-hardening/handoff-rfc0013-closeout-d7-r22.json)
bind the amended result, complete card inventory and stopping boundary.

## Final verification and ownership

At unchanged product source, the 17-test focused/public-path pass is green in
52.91 seconds: core/provider read policy, epoch/workbench host, held loading,
drawer/diff, strict endpoint fixture, actual selected-file safety composition,
provider/pager, composed logging/backpressure and profile management. The ignored
log is `build/rfc0013-resume-20260919/r24-closeout-focused.log`. Three additional
queue scheduler/Qt/composition checks pass in 4.92 seconds at `ef6d2d7f` in
`r24-closeout-queue.log` beside it; they do not claim a rendered offline course.
All use safe fixtures. Prior signed r23 native proof, source/anchor matches, 5/5 external CLI
fixture tests and relevant scoped formatting are reused, not rerun against the
private target. Strict tidy and the broader Qt suites remain explicitly non-green.

The documentation gate and its 64 behavior tests pass; the scoped board is valid
with 937 total cards. The first documentation attempt correctly refused a renamed
closeout-checklist heading. The required heading was restored with a current
nine-row matrix, and the old matrix retained separately; no verifier rule changed.
Plan structure/reference checks and `git diff --check` pass.

The current round launches no native app or review server. Owner-launched r23
PID 5964 was discovered on entry and is untouched. Session
`cbd48b91000b00f1e72e1ad62c7c384c` remains at 443; only unrelated BPR-0755 is
protected. Historical review packet bytes/links are not edited. The unrelated
`.mask.lock.json` and ignored nested worktree are preserved.

Documentation impact: RFC, index, README, live cards and plan describe the new
acceptance boundary. Core policy, typed requests/events, authority, concurrency,
persistence, provider/authentication, package and security behavior have **no
runtime change** in this documentation-only closeout round. System architecture
continues to describe the currently implemented both-pane refusal, not the
deferred partial-pane design.

## Standalone BPR-0945 tidy follow-up — 2026-09-25

The r4 transfer above remains the historical RFC closeout decision. Its later
standalone repair changes only `dif_runtime.hpp`'s inclusion guard and the
unsigned intermediate arithmetic of Home's existing four-bit mask, at
`f8b589e7f10811126c1b811ca6f2b28d48738e38`. The canonical helper first reproduced
both recorded diagnostics, then passed the four affected translation units with
warnings as errors and the host SDK. Seven focused Home, SQLite, Dif and composed
Qt checks pass. The [BPR-0945 handoff](../rfcs/plans/bpr0945-tidy-headers/HANDOFF-2026-09-25.md)
records exact commands and candidate limits. Public values/types, persisted
schema, behavior, provider authority, lifecycle, assets and build selection have
no semantic change. No wider tidy or native-platform acceptance is inferred.
