---
schema: "implement-rfc.card.v1"
id: "BPR-1883"
title: "Refresh conversations before waiting for commit pagination"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1882--BPR-1883--c940b7c58a89.json","related_cards":["BPR-1851"],"milestone_plan":{"id":"responsiveness-latency-v1","path":"docs/rfcs/plans/responsiveness-latency-v1/milestone-plan.json"}}
---

# BPR-1883: Refresh conversations before waiting for commit pagination

The owner explicitly permits decoupling loading on 2026-11-06 so useful content
can finish earlier, provided unfinished work remains clearly marked as loading.

BPR-1851 already publishes a refresh's complete reviews and review threads
before later collections, retaining the accepted snapshot's authority and the
existing partial-refresh indicator. Commit pagination still delays that
observation even though its contents do not depend on commits.

## Acceptance

- AC1: An authenticated refresh with observable review threads reads and
  publishes its complete conversation before commit pagination, without adding
  requests or changing first-open/public-preview read order.
- AC2: Final snapshot validation, cancellation, failures, identity checks and
  same-head-only early publication retain their contracts.
- AC3: The composed refresh displays the conversation earlier, keeps the
  comparison visible and marks remaining loading until the refresh completes.
- AC4: Record focused verification, optimized before/after measurements and
  a one-line journal entry.

## Evidence

The pager reads commits after its complete conversation observation only for
an authenticated refresh with an observer and review-thread support. A single
local commit-reading operation retains the same pagination, limits, count and
final-head validation in either order. First-open, signed-out and unobserved
reads retain their earlier ordering. No additional requests or worker lanes
are introduced.

The observation continues through the existing host/core checks for request,
account, repository, pull request, base and head. It does not accept a
replacement snapshot or confer new epoch or write authority. The existing partial
loading surface now says “Refreshing remaining details…” because commits may
also remain pending. The `frontend-design:frontend-design` skill was loaded;
the existing native layout and pending-state presentation are retained.

Release, Apple M4 Pro arm64, macOS 26.6.2, AppleClang 21.0.0, Qt 6.12.0,
`QT_QPA_PLATFORM=offscreen`, default scale, configured production mimalloc.
The existing composed `refresh_current_file_public_path` course uses synthetic
provider/cache fixtures and inserts 330 ms before each read. Baseline product
source is `6ec53ca3c`; both runs use the same fixture, optimized configuration
and three rounds, with no concurrent task-owned build or test. Times are ms:

| Round | Conversation before | Conversation after | Settled before | Settled after |
|---|---:|---:|---:|---:|
| 1 | 1,997 | 1,676 | 5,987 | 6,005 |
| 2 | 2,002 | 1,673 | 6,000 | 6,029 |
| 3 | 1,999 | 1,670 | 5,990 | 6,018 |

The median conversation arrives 326 ms earlier, approximately one simulated
round trip. Every round makes the same 18 reads, reports zero blank comparison
samples, and shows the partial-loading marker with the fresh conversation.
The full refresh still takes about six seconds; this is an earlier useful
result, not a claim of faster terminal completion or live GitHub timings.
No Debug timing gate was run or claimed.

- `github_paged_read` passes: conversation arrives before any commit response
  on one- and three-page reads; count mismatches and wrong final heads still
  reject the snapshot, and cancellation after early publication stops further
  commit pages. First-open ordering is explicitly retained; the existing
  anonymous public-preview cases also pass. As a negative control, the new
  course fails against the old pager at “the complete conversation is
  observable before any commit page returns.”
- `application_github_window_read_host` passes, including same-head/base-only
  early conversation, retained snapshot identity and unchanged epoch handoff.
  `github_read_contract` and `github_provider` pass.
- The composed `refresh_current_file_public_path` passes with all assertions
  enabled at 330 ms/read. Its full-window offscreen render was inspected: the
  comparison remains visible, the fresh comment count is present, the stage
  reads “Loading commits…”, and the partial marker reads “Refreshing remaining
  details…”. No wash reappears after freshness; the marker ends at completion.
  This is offscreen layout/behavior evidence, not native desktop acceptance.
- `application_refresh_feedback_public_path` passes through the real composition
  root. The earlier BPR-1882 projection/host results remain applicable.
- `architecture_boundaries`, `core_dependency_boundary`, clang-format 22.1.8,
  and clang-tidy 22.1.8 for the pager and both changed test translation units
  pass. The existing inefficient string-concatenation warning in the touched
  fixture was removed without changing its bytes.
- The documentation gate, its 67 regression tests, explicit milestone-plan
  structure check and `git diff --check` pass.

Broader checks retain pre-existing failures. `architecture_boundary_contract`
fails in its GitHub CLI factory fixture; its positive control fails identically
on exported baseline `6ec53ca3c`. `test_network_isolation_manifest` refuses the
existing `application_home_open_feedback_cli_public_path` registration because
it has no `WINDOWS_UNCONTAINED_TESTS` entry. The baseline registration and pin
checker reproduce that exact failure. An initial missing executable diagnostic
for `gh_cli_git_smart_transport` was resolved by building that manifest
prerequisite; the missing pin is the only remaining manifest failure. Neither
failure is represented as passing evidence or modified by this slice.

Whole-file Qt clang-tidy also reports `QueueFilterModel` potentially null in
`update_queue_status` (line 5937) and the `finish_choice` local's naming
(line 13269, previously recorded by BPR-1851). Comparing the whole Qt file with
baseline `6ec53ca3c` confirms that only the loading string differs; both
diagnosed paths are unchanged. No diagnostic names the changed line. This is
recorded as a failed whole-file check, without broadening this latency slice
into unrelated queue/settings changes.

The [journal](../../development/responsiveness-journal.md) records the improvement.
