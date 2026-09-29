# BPR-0972 application operation inventory and evidence

Status: AC1–AC3 accepted for the complete `application-wide-scaling` milestone.
This is structural acceptance of the inventoried paths, with the residual I/O,
integration-failure and platform limits below; it is not a whole-architecture
or hardware performance pass.
Mandate baseline: `a2fded0cd482aaaaaee0399764b60145cd4938ac`.
Current segment: `codex-takeover-worktree`, amendment a2 in
[the mandate](../rfcs/plans/bpr0972-active-changed-scaling/mandate.md).

The observations below use safe local fixtures on macOS arm64, Qt 6.11.2,
the Debug dev graph and offscreen Qt. They do not establish Windows/Linux
native behavior or BPR-0967's optimized latency and memory budgets.

The final product/test candidate is `149704df`, including the sibling reload
failure correction after `90e21d0c` (product `2cb4c2be` plus the 32/4096
maintenance-counter assertion). Earlier measured runs retain their original
revision below; final integration checks the repaired candidate. Here **active** includes the complete accepted
PR's compact ordered navigation/index state, the selected source/thread, open
window consumers, retained bounded epoch page and accepted queue/repository order
(including currently filtered rows).
It does not mean only pixels currently on screen. Initial adoption visits its
bounded complete input. A changed receipt projects only its file data but still
maintains the dense active order: host order maintenance is O(active order +
changed files), not O(changed files) alone. This is a per-receipt-transition
bound: the shipped receipt port withholds only the currently retained file,
so a transition restores at most that file and withholds the new retained file.
It is not a bound for an arbitrary bulk set of withheld rows. Qt folder-shape fallback additionally
builds and sorts the active tree before reconciling identities; no linear-time
or changed-only construction claim covers that fallback. No zero-total-row-work claim is made.

## Public operation inventory (AC1)

| Operation | Public entry and retained owner | Work, retained bounds and residual costs |
|---|---|---|
| File selection | Actual Files list/tree selection → `workbench_application_host::apply` → exact selected-diff request → epoch actor and diff host → comparison publication | Host and drawer use retained identity indexes; selection no longer copies the file projection or rebuilds Overview. Installed Git resolves only selected material on the epoch actor. Reading restoration, tab restoration and link membership use the same retained index. |
| Receipt | Seen / Mark read / Accept / Exclude QAction → `review_ledger_application_host::submit` → receipt projection and workbench actionability | Durable receipt changes update one Overview row and a typed FilesDelta. Catalogue observation and full file projection are avoided. Contiguous ordered storage still shifts positions. The ledger's synchronous store calls remain an identified decision-lane issue. |
| Conversation | Actual conversation card Mark read → `mark_conversation_read` → ledger → `publish_thread_read_state` | A retained thread index selects one root; read-state recomputation visits that thread's posts. The panel caches selected-file roots and unread navigation; cards and thread map retain indexed state. |
| Queue | Open/Refresh or conversation activity → profile queue host → typed provider completion → Home and Repositories | Retained enrollment/activity indexes avoid catalogue joins for an unchanged enrollment. The hub publishes one item to caught-up consumers and a complete replay to new or lagging consumers. Home and Repositories move/update identities without reset; reentrant delivery cannot skip a delta base. Full acceptance, new membership, withdrawal and rule changes retain complete reconciliation. |
| Overview | Accepted PR snapshot, durable receipt publication and changed-since-review terminal → retained Overview projection/index | Initial complete acceptance visits snapshot files/threads and joins receipts. Receipt deltas update changed rows only. A changed-since-review request binds exact authority/head inputs; stale observations do not publish. |
| Epoch | Updates line / selector → epoch host → repository actor → exact catalog and comparison update | The catalog/page and comparison identities remain typed and serialized. A catalog-only update at the same accepted head projects history header facts only. Changed heads and unresolved changed-since-review inputs still require complete acceptance. |
| Source | Actual selected comparison and scrollbar → retained semantic index, measured axis and native source panes | The 20,000-line course scrolls with zero semantic index adoptions, geometry rebuilds or pane measurements. Worker terminals pass existing exact session/authority/file/comparison checks. |

## Retained actionability candidate

