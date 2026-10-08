---
schema: "implement-rfc.card.v1"
id: "BPR-1327"
title: "Prepare changed-file comparisons before they are opened"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner chat 2026-10-08","kind":"performance","related_issues":["BPR-0967","BPR-0905","BPR-0794","BPR-0795"],"branch":"claude/bpr-1327-prefetch"}
---

# Prepare changed-file comparisons before they are opened

## Owner report (chat, 2026-10-08)

> "For even a small review it seems to not be preloading the files until I go
> to view their diff, it's quite slow to open the diff file against a live pr"

## Diagnosis

A pull request opened through the connected GitHub CLI loads its comparison
with `git_smart_acquisition_mode::sparse_commit_trees`: the clone is
`--depth=1 --filter=blob:none`, so the cache holds commits and trees and no
file contents. `object_loading_for` chooses this for every pull request whose
reported shape is consistent, small ones included (the
`sparse_capability_applies_independently_of_pull_request_size` host test
pins it). Nothing reads a blob until the reviewer selects its file.

Opening one file then does, in order, on the repository's single epoch actor
FIFO:

1. Two local `git ls-tree` lookups (path to blob at each endpoint; four for a
   rename).
2. For each side, a local `cat-file --batch-check` that misses, the durable
   sparse-residency checks, then `read_exact_blob`: three `gh` processes,
   `gh api user`, `gh api repos/{o}/{r}/git/blobs/{sha}`, `gh api user`.
3. The diff computation on the diff worker.
4. Syntax classification, which reads both blobs again through the same
   materialization port. The network bytes are never retained, so it repeats
   step 2's network reads.

Measured live against `MikeMarcin/BPR-Test` pull request 40 (3 files, 4
blobs) with `gh` 2.101.0. The transport's own shape (viewer, blob, viewer)
costs 1,196–1,538 ms per blob; through the CLI account-observation wrapper,
which adds a viewer read on each side, the shipped read is five `gh` calls and
costs 1,616–1,861 ms per blob. A modified file therefore waits about 3.4 s for
its two blobs before the diff can appear, and the classifier's repeated reads
add as much again on the same FIFO. A revisit after the diff cache evicts the
file pays it all again.

One GraphQL query reading the same four blobs by OID took 488–628 ms (cost 1
point), 1,881 ms with the same four account observations around it, and every
returned text hashed to its blob OID. The exact variables-only query the
transport compiles was checked live the same way.

## Acceptance

- AC1 (measure): before and after numbers for opening the first file and
  later files, on a live-shaped fixture with real fetch latency, small and
  large pull requests.
- AC2 (prepare): after a comparison is available, every changed file's
  comparison is prepared in the background in the order current file, next
  file, then the rest; a huge pull request prepares only a bounded window
  around the cursor. Opening a prepared file performs no network read and no
  diff computation.
- AC3 (bounds): preparation stays inside the existing diff cache entry and
  byte budgets, retains a bounded set of verified blob bytes, never evicts a
  comparison the reviewer opened, reads missing blobs in batches, and is
  cancelled by a pull-request, comparison or account change and by close.
- AC4 (responsiveness): the 18 large-review rows pass enforced on an
  optimized tree; the reviewer's own selection is never queued behind more
  than one preparation step.
- AC5 (tests): prepared-file open latency, cancellation, budget caps and the
  large-review window each have a test that fails before the change.

## Design (2026-10-08)

The [system architecture](../../architecture/system-architecture.md#concurrency-and-stale-work)
records the design. In short:

- **Order** (`core::prepared_comparison_order`): current file, the files after
  it, then the ones before it, in a window of 16. Up to 16 files the whole
  review; beyond, the window around the cursor, moving with it.
- **Resolution** (`epoch_application_host::prepare`, on a new `PrepareEffects`
  worker): each file is resolved on the repository actor as an open would be
  and kept as a prepared grant (at most 64), dropped with the selected grant.
  Opening a prepared file reuses it without Git.
- **Blobs**, sparse reviews only: the grants' missing blobs are read in
  batches of at most 16 (each at most 512 KiB) through
  `git_smart_transport_port::read_exact_blobs`, which the GitHub CLI transport
  implements as one GraphQL read between one pair of account observations.
  The network round trip runs off the actor. The host re-checks each blob's
  SHA-1 and retains verified bytes per actor (256 blobs, 16 MiB), so the
  classifier's second read and revisits stay local. A transport without
  batches reads nothing ahead.
- **Diffs** (`diff_application_host::prepare`): one at a time, only while no
  requested comparison is outstanding, stored unpublished inside the existing
  cache with a share of at most a quarter of its entries and an eighth of its
  bytes (16 entries, 8 MiB), never evicting a comparison the reviewer opened
  and evicted first. A request for the comparison being prepared waits for
  it; any other request stops it. A prepared side is never larger than a
  quarter of the byte share.
- **Lifecycle**: a new window cancels the old one; pull-request or account
  invalidation cancels it and discards queue and entries; window close stops
  it and shutdown waits for it.

## Measurements (macOS arm64 M4 Pro, Qt 6.11.2, Apple clang Release, tests-on)

The machine was shared with other agents' builds throughout (load average
45–200 on 14 cores), so timings carry that noise; before and after were run
interleaved under the same conditions.

