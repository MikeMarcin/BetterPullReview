# Find and Find in Files continuation

Mandate `find-and-find-in-files-v1-r1`; run `find-and-find-in-files-v1`, segment
`coordinator-implementer-001`. The direct request is to implement the original
Find draft through coordinator/implementer. Q1 full-file both-side search and Q2
active-tab exact comparison are settled. The user required import on main;
main import `e4dd160d6` satisfied that amendment. No acceptance waiver was added.

Source/tracker root: `/Users/jaeger/.codex/worktrees/a675/BetterPullReview`.
Branch: `codex/find-and-find-in-files`. Immutable RFC_BASE_SHA:
`aa3d71fe470627e1e78488eb51a9114a788f33bd`.
[Resolved strategy](execution-strategy.json) remains coordinator/implementer,
with one fresh nondelegating `rfc-implementer` per milestone. M1 route:
`gpt-6.1-sol`, `xhigh`.

## Accepted M1

`search-session-policy`, [[BPR-1419]], completed through the public Qt-free core
and host API. Product `34256fb55`; exact adopted main
`bc4b4aafe6f51020aa469c519e936d616e124e89`; integrated candidate
`7efc8c2d547bc74208069f9e7331e0f0253bc107`; acceptance commit `213a7dd15`.
[Binding](m1-binding.json), [acceptance](m1-acceptance.json),
[scoped integration](m1-scoped-integration.json), [self-review](m1-self-review.json)
and [ledger](../../../../docs/verification/RFC-BPR-0077.md) preserve the result.

Changed product paths are `src/core/{include/bpr/core/source_search.hpp,src/source_search.cpp,CMakeLists.txt}`,
`src/application/{include/bpr/application/source_search_host.hpp,src/source_search_host.cpp,CMakeLists.txt}`,
`tests/core/{source_search_tests.cpp,CMakeLists.txt}`,
`tests/application/{source_search_host_tests.cpp,CMakeLists.txt}` and
`tests/support/source_search_fixture.hpp`. Documentation is
`docs/architecture/source-search.md`, the system-architecture link, the live
Find RFC/ledger, its own cards and this checkpoint tree.

Core is trivially copyable, 1,528 bytes on the measured Mac, with a 2 KiB compile
ceiling. Scope/request equality rejects reversed, foreign and duplicate
answers. Query ownership survives corpus-unavailable gaps. The host retains
one effect and one latest replacement, with at most 64 deferred feedback
operations and 4 KiB queries. 101 admitted edits produced 101 terminals with
only two worker calls; 1,000 match steps called no worker. Empty, partial,
invalid-pattern and limit status stay distinct. Reentrant stop/shutdown,
projection failures and refused/throwing effects are covered.

Build, scoped core/host, both architecture tests, touched format/tidy22,
documentation and its 67 regression tests pass. The full core-only invocation
is **failed**, 269/272, with [dispositions](m1-integration-failures.json):
adopted-main C++ syntax-fixture format, adopted-main GitHub fixture inventory,
and uncached LibArchive acquisition attempted under the network-denied Qt
provider test. The first two reproduce on an exact main export; failing inputs
are byte-identical to adopted main. No external repairs or global passing claim
were made. The scoped four-case result is separate from that full-suite result.

## Accepted M2 and next engineering owner

`shared-matching`, [[BPR-1420]], completes on product/ownership-document candidate
`c88131c9d0ca331da95eab1d701de718ef557f9e`; the acceptance/finish commit is recorded
in [m2-return.json](m2-return.json). [Acceptance](m2-acceptance.json),
[scoped integration](m2-scoped-integration.json), [self-review](m2-self-review.json)
and the [ledger](../../../verification/RFC-BPR-0077.md) bind the unchanged mandate.
All four M2 exits pass. Core state is 1,592 bytes after fixed selection identity;
text, programs and results remain outside it. Exact pane records reach the
serialized acceptance boundary; traversal shares the same immutable collection.

Start a fresh owner on `exact-pr-corpus`, [[BPR-1421]], bind accepted milestones
`search-session-policy` and `shared-matching`, and claim before editing. Follow
[the authored plan](../../../rfcs/plans/find-and-find-in-files-v1/milestone-plan.json).
All original AC01–AC08 remain Pending. The final card retains their complete text.

The core public API is `source_matcher.hpp`: compile once, evaluate path filters
before acquisition, stream exact ordered entries through `match_source_entry`,
and preserve its qualified limits. A PR execution returns `source_search_execution`
with exact records/counts, under the existing request identity. M3 still owns exact
historical catalogue membership, epoch actors/acquisition, immutable read coalescing,
aggregate corpus/cache/results admission, 128-result batching, coalesced progress
and navigation priority. M2's 64 MiB per-entry reservation is not proof of that
complete aggregate. Existing source/diff caches were not enlarged. No source
acquisition, shipped Find action, Qt command/highlight/reveal, cold measurement or
native/human acceptance is inferred from M2. Review interpretation is not pending.

The required frontend-design skill is available at
`/Users/jaeger/.claude/plugins/cache/claude-plugins-official/frontend-design/f359d43b4060/skills/frontend-design/SKILL.md`.
It was read for preparation; later UI work must load it and respect the RFC's
native palette/font/control constraints. No live desktop or personal app data
is an evidence surface.

## Ownership and command state

M1 commands have ended: core-only configure/build, focused tests, full core-only
integration, touched format/tidy, documentation, baseline probes and bookkeeping.
The failed initial compile and fixture-tidy findings were corrected; the first
board-finish invocation was refused before mutation because repeated `--paths`
flags supplied only their final value, then retried with one explicit file list.
The source mutation negative control and random baseline export were removed.
No agent descendants, review server, frozen candidate, remote mutation or push
exist. There is no human/native/independent decision required for M1. Existing
core-only build output is retained for the successor. Main ownership belongs
to its own chat; this lane authored no main changes.

