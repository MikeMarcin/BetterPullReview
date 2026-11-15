---
schema: "implement-rfc.card.v1"
id: "BPR-2192"
title: "Three courses broken since the account-setup merge (75ab1e0fa): Enhance, the tour link and the tour CLI stop"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-2181 second round, 2026-11-15: failing at the lane and at its parent 4bb44f28f in a scratch worktree; coordinator asked for a bisect and a card","related_cards":["BPR-2181","BPR-2051","BPR-2122","BPR-2076","BPR-2013"],"reservation":"docs/issues/_reservations/BPR-2192--BPR-2193--222f766917c9.json"}
---

# BPR-2192: Three courses broken since the account-setup merge (75ab1e0fa): Enhance, the tour link and the tour CLI stop

Three public courses fail on the v0.5.0 prerelease lane, and fail the same
way at `4bb44f28f`, the commit before BPR-2181's second round. They are not
BPR-2181's: none of them runs through the session runtime.

## Bisect

Built in a scratch worktree with `cmake --preset dev` and only each course's
targets (`test_review_tour_composition_public_path`,
`test_scoped_writing_agent`, `test_review_tour_publication_public_path`),
`git bisect run` between good `f51063b5b` (both bisected courses pass there)
and bad `4bb44f28f`. Many commits in the range do not build for these
targets and were skipped, so the bisect ends on a set rather than one
commit. Both bisects end on the same set:

- `75ab1e0fa` Merge main (c5a0f0823) into the lane: account setup in the
  window, in-PR link routing, RFC-BPR-0125
- `d47362061` WIP BPR-2076 (Explain this file composition)
- `153e28d0f` Explain this file in Review (BPR-2076)
- `bb8560026` Finish BPR-2076 (CLI stop bisect only)

The shared point is the merge `75ab1e0fa`, which brought main's account
setup in the window and in-PR link routing together with the lane's tour and
Explain this file work.

## The three failures

- `application_review_tour_link_public_path`: crashes (exit -11). The course
  (added with in-PR link routing in `d1f641ab1`) clicks the tour editor's
  `tourReturn` button (`tests/end_to_end/review_tour_composition_public_path.cpp`,
  "Followed again, the tour now in the window opens at its first stop"),
  which `93b1045a8` (BPR-2013) removed on the other side. Neither commit
  contains the other; they meet in `75ab1e0fa`, and the course dereferences
  a null button.
- `application_review_tour_cli_ordinary_stop`: `FAILED: CLI course consented
  to the chosen account in the account setup, and no read question opened`
  (`tests/end_to_end/cli_publication_course.hpp`). The CLI way's consent no
  longer leads to a read question since account setup moved into the window.
  `application_review_tour_cli_ordinary_native` passes.
- `application_enhance_comment_note_public_path`: `review-tour-composition:fail
  Use puts it in the Local comment to save; nothing is saved until Save`,
  with `Private draft. writing= agent=No agent invocation`: Enhance's
  writing run never starts, so there is nothing to Use.

## Acceptance

- **AC01:** each of the three courses passes, or is changed to the product's
  current behaviour with the reason on this card.