The inherited 12-file patch now has dedicated drawer/tree equivalence tests
and a composed file Mark read gesture. The model tests cover tree, flat and
changed-first orders, resident filtering, compacted folders, removing a whole
branch, returning files with their retained Overview facts, and new paths.
Neither model resets. The drawer finds removed identities through its retained
index. Optional catalogue ordinals distinguish unnumbered ports; malformed
withheld entries and an oversized combined catalogue refuse admission.

Commands, from the assigned worktree:

```sh
cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build --preset dev -j2 --target test_better_pull_review_qt_files_tree_tests test_better_pull_review_application_workbench_host_tests test_application_scaling
ctest --preset dev --output-on-failure -R '^(qt_files_tree|application_workbench_host|application_active_changed_scaling)$'
```

Result: 3/3 pass. At both 32 and 4,096 files, three retained actionability
changes publish three moved files with zero observations, accepted catalogue
rows or full file projection rows. This does not count vector position shifts
as zero work and is not a whole-process allocation claim.

The strengthened course reports three moved files at 32 and 4,096 active files:
93 and 12,285 `FilePositionVisits`, and 53 and 8,181
`PublishedFileRowsShifted`, respectively. Both must be nonzero and at most three
times the active-order size. The 515-file composed Mark read reports 514 position
visits and 478 shifted published rows alongside zero accepted/projected file
rows. This explicitly exposes the linear maintenance behind the compact delta.

The composed course holds 515 files, 1,000 foreign-authored threads, and a
20,000-line source. File selection reads one receipt and rebuilds no Overview.
Seen and Mark read each publish one Overview receipt row. Mark read publishes
one actionable-file delta, retains the readable comparison, performs no
full catalogue observation, and resets no model. Conversation Mark read visits
one thread and one post. Scrolling visits six alignment segments and rebuilds
no accepted index, axis or pane measurement. Both same-root rounds cancel the
sibling window's held provider and preserve the survivor's exact source; no
remote mutation is submitted. Process allocation and elapsed observations are recorded below. Final integration and matched baseline diagnosis are recorded below; the separate synchronous I/O finding is retained as BPR-1211.

## Source and completion bindings

The source links below bind the inventory to the actual owners, not similarly
named helpers. Full work means accepting/replacing an entire bounded input;
changed work means retaining that accepted input and applying a named update.
Active ordered rows may still move physically when a rank changes.

