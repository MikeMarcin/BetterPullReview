# BPR-0972 application operation inventory and evidence

Status: in progress. This is the current evidence record for the complete
`application-wide-scaling` milestone; it does not yet accept AC1–AC3.
Mandate baseline: `a2fded0cd482aaaaaee0399764b60145cd4938ac`.
Current segment: `codex-takeover-worktree`, amendment a2 in
[the mandate](../rfcs/plans/bpr0972-active-changed-scaling/mandate.md).

The observations below use safe local fixtures on macOS arm64, Qt 6.11.2,
the Debug dev graph and offscreen Qt. They do not establish Windows/Linux
native behavior or BPR-0967's optimized latency and memory budgets.

## Public operation inventory (AC1)

| Operation | Public entry and retained owner | Bounded work and remaining investigation |
|---|---|---|
| File selection | Actual Files list/tree selection → `workbench_application_host::apply` → exact selected-diff request → epoch actor and diff host → comparison publication | Host and drawer use retained identity indexes; selection no longer copies the file projection or rebuilds Overview. Installed Git resolves only selected material on the epoch actor. Reading restoration and tab membership checks still need inspection. |
| Receipt | Seen / Mark read / Accept / Exclude QAction → `review_ledger_application_host::dispatch` → receipt projection and workbench actionability | Durable receipt changes update one Overview row and a typed FilesDelta. Catalogue observation and full file projection are avoided. Contiguous ordered storage still shifts positions. The ledger's synchronous store calls remain an identified decision-lane issue. |
| Conversation | Actual conversation card Mark read → `mark_conversation_read` → ledger → `publish_thread_read_state` | A retained thread index selects one root; read-state recomputation visits that thread's posts. The panel caches selected-file roots and unread navigation; cards and thread map retain indexed state. |
| Queue | Open/Refresh or conversation activity → profile queue host → typed provider completion → Home and Repositories | Full joins are no longer quadratic. Remaining: per-item publication, Repositories reconciliation instead of model reset, and exact cancellation/replay evidence. |
| Overview | Accepted PR snapshot, durable receipt publication and changed-since-review terminal → retained Overview projection/index | Initial complete acceptance visits snapshot files/threads and joins receipts. Receipt deltas update changed rows only. A changed-since-review request binds exact authority/head inputs; stale observations do not publish. |
| Epoch | Updates line / selector → epoch host → repository actor → exact catalog and comparison update | The catalog/page and comparison identities remain typed and serialized. Remaining: catalog-only changes currently rebuild the entire Overview. |
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

The composed course holds 515 files, 1,000 foreign-authored threads, and a
20,000-line source. File selection reads one receipt and rebuilds no Overview.
Seen and Mark read each publish one Overview receipt row. Mark read publishes
one actionable-file delta, retains the readable comparison, performs no
full catalogue observation, and resets no model. Conversation Mark read visits
one thread and one post. Scrolling visits six alignment segments and rebuilds
no accepted index, axis or pane measurement. Both same-root rounds cancel the
sibling window's held provider and preserve the survivor's exact source; no
remote mutation is submitted. Process allocation and per-gesture elapsed
counts, stronger queue/epoch checks and negative controls remain outstanding.

## Current limits and unfinished obligations

AC1 inventory needs final concrete source/check bindings. AC2 queue and epoch
repairs, declared work counters, and negative controls remain. AC3 allocation
counts and expanded composed gestures remain. Strict tidy, the reviewed
composition digest reseal, one final dev integration pass, documentation gate
and acceptance-driven board finish remain. No native or human result is inferred.
