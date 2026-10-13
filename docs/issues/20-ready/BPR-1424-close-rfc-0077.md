---
schema: "implement-rfc.card.v1"
id: "BPR-1424"
title: "Verify and close RFC-BPR-0077"
status: "Ready"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1423"]
metadata: {"milestone":"pr-search","source_id":"DRAFT-0020","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","source_title":"Large-review performance and full Find RFC acceptance","source_milestone":"qualification-closeout","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0077"]
---

# BPR-1424: Verify and close RFC-BPR-0077

RFC base: `aa3d71fe470627e1e78488eb51a9114a788f33bd`.

## Acceptance criteria

- **AC1:** AC01–AC08 retain their complete original meaning and all pass on the exact integrated candidate, with any required attended/native judgment actually supplied.
- **AC2:** Large public-path fixture records warm/cold matching/acquisition, cancellation latency, peak memory, hardware/cache context and committed regression thresholds; navigation remains usable during cold scan and structural no-rescan rules hold.
- **AC3:** Command map, source-search behavior, resource ownership, baseline diff, verification ledger and live tracker agree; architecture and core-only checks plus documentation gates pass.

## Continuation

Import only under `find-and-find-in-files-v1-r1`; implementation and acceptance remain pending.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.

## Whole-RFC acceptance criteria

- **AC01:** Find in either split pane reaches offscreen unchanged text only in
  that pane. Unified Find follows its defined source order without duplicate
  unchanged rows or a fabricated cross-version multiline match.
- **AC02:** The shipped Find bar demonstrates selection seeding, shared match
  options, multiline input, fixed Find in Selection bounds, match counts,
  wrapping and focus restoration as specified. Unicode/line-ending mappings
  select the exact source span; invalid or unsupported regex cannot freeze the
  UI or leave an old result labelled current.
- **AC03:** PR search finds full-file matches on both sides of the active
  comparison in unopened, filtered, excluded, added, removed and renamed
  source. A historical-only path distinguishes correct membership from reuse
  of today's catalogue.
- **AC04:** Real result activation and F4 traversal open the exact file, side,
  span and comparison through the existing selection/reveal path, preserve
  sibling tabs and review state, and follow the defined deduplication and path
  filters.
- **AC05:** Rapid query, tab, epoch and authority changes, followed by reversed
  completion order, cannot publish stale matches or redirect a newer reading
  gesture. Clearing/closing a search quiesces its work; refresh preserves a
  still-valid pinned comparison without searching history.
- **AC06:** Incomplete material, binary/unsupported sides, cancellation,
  transport failure and search limits visibly qualify coverage. Only a complete
  empty corpus reports an unqualified no-match result, and retry uses the same
  exact authority rather than a newer revision.
- **AC07:** The composed action/key paths have unique shortcuts, correct
  modality/editor ownership, accessible match/scope/status descriptions and
  usable native layouts at supported widths. Rendered highlights coexist with
  syntax, selection and thread markers without altering comparison geometry.
- **AC08:** A large-review public-path fixture records warm/cold performance,
  cancellation and bounded memory, and proves the structural execution rules
  under “Efficient execution and ownership.” Source navigation remains usable
  during a cold scan; core-only and architecture-boundary checks pass.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** Find in either split pane reaches offscreen unchanged text only in that pane. Unified Find follows its defined source order without duplicate unchanged rows or a fabricated cross-version multiline match. | Pending implementation and exact-candidate verification | pending | — |
| **AC02:** The shipped Find bar demonstrates selection seeding, shared match options, multiline input, fixed Find in Selection bounds, match counts, wrapping and focus restoration as specified. Unicode/line-ending mappings select the exact source span; invalid or unsupported regex cannot freeze the UI or leave an old result labelled current. | Pending implementation and exact-candidate verification | pending | — |
| **AC03:** PR search finds full-file matches on both sides of the active comparison in unopened, filtered, excluded, added, removed and renamed source. A historical-only path distinguishes correct membership from reuse of today's catalogue. | Pending implementation and exact-candidate verification | pending | — |
| **AC04:** Real result activation and F4 traversal open the exact file, side, span and comparison through the existing selection/reveal path, preserve sibling tabs and review state, and follow the defined deduplication and path filters. | Pending implementation and exact-candidate verification | pending | — |
| **AC05:** Rapid query, tab, epoch and authority changes, followed by reversed completion order, cannot publish stale matches or redirect a newer reading gesture. Clearing/closing a search quiesces its work; refresh preserves a still-valid pinned comparison without searching history. | Pending implementation and exact-candidate verification | pending | — |
| **AC06:** Incomplete material, binary/unsupported sides, cancellation, transport failure and search limits visibly qualify coverage. Only a complete empty corpus reports an unqualified no-match result, and retry uses the same exact authority rather than a newer revision. | Pending implementation and exact-candidate verification | pending | — |
| **AC07:** The composed action/key paths have unique shortcuts, correct modality/editor ownership, accessible match/scope/status descriptions and usable native layouts at supported widths. Rendered highlights coexist with syntax, selection and thread markers without altering comparison geometry. | Pending implementation and exact-candidate verification | pending | — |
| **AC08:** A large-review public-path fixture records warm/cold performance, cancellation and bounded memory, and proves the structural execution rules under “Efficient execution and ownership.” Source navigation remains usable during a cold scan; core-only and architecture-boundary checks pass. | Pending implementation and exact-candidate verification | pending | — |