- Selection: [`workbench_application_host.cpp`](../../src/application/src/workbench_application_host.cpp), `file_position`, `finish_selected_diff` and `publish_changes`, own the retained index and exact request terminal. The terminal checks session, request domain, generation, complete workbench authority, selected file and selected-diff context before publication. [`github_application.cpp`](../../src/app/src/github_application.cpp), `advance_reading_restoration`, `drive_tab` and link navigation, use `contains_file`; selected material reaches the ledger through `observe_selected_material`. `test_routine_selection_visits_only_the_retained_index` exercises 32/4096 files. The composed course uses the real Files selection model and installed Git actor.
- Receipt and Overview: [`review_ledger_application_host.cpp`](../../src/application/src/review_ledger_application_host.cpp), `observe_material`, `submit`, `refresh_receipt`, `load_receipt` and durable revision tracking, retain one selected material and indexed durable receipts. Composition `publish(receipt_projection)` and `refresh_overview_receipts` update only named rows; [`pull_request_overview.cpp`](../../src/application/src/pull_request_overview.cpp), `overview_receipt_index::apply`, compares with full projection in `application_pull_request_overview`. Initial acceptance, missing/overflowed revision history, a missing durable receipt, changed head, private-record changes and unresolved changed-since-review work retain full Overview projection. These fallbacks are explicit; the changed receipt path is not a blanket claim about every Overview publication.
- File actionability: the workbench host's `retain_actionability_now` restores its small withheld set and moves the named receipt file. Unnumbered catalogs or an in-flight observation use the existing complete observation fallback. The indexed last-published order no longer searches all file identities. `FilePositionVisits` counts dense index maintenance and `PublishedFileRowsShifted` counts contiguous projection-row shifts. [`workbench_drawer.cpp`](../../src/adapters/qt/src/workbench_drawer.cpp) and [`files_tree.cpp`](../../src/adapters/qt/src/files_tree.cpp) apply typed deltas and retain identity; Qt equivalence courses cover filters, branches, new/returning rows, compact folders and persistent indexes with zero resets. `files_tree_model::apply_delta` still calls complete `build_nodes`/`reconcile` when a folder appears, disappears or compacts; that scans the active file tree and rebuilds its identity index, without a model reset. Direct sibling rank moves and contiguous vectors retain position-shift costs. These active-presentation fallbacks are not counted as zero work or changed-only tree construction.
- Conversation: composition `cached_thread`, `publish_thread_read_state` and `mark_conversation_read` select one retained thread and visit its posts. A newly accepted provider snapshot or ledger retirement invalidates the cache. Duplicate conversation identities deliberately use complete observation. [`github_conversation_panel.cpp`](../../src/adapters/qt/src/github_conversation_panel.cpp), [`conversation_cards_view.cpp`](../../src/adapters/qt/src/conversation_cards_view.cpp) and [`github_thread_map.cpp`](../../src/adapters/qt/src/github_thread_map.cpp) retain indexes, selected-file roots and unread navigation. Dedicated 32/4096-thread tests observe eight selected-thread nodes independent of unrelated thread count; one-thread read-state updates equal full state.
- Queue: [`review_queue_enrollment.cpp`](../../src/application/src/review_queue_enrollment.cpp), `finish_refresh`, `publish` and `queue_projection_hub`, own persistence, typed provider completion and per-consumer revision admission. Both an unchanged observed membership and a retained discovery/follow/review with no new role publish one item. New membership, withdrawal, rule changes and full adoption retain complete reconciliation and durable-before-publication. [`review_landing_model.cpp`](../../src/adapters/qt/src/review_landing_model.cpp) and [`repositories_drawer.cpp`](../../src/adapters/qt/src/repositories_drawer.cpp) retain identity indexes and reconcile without resets. Home grouping/proxy filtering still visits the active presentation; ordered rank moves and root placement still maintain affected positions. The 32/4096-row host and Qt courses distinguish that cost from projecting catalog data.
- Epoch: [`epoch_application_host.cpp`](../../src/application/src/epoch_application_host.cpp) dispatches core transitions, repository actor work and immutable terminals to its serialized decision lane. Composition `publish(epoch_presentation_update)` applies retained `EpochRows` before choosing complete Overview or `project_overview_history`. Same-head catalog changes read retained epoch rows twice, with no file copy, sort or receipt lookup; [`pull_request_overview_page.cpp`](../../src/adapters/qt/src/pull_request_overview_page.cpp), `apply_history`, updates header facts and labels only. Complete projection/header equivalence covers unsorted/repeated heads, empty and pruned history at 32/4096 files. A public Refresh on the single unchanged fixture epoch legitimately produces no catalog delta; its course measures the accepted no-op separately.
- Source: [`diff_application_host.cpp`](../../src/application/src/diff_application_host.cpp) owns typed compute and cached-source completions; [`diff_workbench.cpp`](../../src/adapters/qt/src/diff_workbench.cpp) retains the accepted semantic index and geometry. New selected material is parsed/indexed as a complete active source; scrolling visits visible alignment segments and reuses that index. The composed 20,000-line course renders the entire window after a real scrollbar gesture. The cold selected-source allocation sample includes full material adoption.

No untagged asynchronous completion was found in these named paths. This statement
is bounded by the inventory, not every application feature. The existing
`application_workbench_host`, `application_epoch_host`, `application_diff_host`
and `application_review_queue_enrollment` suites inject reordered generations,
stale/foreign/duplicate completions, cancellation despite a port ignoring its token,
reentrant publication, callback failure and shutdown. Queue deltas additionally
exercise a consumer missing a revision during another consumer's callback: it
receives complete state rather than an invalid delta base. Composed sibling tests
preserve exact material, semantic selection and scroll and refuse old-window
notifications. The reading course waits for the real Discuss-to-Read tab
restoration before choosing a new reading line; it does not count that intentional
restoration as receipt fanout.

## Synchronous I/O finding

