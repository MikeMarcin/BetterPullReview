---
schema: "implement-rfc.card.v1"
id: "BPR-0655"
title: "Defect composition seed stalls resolving the comparison"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
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