The M1 engineering writer is released at handoff. Any final checkpoint-only
commit changes no tested product bytes; candidate identity and later tracker
revision are kept distinct in the return record.

## M2 final ownership and command state

The M2 writer is released at handoff. [Command state](m2-command-state.json) lists
completed build, scoped integration, format/tidy, reference, mutation and documentation
commands. No descendants, protected/frozen candidates, review server, live desktop,
main writes, remote mutation or push exist. Task-owned oracle/UCD copies and diagnostic
scripts were removed after receipts; the shared core-only build is retained for M3.
Any final checkpoint-only commit changes no tested product bytes. Product identity
and later tracker revision are kept distinct in the return record.

## Accepted M3 and next engineering owner

`exact-pr-corpus`, [[BPR-1421]], completes on tested product
`df4b42d6942d131caebc2343a6ed360c8023bc0d` (initial implementation
`4b6bc16e50d6e134efc3570c2dccff74898e0d65`). [Acceptance](m3-acceptance.json),
[self-review](m3-self-review.json), [correction checks](m3-correction-verification.json)
and the [ledger](../../../verification/RFC-BPR-0077.md) bind all four M3 exits.
The guarded finish and latest checkpoint/tracker revisions are in
[m3-return.json](m3-return.json). This section supersedes the earlier M2 next action.

Start a fresh owner on `pane-find`, [[BPR-1422]], bind accepted milestones
`search-session-policy`, `shared-matching` and `exact-pr-corpus`, and claim the
Ready card before editing. Read the required frontend-design skill before UI
work. Original AC01–AC08, shipped native commands/rendering/reveal, final composed
races and large-review warm/cold performance remain pending. No acceptance waiver
or engine question was introduced. M1's 269/272 broad-core failure receipt remains
retained; do not replace it with this milestone's focused passing claim.

### Composition APIs

- Pane Find uses `execute_source_pane_match(work, acceptedEntry, stop)` from
  `source_search_host.hpp`. It accepts the exact full accepted pane material and
  performs no acquisition. Keep pane and PR hosts/query state independent. Before
  admitting pane Find work, call the epoch port's `yield_to_source_navigation()`;
  actual selected-source work also interrupts background reads/scanning through
  the existing reviewer-work guard. M4 owns controller, decoration and exact reveal.
- PR composition (M5) calls
  `epoch_application_host::bind_source_search(currentWorkbenchAuthority, pinnedComparison, activeTab)`
  on the decision lane. Retain that optional scope while the exact authority/tab/
  comparison remains valid; query/options/path changes go to the search host, not
  a fresh catalogue grant. The scope carries a catalogue generation independent
  of query/request generations. Rebind only for a real scope change, and pass
  `nullopt` when unavailable. Never substitute the current accepted catalogue for
  the pinned historical pair.
- Construct `source_pr_search_execution(epochHost)` and a `source_search_host`
  with the existing decision/effect executors and a projection port. Use
  `set_scope`, `set_query`, `set_paths`, `retry`, `navigate`, `clear`, `close` and
  `shutdown`. All host ingress/inspection runs on the serialized decision lane;
  epoch host, execution/projection ports and executors outlive host quiescence.
- `source_search_projection::Corpus` is the immutable exact ordered catalogue.
  Its entries provide `File`, optional `Left`/`Right` path/blob/mode and primary
  `path()` for result headers. `Matches` holds file identities and exact original
  spans/blob locations; identical same-entry blobs retain both side locations.
  `Skipped` records per-file/side typed reasons; `RetryAvailable`, `Summary`,
  `Status` and `NoResults` qualify coverage. Only complete empty coverage may
  present an unqualified no-results label. `retry()` retains the same scope.
- The epoch corpus port performs all reads. Its `catalogue`, `read`,
  `demand_changed` and `scan_stop_token` are worker capabilities, not widget
  effects. Material is immutable and two documents are streamed at a time;
  replacement demand is installed before cancelling an old matcher so a still
  needed exact blob read can be shared. Path policy validates before acquisition.
- M4/M5 must still compose authority/tab/comparison invalidation and cancel any
  pending reveal on later reading gestures. Result activation binds exact
  authority, catalogue, tab, comparison, file/path/side/blob/span and current query
  generation through the existing selection/reveal controller; the corpus API
  alone does not supply native activation acceptance.

### M3 ownership and command state

The M3 writer is released at handoff. [Command state](m3-command-state.json)
records completed core/Git builds, focused contracts, architecture checks,
negative control, corrected format/tidy, documentation and bookkeeping. No
descendants, protected/frozen candidates, review server, live desktop/personal
app data, main writes, remote mutation or push exist. Temporary mutation and
tidy scripts/databases are removed after receipts. The owned `build/core-only`
and `build/find-corpus-git` trees are retained for successors. The latter uses
the existing pinned main dependency cache read-only, offline, with Qt 6.11.2;
it built only Git contract targets, not a new UI acceptance candidate.

Full-file Git tidy retains two unchanged baseline diagnostics (the existing
`small_text` name and pack-fixture temporary concatenation). M3's changed lines
pass separately; no unrelated code repair or global tidy pass is asserted.
Product fingerprints and exact scoped candidate identity stay distinct from
later checkpoint-only commits. There is no required human/native/independent
decision for this bounded M3 outcome; later milestones retain those obligations.