[BPR-1211](../issues/30-in-progress/BPR-1211-move-review-ledger-reads-and-writes-off-the-decision-lane.md)
retains the architectural gap: the review ledger still performs bounded SQLite
reads/writes synchronously under its host mutex on the decision lane. The actual
composition sites are `dispatch_receipt` → `submit`, `observe_selected_material`
→ `observe_material`, sibling `refresh_shared_ledger` → `refresh_receipt`,
`mark_conversation_read`, `publish_overview` → `load_receipt`/`private_observation`,
and private-context/defect publication → `private_observation`. Initial conversation
observation loads the conversation receipt set; cached routine Mark read does not.
Profile retirement destroys the store/host only after its existing lifetime path.
Queue/provider/Git/cache work uses effect owners; Qt models perform no I/O.

At the observed 515-file/1000-thread course, one file selection loads one receipt
and performs three private observations; two selections load two receipts and six
private observations. Seen and Mark file read each perform one receipt persistence
request, zero receipt loads, zero private loads/writes and zero conversation loads.
Conversation Mark read performs one conversation write and no receipt/private or
conversation loads. These counts do not measure SQLite waiting time. BPR-1211
requires held-store responsiveness, bounded scheduling, exact lifetime/authority,
FIFO and durable-before-publication tests. This finding is neither waived nor
reported as an external blocker, and no blanket no-I/O or whole-architecture pass
is claimed by BPR-0972.

## Compiled negative controls

[The durable results](bpr0972-negative-controls.json) bind eight separate mutations
to `90e21d0c`, including source hashes, exact replacements, build/CTest commands
and refusal messages. Every mutant compiled; every named test failed; exact source
bytes were restored afterward. These are behavior changes, not counter tampering:

| Disabled property | Observed refusal |
|---|---|
| Retained file projection | Routine selection reconstructs the full projection |
| Targeted receipt actionability | A read retained file no longer leaves as a one-file delta |
| Queue item publication | Unchanged enrollment projects the whole catalogue |
| Repository delta work | Single-item changes rebuild the catalogue |
| Repository persistent identity | A reset invalidates the retained child/parent indexes |
| History header update | Header facts differ from complete projection |
| Retained Overview receipt projection | Unchanged history refresh reconstructs Overview |
| Single-thread conversation observation | One Mark read observes unrelated threads/posts |

The positive candidate was rebuilt after these experiments. All owning
structural/public courses pass in the final dev integration; focused core,
format and strict tidy replays also pass. The wider integration has the exact
failures recorded below. Native Windows/Linux remain deferred; no human or
native acceptance is inferred.

## Queue, epoch and allocation observations

The queue host uses retained identity indexes for enrollments and activity. A
refresh that leaves the existing durable membership unchanged publishes one
`UpdatedItem` and performs no persistence request or complete register projection.
The projection hub retains the complete state for a new attachment and for a
consumer that missed a revision during reentrant publication. The legacy
single-sink constructor receives complete snapshots; the shipped profile/window
composition receives deltas. Notification observation uses the same changed item.

`test_known_refresh_delta_and_complete_attachment_replay` exercises 32 and 4,096
records, including reentrant publication between deliveries. Each normal refresh
projects zero catalogue rows and one changed item. New and lagging consumers
receive the same complete queue as `host.queue()`. Existing stale-session,
cancellation, callback-failure and generation-admission tests remain in the same
executable. `test_incremental_queue_matches_complete_projection` compares every
row against the complete core order at both sizes, through bucket moves in both
directions, title edits, filtering, root disappearance and reappearance. Root and
child persistent identity and collapsed state survive. Routine updates emit no
reset; title-only updates perform no positional reindexing. Contiguous rank moves
still shift the intervening positions, counted separately from item projection.
Home's grouped summaries and Qt proxy filtering are still active presentation
work and are not reported as zero work by these model counters.

The epoch history helper reads retained epoch rows without copying or sorting the
file catalogue. Tests compare its update with complete Overview at 32 and 4,096
files for unsorted/repeated heads, pruned and empty history. The Qt page test
compares exact header labels and all retained facts with a complete page and
asserts zero file-model reset, move, insert, remove or data-change signals.

