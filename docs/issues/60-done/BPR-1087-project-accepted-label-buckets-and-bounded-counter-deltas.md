---
schema: "implement-rfc.card.v1"
id: "BPR-1087"
title: "Project accepted label buckets and bounded counter deltas"
status: "Done"
rfcs: ["RFC-BPR-0049"]
depends_on: ["BPR-1078", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-13","milestone":"overview","acceptance":["R25","R26","R27","R29"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"tests/application/feedback_counters_tests.cpp","detail":"Website-only body edit with the same head: one body reparsed, the rest reused; Changed exactly PR + file + its two folders; the conversation moves buckets, never in both. Resolve and reopen: no body read, the same four scopes change. bpr-main-win windows-dev Debug, application_feedback_counters."}},{"criterion":"AC2","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"src/application/src/feedback_sources.cpp","detail":"One adapter from github_pull_request_snapshot: native threads or REST groups, never both; a thread's file only from its exact path (empty is unanchored); a generated tour index comment is a reference (exact re-render with render_tour_index), tour stops counted. Tests: adapter facts, scope set without a guessed file, index set aside; 400 seeded rounds match the core's full-population tally per scope."}},{"criterion":"AC3","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"src/application/src/feedback_counters.cpp","detail":"Kept annotation set: unchanged bodies are reused (Reparsed 0 on resolve/reopen/unchanged); project(scope, columns, mode) reads a kept tally only, generation and epoch unchanged, no provider in the model. Mutation without kept annotations fails 'only the edited body is read again'."}},{"criterion":"R25","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"tests/application/feedback_counters_tests.cpp","detail":"This card's share: file, folder and PR scopes with exact attribution, unanchored remainder at PR scope only, tour index as reference and stops counted, identity dedupe via one source per conversation; scope set equals the oracle's."}},{"criterion":"R26","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"tests/application/feedback_counters_tests.cpp","detail":"This card's share: edits, resolves, reopens and departures update only the affected scopes; partial and stale coverage marked explicitly (collection_partial; every scope stale at once). Paging-level partial facts are not yet in the read model (limit recorded)."}},{"criterion":"R27","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"src/application/include/bpr/application/feedback_counters.hpp","detail":"This card's share: every kept tally carries one epoch and coverage, and projections for any columns and mode come from the same kept tally, so a display and its drill-down can share one population. Display, sorting and drill-down are BPR-1089's."}},{"criterion":"R29","outcome":"pass","revision":"26c4e59967134674c5ebf061817f041efe96db76","evidence":{"kind":"command","path":"src/application/src/feedback_counters.cpp","detail":"This card's share: cached classification (kept annotations), bounded incremental projection (only changed units' files, folders and PR re-tallied; Retallied 4 for one edit), no per-label or column-change reads. Mutation without folder re-tally fails. Timing evidence is BPR-1094's."}}]
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

**After rebasing onto [[BPR-1086]]** (`e80e1865d`), which made
`core::published_feedback_segment` the one rule for a published body's
segment, including a tour stop's narrative label: the shared adapter now calls
that core function instead of its own copy. The preflight reads it through the
adapter. The merged tree rebuilt with no `warning C` line, and the same
selection, which now has 20 rows with [[BPR-1086]]'s new core row, passes 20
of 20.

## macOS run (coordinator, 2026-10-06)

At 6984585b9 on macOS arm64 Debug, clean worktree: 18/18 of
application_feedback_counters, core_feedback_*, core_review_preflight, the
defect application rows, application_label_authoring_contract,
qt_feedback_label_threads, qt_conversation_cards_view,
application_writing_delivery, the round-trip contract and
architecture_boundaries.
