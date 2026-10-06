---
schema: "implement-rfc.card.v1"
id: "BPR-1087"
title: "Project accepted label buckets and bounded counter deltas"
status: "In Progress"
rfcs: ["RFC-BPR-0049"]
depends_on: ["BPR-1078", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-13","milestone":"overview","acceptance":["R25","R26","R27","R29"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1087: Project accepted label buckets and bounded counter deltas

Pack alias: **PACK-CC-13** (live `BPR-1087`, allocated at import).

## Outcome and work

Adapt the current accepted read model into normalized core inputs, using native resolution observations and REST unknown states. Cache annotations by exact body identity/revision. Publish coherent per-conversation/file/ancestor deltas and explicit stale/partial coverage.

## Acceptance

- Website-only body edits and resolve/reopen changes update the correct population without requiring a new head.
- No duplicate native/REST/tour-link entries or guessed file mappings appear.
- Unchanged bodies are not reparsed and changing columns performs no provider requests.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Built and verified on Windows (bpr-main-win, 2026-10-06)

This card owns the projection only: the application-layer adapter, the kept
annotations and the per-scope deltas. Column settings are [[BPR-1088]]'s.
Rendering, through `feedback_mark_theme`, plus sorting and drill-down (R27's
display share) are [[BPR-1089]]'s. Nothing here draws, so there is no owner
look for this card.

**What was built** (`src/application`):

- **The shared feedback adapter**: `feedback_sources.hpp/.cpp`,
  `observe_feedback_sources`. It reads the accepted read model
  (`github_pull_request_snapshot`), never the presentation projection, whose
  plain `Resolved` cannot say "unknown".
  - Native threads carry their own resolution. REST groups are `unknown`.
    Top-level comments and review summaries are `not_applicable`. My own
    pending review is `pending_publication`.
  - A thread's file comes only from its exact path, and an empty path is
    unanchored.
  - Native threads and REST groups are one observation each, never both.
  - A top-level comment that is exactly the generated tour index is marked as
    a reference: rendering its one link list again with `render_tour_index`
    gives back the whole body. Any words of a person's around the list make
    it an ordinary comment. Tour stops stay counted as themselves.
  - `observe_review_preflight` now reads through this adapter, unchanged in
    behaviour, so the two cannot drift.
- **The counter model**: `feedback_counters.hpp/.cpp`, `feedback_counter_model`.
  - It keeps the annotation set between observations, so
    `refresh_feedback_annotations` reuses every unchanged body.
  - It keeps one core tally per scope: the pull request, every anchored file
    and every folder above one. File and folder tallies are taken from the
    units in that scope, plus any unit with an unknown anchor, so the core
    reads them as it reads the whole population.
  - A new snapshot is compared with the kept population unit by unit. Only the
    pull request, the changed units' files and those files' folders are tallied
    again. A new pull request, a new coverage or an unknown anchor tallies
    everything.
  - The update names the changed and the removed scopes, the bodies reparsed
    and reused, the scopes re-tallied and the tour index references. Every kept
    tally is restamped with the current epoch and coverage, so counts read
    together are one snapshot.
  - `project(scope, columns, mode)` applies `project_feedback_counters` to a
    kept tally only, so changing columns or mode reads no conversation and asks
    no provider.
  - `observe(…, stale)` marks a kept snapshot stale. A collection that was not
    read is partial.
- **Composition**: the model is not composed into `github_application` here.
  [[BPR-1089]] composes it where it draws, so no per-snapshot work runs before
  anything reads it, and the round-trip pins do not move.

**Tests**: `tests/application/feedback_counters_tests.cpp`,
`application_feedback_counters`, 0.7 s.

- The adapter's facts: native, unanchored, not applicable, pending, and only
  the generated index treated as a reference.
- Buckets per pull request, file and folder. The unanchored question has no
  file.
- A website-only edit with the same head: one body reparsed, the rest reused.
  Changed is exactly the pull request, the file and its two folders; 4 scopes
  re-tallied. The conversation moves buckets, never counted in both.
- Resolve, then reopen: no body read, and the same four scopes change. An
  unchanged snapshot changes nothing and advances no generation.
- A thread leaving removes its now-empty file and folder.
- Columns and mode: a narrow and a wide column set over one kept tally give
  the same epoch and total. Generation and epoch are unchanged. A duplicate
  column is refused.
- Coverage: a REST group is `unknown`, and its unresolved count carries
  `resolution_unknown`. No inline collection means `collection_partial`. A
  kept snapshot marks every scope stale at once, reading no body.
- Another pull request: every body is read and every scope re-tallied.
- 400 seeded random rounds (seed 1087) of edits, resolutions, path moves,
  arrivals and departures. After each, every kept tally equals the core's own
  full-population tally for its scope, and Changed and Removed equal exactly
  the scopes whose oracle counts changed or vanished.

**Failing-before controls** (mutations of `feedback_counters.cpp`, each run
and then reverted):

| Mutation | Fails at |
|---|---|
| No kept annotations | "only the edited body is read again; every other is reused" |
| No folder re-tally | "the edit changes its file, that file's folders and the pull request, and nothing else is tallied" |
| Tour index counted | "the first snapshot is admitted, with the index set aside" |

A fourth was found while building it: comparing tallies without their pull
request key reported nothing changed on a different pull request. That is now
pinned by "another pull request reads every body and tallies every scope
afresh".

**Regressions** (`build/windows-dev` Debug, whole tree, MSVC 14.51, no
`warning C` line): 19 of 19 with `-j4`.

- `application_feedback_counters`;
- the preflight and review-submission rows that now read the shared adapter:
  `core_review_preflight`, `core_review_submission` and
  `application_review_submission_*`, the composition rows taking 41–44 s;
- `core_feedback_*`, including `core_feedback_partition`;
- `application_label_authoring_contract`,
  `application_review_round_trip_composition_contract` and
  `test_network_isolation_manifest`;
- `architecture_boundaries` and `architecture_boundary_contract` (427.2 s).

clang-format 22.1.3 is clean on the new and changed files.

**Limits.**

- The read model's coverage is only available or unavailable. Page
  truncation or a rate-limit cut is not yet a separate fact, so "partial"
  means the inline collection was not read. "Stale" is the caller's to say.
- No code yet produces an `unknown` anchor, for a lost anchor or an uncertain
  rename. The model handles one by re-tallying everything.
- Timing evidence is [[BPR-1094]]'s.
- macOS and Linux runs of `application_feedback_counters` are owed. The code is
  portable application code.