Focused command:

```sh
ctest --preset dev --output-on-failure -R '^(application_review_queue_enrollment|qt_repositories_drawer|application_pull_request_overview|qt_pull_request_overview_page|application_active_changed_scaling)$'
```

Result: 5/5 pass, 65.71 seconds. The subsequent direct one-row tree update and
newly matching filtered-root insertion pass `qt_repositories_drawer` again
(2.24 seconds). These are local macOS Debug/offscreen observations.

The following allocation samples were recorded during the final integration on
`149704df`. The course passes in 34.55 seconds. Its [complete structural
observations](bpr0972-final-verification.json) retain both same-root rounds. The
composed course instruments unaligned scalar and array C++ `operator new`
across all threads. A direct 37-byte allocation calibrates the counter before the
course. These are requested bytes and calls, not retained memory, peak RSS, Qt/C
`malloc` or aligned allocations. Elapsed time includes fixture work, event-loop
polling and settling, so this is structural evidence, not BPR-0967 performance
qualification. No allocation threshold is inferred from one hardware run.

| Gesture, first same-root round | New calls | Requested bytes | Elapsed microseconds including settle |
|---|---:|---:|---:|
| Queue item update during a complete PR Refresh | 380,603 | 505,009,789 | 1,019,242 |
| Refresh unchanged epoch history | 37,225 | 5,476,526 | 1,088,443 |
| Select one file | 32,109 | 4,097,160 | 1,031,475 |
| Seen | 979 | 114,419 | 148,886 |
| Mark file read | 1,507 | 190,973 | 149,224 |
| Mark conversation read | 1,251 | 210,973 | 150,371 |
| Source scroll and render | 24,466 | 2,121,568 | 30,180 |
| Two selections including cold 20,000-line source during sibling provider load | 1,596,006 | 292,513,261 | 2,741,140 |

The earlier focused course at `2cb4c2be` passed in 36.28 seconds; the final
34.55-second replay binds these measurements to repaired source `149704df`.
The public Follow first durably enrolls the fixture PR. Actual PR Refresh then
updates exactly one queue item, visits zero queue-catalogue rows and performs
zero repository full builds. Its allocation sample includes complete provider
snapshot acceptance, not just the queue delta. Refresh unchanged history
completes ready/quiescent with zero file projection and Overview rebuilds,
one selected-material receipt load and six private observations; it emits zero
catalogue/header deltas because that fixture epoch did not change. The separate
history helper/page courses provide changed-header equivalence evidence.

The second same-root round observes durable receipt recovery and repeats concurrent
loading/cancellation. Its selection measured 31,992 calls / 4,019,612 requested
bytes; source scroll 22,512 / 1,935,456; the two selections 1,593,529 / 292,180,861.
Re-selecting the durable read file also removes that file from its restored active
order (514 position visits / 478 shifted published rows), without full projection.
The large cold-source measurement includes construction of that selected material;
it is not a claim that source adoption allocates only visible lines.

## Integration prerequisite repair

Commit `ecf93cfd` separates the existing terminal writers' Windows
`CancelSynchronousIo` use from Job Object/handle-inheritance containment ownership.
It names only the two exact terminal owners, registers the MCP header in its
executable, and retains negative controls refusing containment there or cancellation
in another MCP file. Both diagnostic probes now use contained temporary support.
Focused ownership tests passed; `architecture_boundaries` and
`contained_temporary_usage_contract` passed 2/2; MCP helper, MCP terminal and app
terminal diagnostic rows passed 3/3. The coordinator landed the repair as
`ca23a9fe`; this is no claim of native Windows execution or BPR-0972 completion.


## Sibling reload-failure regression

The exact selected-material reload and the absence of selected material are now
separate branches. A failed SQLite load retains accepted receipt/actionability;
only missing exact material restores conservative order. The public multiwindow
course commits through the writer's actual QAction, temporarily renames its own
disposable receipt table before queued sibling delivery, observes the real store
failure, restores the table, and retries through another public write. The old
production candidate fails the accepted-order assertion (74.74 seconds); the
pending-selection cache concern was disproved because `clear_diff_selection`
clears `ReviewLedgerReceipt` before starting the next request.

