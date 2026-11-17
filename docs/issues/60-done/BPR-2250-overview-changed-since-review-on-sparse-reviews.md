---
schema: "implement-rfc.card.v1"
id: "BPR-2250"
title: "The Overview's changed-since-my-review mark is never shown on a sparse review"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2250--BPR-2250--c51365348293.json","origin":"BPR-2247 remaining risk, 2026-11-17","related":["BPR-0654","BPR-2247"],"acceptance":["BPR-2250:sparse","BPR-2250:gone"]}
---

# BPR-2250: The Overview's changed-since-my-review mark is never shown on a sparse review

## Finding

[[BPR-2247]] found that most pull requests load sparsely
(`object_loading_for` in `src/application/src/github_window_read_host.cpp`
answers `selected_objects` within the read limits), so the review's cache holds
only its heads' commits and root trees. What changed since my review now reads
through `resolve_returning_change`, which fetches what the cache lacks.

The Overview's "changed since my last review" mark ([[BPR-0654]]) still reads
through `resolve_changed_paths` (`changed_since_review` in
`src/app/src/github_application/window_read_state.cpp`). That read asks the
cache alone and never fetches: the reviewed head's commit and subtrees are
missing on a sparse review, so the read fails, the failure is not asked again
until an input moves, and the mark stays unshown. A force-pushed reviewed head
is never readable there either.

## Scope

- The mark reads the reviewed head through the review's own GitHub access
  when the cache lacks it, as `resolve_returning_change` does (trees only; by
  SHA when it is outside the head's history).
- When the reviewed commit truly cannot be got, the Overview shows no mark
  and nothing claims the files are unchanged.

## Acceptance

- `BPR-2250:sparse`: on a sparse review whose reviewed head is not among the
  review's objects, the Overview marks exactly the files that changed since
  the review.
- `BPR-2250:gone`: with a reviewed head GitHub no longer serves, no file is
  marked changed or unchanged, and nothing is asked again in a loop.

## Owner rules (coordinator mandate, 2026-11-17)

The current state's load is never delayed by historical fetches; lazy work
is preferred to an eager prefetch; "still loading" may be shown rather than
waiting ([[BPR-2251]]'s owner decision).

## Findings

- The mark asked `resolve_changed_paths`
  (`src/app/src/github_application/window_read_state.cpp:869` at `6aa6883c9`),
  which inspects the reviewed head in the retained cache only
  (`src/application/src/epoch_host_review_reads.cpp:65`) and never fetches.
  On a sparse review that fails, the failure is kept until an input moves
  (`window_read_state.cpp:881`), and the mark stays unknown.
- Found while fixing it: [[BPR-2251]]'s "off the actor" fetch was posted to
  the epoch host's own effect executor
  (`epoch_host_review_reads.cpp:1553`). In the composition that executor is
  the profile's `EpochEffects` (`window_construction.cpp:696`), one worker
  thread, and the profile's cache actors drain on that same executor
  (`src/application/src/profile_epoch_actor_registry.cpp:53`). So in the
  shipped app a held fetch of the past still held every actor read queued
  after it, the current state's included. BPR-2251's host course gave each
  effect a thread of its own, so it did not see this.

## Decision: when the mark is read

Not gated on the Overview being visible: the Overview is where a pull
request opens, so "when shown" is "at once" in practice, and the read is
already bounded so it costs the current state nothing:

- It is admitted only once the current state is ingested (the host refuses
  it during ingestion; the window asks again on the next epoch publish), so
  it never starts ahead of the load.
- It asks paths only: one tree diff, reviewed head to head; no blob is read.
- Its cache phases run at the actor's preparation priority, behind every
  reviewer read.
- Its fetch, when the review lacks the reviewed head, runs on a lane of its
  own, so a slow or failing GitHub holds neither an actor nor the epoch lane.
- Until it answers, every file reads "unknown", never "unchanged"
  (the existing `overview_change_since_review::unknown`).

## Fix

- Host: `returning_change_request::PathsOnly` answers which files changed
  with no blob read and no file bound, at preparation priority.
  `epoch_application_host::set_history_effects` names the executor the
  change since a review's GitHub reads run on.
- Composition: the profile owns `EpochHistoryEffects`, a worker of its own;
  each window's epoch host is given it, and profile shutdown waits for it
  and drains it before `EpochEffects`. What changed since my review and the
  commit asks ([[BPR-2251]], [[BPR-2248]]) now fetch there too.
- Window: `changed_since_review` asks `resolve_returning_change` (one step,
  reviewed head to head, the reviewed head as `Outside` so a force-pushed
  one is fetched by SHA, the pull request's paths, `PathsOnly`). A stale
  answer or a refusal is asked again on the next publish; an unavailable
  commit or any other failure is kept, so nothing is asked in a loop.

## Evidence

Lane commit `f3c9360aa`, 2026-11-17, macOS dev preset; full
`cmake --build --preset dev`.

| Criterion | Check | Result |
|---|---|---|
| `BPR-2250:sparse` | `application_epoch_host` (`a_held_overview_mark_fetch_leaves_the_epoch_lane_free`): a sparse review lacking the reviewed head; the paths-only read fetches its trees by SHA on the history lane, reads no blob, and names exactly the pull request's changed file (`a.cpp`, not `outside.cpp`); `application_returning_review_mark_public_path`: my review recorded on the first head, the Overview marks exactly `src/engine/renderer.cpp` | Pass |
| `BPR-2250:gone` | the host course: a reviewed head the lease cannot fetch is `earlier_unavailable` with no files; the mark course: my review on `0badc0de…`, every file stays unknown and the mark's read count does not move while the Overview republishes | Pass |
| Load not delayed | the host course runs the host's effects on one serial lane, as `EpochEffects` is, and holds the fetch: the reviewer's blob read completes meanwhile (`reviewer-read-done=1 answered=0`). Without `set_history_effects` the same course fails ("the reviewed head's fetch runs on the history lane"). The mark course holds `EpochHistoryEffects` from startup and the pull request loads and a file's diff opens within 10 s | Pass |

Also passed: the `latest`, `addressed`, `gone` returning-review courses,
`application_commit_asks_public_path`, `core_returning_review`,
`architecture_boundaries`, and a 77-test selection over the agent tab,
chat, ask menu, report, overview, epoch and runtime suites at `-j4` (one
`addressed` timeout under that load passed alone, 9.5 s). Format check on
every changed C++ file; clang-tidy on the changed translation units reports
only findings on lines this card did not touch; documentation gate.
`test_network_isolation_manifest` fails on eight rows other lanes
registered without an entry (`application_review_seen_public_path` and
others); `application_returning_review_mark_public_path` has its entry.

## Remaining risks

- The public-path fixture transport fetches no trees, so the composed
  course cannot hold a real fetch; the fetch on the history lane is shown
  by the host course, and the composition's wiring by one call
  (`window_construction.cpp`) the mark course depends on only indirectly.
- `EpochHistoryEffects` is one worker per profile: one window's slow fetch
  of the past delays another window's, and a What changed press waits
  behind a mark's fetch. Neither delays the current state.
- `resolve_commit_filter_touches` still acquires its chain trees on the
  actor ([[BPR-2251]]'s remaining risk).
- `resolve_changed_paths` has no production caller now; its host course
  remains.
- The mark now follows a tree diff: a mode-only change marks the file, and
  a reviewed head whose diff to the head exceeds `maximum_git_changed_entries`
  leaves the files unknown.
