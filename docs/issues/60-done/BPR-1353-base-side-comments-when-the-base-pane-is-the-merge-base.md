---
schema: "implement-rfc.card.v1"
id: "BPR-1353"
title: "Base-side comments when the base pane is the merge base"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"BPR-1335 owner look, session b4b76c70c8e54f1933410b582189d0e8, BPR-1335-base-side-v001 Accept","decision_id":"aa39b2f7-bdc6-4ac0-acb9-66417c610e42","reservation":"docs/issues/_reservations/BPR-1353--BPR-1354--7fb3f9709caa.json","related_cards":["BPR-1335","BPR-1290","BPR-1212"],"wip_branch":"bpr-1335-base-side-wip","wip_commit":"cf85998b9","superseded_by":"BPR-1212","superseded_at":"d785a3f13","acceptance":["BPR-1353:gate","BPR-1353:offer","BPR-1353:pin","BPR-1353:publish","BPR-1353:moved-base","BPR-1353:live"]}
---

# BPR-1353: Base-side comments when the base pane is the merge base

## Owner note

On [[BPR-1335]]'s add-button v001 look the owner asked, verbatim: "And why
can't I add a comment from the left pane? Seems like that would be reasonably
useful, is there a technical reason I can't?"

BPR-1335 answered it (candidate `BPR-1335-base-side-v001`, revision
`answer-b864e319d-v001`): GitHub takes `side: LEFT` comments, but numbers
them in the merge base of the base branch and the head, while BPR's base
pane shows the base branch's accepted tip. When the base branch has moved on
since the pull request branched, a base-pane line number names other text on
GitHub, or nothing. The owner **accepted** going ahead with the follow-up
(session `b4b76c70c8e54f1933410b582189d0e8`, decision
`aa39b2f7-bdc6-4ac0-acb9-66417c610e42`, 2026-10-09T05:08:01Z, no note).

## Superseded by BPR-1212

Closed as a duplicate on 2026-10-09, the day it was filed, on the
coordinator's word. [[BPR-1212]] v002 (branch `bpr-1212-v002`, at
`d785a3f13`) already implements this follow-up: base-side comments behind the
merge-base check, `side: LEFT` publication, and base-side threads pinned to the
left pane. It waits on the owner's review and its 100 ms gates before it
merges, and its acceptance stands in for every criterion below. Nothing here
was built; nothing is to be built from `bpr-1335-base-side-wip`, which stays on
`origin` until the coordinator removes it. The plan is kept as it was filed.

## Plan

1. **The gate.** The host checks, from the local Git copy and without
   fetching, whether the left pane's commit is the merge base of the base
   and the head: the existing `is_ancestor` / `derive_comparison_base` ports
   (`src/application/include/bpr/application/git_cache_port.hpp`,
   `git_process_port.hpp`). When the base pane's commit is an ancestor of the
   head, it is the merge base and its numbering is GitHub's. When the local
   copy cannot answer (object missing, no cache), the answer is "not proven",
   never a fetch.
2. **Only when it is.** That one fact sets the window's
   `LeftConversationEndpointCurrent` (today the composition hard-codes
   `leftHasCurrentDiffRevision = false` in `src/app/src/github_application.cpp`).
   With it, the base pane offers its "+" and `side: LEFT` comments, and
   existing base-side threads are pinned to the left pane, together.
   Without it, both stay off and the base pane says why.
3. **The publish side requires the same check.** The mutation host refuses a
   new base-side comment unless the same merge-base fact holds for the
   revision it publishes against. No file-comment fallback.
4. **Proof.** A fixture whose base has moved past the merge base, and one
   live round on `MikeMarcin/BPR-Test`.

## Starting point

The base-side half is written on the branch `bpr-1335-base-side-wip`
(`cf85998b9`, pushed to `origin`; not built beyond its controls, not for
main as it stands). It is based on `a5cba14fb` and needs rebasing over
BPR-1335 v002 (`b864e319d`), which moved the "+" into its own column. It
holds:

- the core's base-side rule (`decide_comment_range`, `base_outside_hunk`);
- `side: LEFT` in the mutation host and the GitHub contract, with no
  file-comment fallback;
- a version-2 recovery record that keeps each child's side;
- the base pane's own "+";
- tests that fail on main: `core_review_round_trip`,
  `core_review_comment_range`, `application_review_mutation_application_host`,
  `sqlite_review_mutation_recovery_storage` and `github_mutation_contract`.

Still missing: the window's reasons and accessible names, the composition's
merge-base gate, the publish-side check and a composed publish course.

## Acceptance

- `BPR-1353:gate`: the merge-base fact is computed from the local Git copy
  with no fetch; a test shows true for an unmoved base, false for a moved
  base, and "not proven" when the object is absent locally.
- `BPR-1353:offer`: with the fact true, the base pane offers its "+" on a
  line in a base hunk and a range across base lines; with it false, no "+",
  and the base pane says why.
- `BPR-1353:pin`: with the fact true, an existing `side: LEFT` thread is
  pinned to its line in the left pane; with it false, it is not.
- `BPR-1353:publish`: the mutation host refuses a base-side comment when the
  fact does not hold for the published revision, and sends `side: LEFT` with
  the right line (and `start_side` for a range) when it does.
- `BPR-1353:moved-base`: a composed course on a moved-base fixture offers
  nothing on the base pane; on an unmoved one it publishes a base-side
  comment that lands on the right line.
- `BPR-1353:live`: one live round on `MikeMarcin/BPR-Test` posts a base-side
  comment and a base-side range from the app and reads them back on the
  right lines, and closes what it made.