FilesDelta does not alter the pre-existing folder hierarchy or indentation: its
full-projection equivalence covers the existing flat, tree, compact-folder and
changed-first modes. The owner's later folder-layout clarification is separately
assigned to BPR-1213 by the coordinator, not an acceptance claim for this patch.

The repaired sibling course passes in 80.51 seconds, including later PR replacement; the resealed composition contract and strict application/test tidy also pass. The table fault is restored using its original canonical schema SQL and preserved rows, so the existing schema-admission guard remains effective.


## Final acceptance and integration diagnosis

AC1 passes with the seven-family public-operation inventory, concrete retained
owners, full-input and presentation fallbacks, position maintenance, synchronous
store sites/counts, and exact completion guards above. AC2 passes with the
32/4,096-record equivalence/identity courses, eight compiled negative controls,
reordered/stale/cancelled/reentrant host terminals and real composed gestures.
AC3 passes with the 515-file/1,000-thread/20,000-line course on `149704df`, both
same-root rounds, held-provider cancellation, retained exact source, final
allocation/reset/visit observations and explicitly bounded platform claims.
The sibling SQLite failure/recovery course passes again in the full integration
(74.92 seconds), after its earlier 80.51-second focused pass.

The complete Debug dev pass ran all **471 tests: 467 passed, four failed**,
exit 8, 6,947.28 seconds. It is not an all-green run. The full stdout and CTest
log hashes, exact commands, binary hashes, host and selected configuration
settings are in [the final verification record](bpr0972-final-verification.json).
Only the scheduler was paused at rows 155, 352, 370 and 393 to serialize Qt
courses across implementers. Each current child finished before a slot grant.
At row 393 the passing child emitted its marker and exited 0, but the paused
scheduler crossed its 120-second parent timeout. Its unchanged, unpaused replay
passes in **12.51 seconds**. Those coordination durations are gate context only.

After the full pass, a fresh immutable `git archive` of
`a2fded0cd482aaaaaee0399764b60145cd4938ac` supplied the baseline. All 796 tracked
build-source blobs matched Git. The same Debug preset, Apple clang 21, Qt 6.11.2,
offline dependency cache and deployment settings were used. Only the benchmark
and Dif-context executable were built, with two jobs. The three baseline and
four candidate diagnostic rows then ran serially with process-scoped caffeinate
while the other implementer explicitly held both Qt and builds; OS-wide load
still is not verified.

| Diagnostic | Immutable baseline | Candidate `149704df` | Unchanged bound/result |
|---|---:|---:|---|
| Accepted metadata, responsiveness | 275.058 ms | 287.325375 ms | Both fail 250 ms |
| Accepted metadata, launch | 278.371375 ms | 279.309292 ms | Both fail 250 ms |
| Dif comment-ready and acknowledge cues | Same two failed terms | Same two failed terms | Draft identity and mutation refusal still pass |
| Workspace diagnostic without scheduler pause | Not repeated | Pass, 12.51 s | Existing assertions retained |

Both timing profiles have empty `InputBudgetFailures`; their only failed
completion budget is accepted metadata. The original full-run candidate values
were 289.124125 and 287.6695 ms. No threshold or assertion changed. These paired
observations establish that the failures predate this milestone; they do not
establish timing parity or attribute the modest duration differences. BPR-0967's
existing owner amendment requires optimized performance acceptance; its 250 ms
metadata ceiling was derived from Release observations, while these Debug runs
are diagnostic/correctness context. No new performance qualification is claimed.

The immutable baseline also reproduces the exact Dif-context cue failure. The
separate BPR-1209 owner changed workshop/corner placement and fixture visibility
setup at `8206236f`, preserved both cue assertions, and reported that course
passing in 20.33 seconds. That is independently owned integration evidence, not
a repair claimed by BPR-0972; the coordinator must check the combined candidate.
No additional all-green replay is inferred from that report.

BPR-1211 retains synchronous ledger I/O as an architectural gap. Windows/Linux
native, optimized hardware budgets and MASK/native-serving readiness remain
unaccepted. `host_load` remains `not_verified`. No live account, private target,
remote mutation, release, push, new attended session or delegation was used.