**Small live review, live-shaped host fixture.** `application_epoch_host`'s
`live_shaped_open_latency` runs the real epoch host over a sparse three-file
review whose transport sleeps the measured live round trips
(`--live-single-blob-ms=1700 --live-batch-ms=1881`). Time to have both sides'
bytes for a modified file:

| | first file | later file |
|---|---|---|
| before (exact single reads) | 3,407 ms | 3,425 ms |
| after, once prepared | 0.15 ms | 0.10 ms |

Preparing all three files took 1,884 ms in the background (one batch). The
registered row runs the same course with 20/25 ms stand-ins. The classifier's
second read of each blob, another ~3.4 s per file before, is now local.

**Large review, real composition and local Git** (515 files,
`application_large_review_prepared_open`): time until the opened file's
comparison is on screen.

| | quiet machine (load 2) | loaded (load 20–110, 4 runs) |
|---|---|---|
| unprepared opens (preparation off, same window) | 58–62 ms | 77–114 ms |
| first open, with its window being prepared | 82 ms | 110–155 ms |
| later opens in the prepared window | 15.1–17.4 ms | 17.5–22.8 ms |

Every prepared open was answered by its prepared comparison, and the course
requires every prepared open to be faster than every unprepared one. In
`application_large_review_cache_entries`, which opens 65 files in order, the
median later open is now on screen in 16.2 ms. Time to quiet (classification
included) is unchanged at about 190 ms there: classification is not prepared.

## Verification

At the head of branch `claude/bpr-1327-prefetch`, rebased on `2d410dfbf`, macOS arm64
optimized tests-on tree (`dev` preset, `CMAKE_BUILD_TYPE=Release`):

- The 18 large-review rows plus the new
  `application_large_review_prepared_open`: 19/19 pass, every report
  `BudgetsEnforced` true with no `InputBudgetFailures` and no
  `UnenforcedLimitMisses`: 182.6 s with a largest input gap of 45.3 ms after
  the final rebase (load 2), and 255.6 s with 56.1 ms before it (load 20–60). Baseline at the
  base commit: 18/18. The two courses that count cache entries now count the
  reviewer's own entries and bound the prepared share separately; no limit
  changed.
- Focused: `core_comparison_preparation`, `application_diff_host`,
  `application_epoch_host`, `application_workbench_host`,
  `gh_cli_git_smart_transport`, `github_provider`, `architecture_boundaries`,
  `application_review_round_trip_composition_contract` (resealed here),
  `application_composition_contract`,
  `application_profile_window_review_restoration_public_path`: all pass.
- Failing-before controls: the diff host's prepared-open test runs an
  unprepared control; the epoch host's live-shaped test asserts the control
  pays two network reads per open; the prepared-open course runs preparation
  off first and requires every prepared open to beat it. The course failed on
  the first composed build (prepared requests carried no request identity, so
  every prepared resolution failed) and on the second (prepared opens were
  labelled "(cached)", which the binary-fallback check caught); both are
  fixed.
- `coding-format-check` passes; clang-tidy reports nothing new in the changed
  code. `verify_documentation.py`: DOCUMENTATION_OK.
- Broad composed set (`^application_|qt_composed|epoch|diff`, 253 rows,
  `-j4`): 249 pass. The four failures also fail at the base commit:
  `qt_composed_inline_drawer_*` (BPR-1332) and
  `application_profile_window_layout_public_path` (its seed process crashes).
  `application_private_review_native` failed once at the base and once with
  this change, and passed on the rerun.

### After the rebase onto `9a8208fba` (2026-10-08)

The branch was rebased four times while this card waited: onto `4cfc15256`,
`e84d30975`, `76b4b4f99` and `9a8208fba`. The last two brought 62 upstream
commits. They include BPR-1212 slice 1, BPR-1328, BPR-1329, BPR-1334, BPR-1336,
BPR-1338, BPR-1339 and BPR-1341, with product changes to
`github_application.cpp`, the fixture-access header, the workbench host and
the large-review benchmark. Git merged the code without conflict. The only
conflict was the round-trip pin; both sides' cards are kept. The tree is the
same macOS arm64 optimized tests-on tree (`dev` preset,
`CMAKE_BUILD_TYPE=Release`), rebuilt at each merged head. The numbers below
are at the final one.

