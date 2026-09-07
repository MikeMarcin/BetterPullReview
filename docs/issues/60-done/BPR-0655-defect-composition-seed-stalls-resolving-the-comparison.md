---
schema: "implement-rfc.card.v1"
id: "BPR-0655"
title: "Defect composition seed stalls resolving the comparison"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"application_defect_composition_contract passes at the fixed head","outcome":"pass","revision":"703969fd","evidence":{"kind":"observation","detail":"ctest -R '^application_defect_composition_contract$' passed (50.4 s, both fixtures, seed and reopen) on the dev build of the working tree; the manual seed course reports DEFECT_COMPOSITION_EVIDENCE:11111110011 with two anchor dispatches and no stale completion. application_review_round_trip_composition_contract and application_composition_contract pass after the digest repin; check_boundaries, strict tidy on github_application.cpp, clang-format and verify_documentation pass."}},{"criterion":"the cause names which change broke it","outcome":"pass","revision":"703969fd","evidence":{"kind":"freeform","detail":"The probe's awaiting_reverse_right phase (BPR-0230, 53b5580d) re-dispatched the left anchor every 10 ms tick while the selector control blanks the combo during resolving (16c51165); every resolution slower than a tick arrived one generation stale. Not a BPR-0646 change: a build of cfe19c4c, the commit before BPR-0646 began, fails the seed course identically; resolution latency (inspect_commit_object 2-21 ms) crossing the tick interval exposed it. Recorded in the card's notes."}}]
---

# Defect composition seed stalls resolving the comparison

`application_defect_composition_contract` fails at the defect seed process:
the run spends its 40-second budget in the `open` stage with the epoch
status "Resolving the exact comparison… The retained comparison remains
visible. An obsolete review-history result was ignored.", core status 4, no
exact pair resolved, and no epoch failure in `logs/diagnostics.log` (the
pipeline succeeds through `revision_prepare` and then nothing more is
logged until shutdown). The verdict is `presentation=0`, so
`EPOCH_FIXTURE_SEED_FAILED`, `GITHUB_FIXTURE_FAILED:github.com` and
`DEFECT_COMPOSITION_SEED_FAILED` follow.

Reproduced on 2026-09-06 at 3d83daae, the head BPR-0646 left, with the
BPR-0647 working tree stashed, so it belongs to neither card. The selector
is not in the focused set BPR-0646 ran; the last head it is known to pass
at is not recorded.

## Scope

- Find why the seed comparison is never resolved after the history result is
  ignored as obsolete: whether the obsolete result is the only one the seed
  course ever receives, whether a retry is owed by the epoch host or by the
  course, and whether a BPR-0646 change to the window (the centre stack, the
  rails, the strip as the title row) removed a presentation the seed waits
  for.
- Owned paths: `src/app/main.cpp` (the fixture course), `src/app/src/github_application.cpp`
  and the epoch host under `src/application` as the cause directs; the
  contract under `tests/end_to_end`.

## Acceptance

- `application_defect_composition_contract` passes at the fixed head, and
  the cause names which change broke it.

## Notes (2026-09-07)

- **Cause.** The defect probe's `awaiting_reverse_right` phase in
  `src/app/src/github_application.cpp` read the left epoch selector as
  "not applied" and set it again on every 10 ms tick. The selector control
  blanks that combo (index -1) the moment the core starts resolving the
  anchor it dispatched (`epoch_comparison_control::update_selection_controls`
  after the selection delta, from 16c51165), so each tick dispatched a new
  `select_comparison_anchor_intent`, each bump cancelled the previous
  resolution, and every `comparison_resolved_event` arrived one generation
  stale (`epoch_reason_code::stale_completion`, core status `resolving`).
  Traced with a temporary event trace in the epoch host: 377 anchor
  dispatches and 0 accepted resolutions in 15 s; commit verification
  (`inspect_commit_object`) took 2–21 ms per resolution on this machine, so a
  completion never beat the next tick. The retry is from BPR-0230
  (53b5580d); the race predates RFC-BPR-0028.
- **Not a BPR-0646 change.** The seed course fails identically on a build of
  cfe19c4c, the commit before BPR-0646 began (`RUN_BUDGET_EXPIRED ...
  stage=2`, the same epoch status, `DEFECT_COMPOSITION_EVIDENCE:00000000000`).
  What exposed it is resolution latency crossing the tick interval; the
  window changes named in scope removed nothing the seed waits for.
- **Fix.** The phase now waits while `EpochHost->state().Status` is
  `resolving` and only asks a settled core that shows another left anchor
  again. The seed course then dispatches two anchors, sees no stale
  completion, and reports `DEFECT_COMPOSITION_EVIDENCE:11111110011`.
- **Follow-up, not carded.** A person choosing an exact anchor in the
  details region sees the selector empty until the core resolves; the
  region is hidden by default since BPR-0646, so it stays an observation.