- **Round-trip pin.** Recomputed on the merged tree in `86817d6bc`, with a
  BPR-1327 note over the BPR-1338 follow-up reseal: full application
  `a24ce58c…`, production app manifest `3a7d3cf5…`, mutation pin unchanged.
  `application_review_round_trip_composition_contract` and
  `architecture_boundaries` pass.
- **Focused rows.** `core_comparison_preparation`, `application_diff_host`,
  `application_epoch_host`, `application_workbench_host`,
  `gh_cli_git_smart_transport`, `github_provider`,
  `application_composition_contract` and
  `application_profile_window_review_restoration_public_path`: 8/8 pass.
- **Large-review rows, `-j1`, budgets enforced.** 19/19 pass in 250.7 s,
  with load 8.8 at the start and 8.4 at the end. Every report has
  `BudgetsEnforced` true and empty `InputBudgetFailures`, `KnownInputMisses`
  and `UnenforcedLimitMisses`. The largest pulse interval was 55.5 ms
  (unavailable diff), against the 100 ms limit.
  `application_large_review_prepared_open`: unprepared opens 73–81 ms, the
  first open while its window was prepared 113 ms, and later prepared opens
  18.2–18.8 ms. All 5 of 5 were answered by preparation. At `76b4b4f99`, under
  load 18 to 9, the rows also passed 19/19 (281.5 s, largest pulse interval
  66.9 ms).
- **Earlier run under load, at `e84d30975`.** 18 of 19 passed, with load
  rising from 4.6 to 41. `application_large_review_launch` failed "Mac fixture
  responsiveness budget exceeded", with every phase at about ten times its
  usual cost. A temporary benchmark binary turned preparation off at startup;
  its source change was reverted and the real binary restored byte for byte.
  The launch row then ran interleaved off/on three times at load 92–117. All
  six runs failed the same budget, preparation off as well as on, so the
  failure did not depend on preparation. One trend is recorded:
  `conversations_pick_open` took 249–727 ms with preparation on against
  164–185 ms off, and `large_file_selection` 1,327–4,017 ms against
  956–1,601. With all 14 cores saturated the preparation worker competes with
  the reviewer's open for CPU. The row passes at load 8 and at load 18.
- `verify_documentation.py`: DOCUMENTATION_OK.

### Owed: Windows and Linux

On `bpr-main-win` (MSVC) and `bpr-main-linux` (GCC), at the pushed `main`
commit or later, run the focused rows and the 19 large-review rows on an
optimized tests-on tree with the station otherwise idle. Then remove the tree.

```powershell
git -C C:\code\git\BetterPullReview pull --ff-only
cmake -S C:\code\git\BetterPullReview --preset windows-app-release -B C:\code\git\BetterPullReview\build\bpr1327-release -DBUILD_TESTING=ON -DBPR_DISTRIBUTION_BUILD=OFF -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build C:\code\git\BetterPullReview\build\bpr1327-release
ctest --test-dir C:\code\git\BetterPullReview\build\bpr1327-release -j1 --output-on-failure -R "^(core_comparison_preparation|application_diff_host|application_epoch_host|application_workbench_host|gh_cli_git_smart_transport|github_provider|architecture_boundaries|application_review_round_trip_composition_contract|large_review_fixture_contract|large_review_replay_contract|application_large_review_.*|application_unavailable_diff_public_path|application_approximate_diff_public_path|application_first_tab_comparison)$"
```

```sh
git -C /home/mmarcin/BetterPullReview pull --ff-only
cmake -S /home/mmarcin/BetterPullReview --preset app-release -B /home/mmarcin/BetterPullReview/build/bpr1327-release -DBUILD_TESTING=ON -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build /home/mmarcin/BetterPullReview/build/bpr1327-release
ctest --test-dir /home/mmarcin/BetterPullReview/build/bpr1327-release -j1 --output-on-failure -R '^(core_comparison_preparation|application_diff_host|application_epoch_host|application_workbench_host|gh_cli_git_smart_transport|github_provider|architecture_boundaries|application_review_round_trip_composition_contract|large_review_fixture_contract|large_review_replay_contract|application_large_review_.*|application_unavailable_diff_public_path|application_approximate_diff_public_path|application_first_tab_comparison)$'
```

Expect 27 rows passing. On both stations the large-review reports should
show `BudgetsEnforced` true, empty `InputBudgetFailures` and empty
`UnenforcedLimitMisses`. `application_large_review_prepared_open` should
show `PreparedHits` equal to the prepared opens.

## Owner decision (chat, 2026-10-08)

The question: preparation reads, over the network, the contents of up to 16
files around the cursor that the reviewer may never open: at most 512 KiB a
file, in batches of 16, one GraphQL point and four account reads a batch.

> Question: "Should preparation always fetch file contents … Accept to keep it
> always on; Reject … to add a setting"
>
> Owner's answer: "Always on (Recommended)"

Network preparation stays always on, with no setting. Nothing in the code
changes for the answer.
