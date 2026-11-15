---
schema: "implement-rfc.card.v1"
id: "BPR-2192"
title: "Three courses broken since the account-setup merge (75ab1e0fa): Enhance, the tour link and the tour CLI stop"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-2181 second round, 2026-11-15: failing at the lane and at its parent 4bb44f28f in a scratch worktree; coordinator asked for a bisect and a card","related_cards":["BPR-2181","BPR-2051","BPR-2122","BPR-2076","BPR-2013"],"reservation":"docs/issues/_reservations/BPR-2192--BPR-2193--222f766917c9.json"}
acceptance: [{"criterion": "AC01", "outcome": "pass", "revision": "b10435495", "evidence": {"kind": "command", "detail": "ctest --preset dev --repeat until-fail:5 at b10435495 (lane db204d4ae plus the fix): application_enhance_comment_note_public_path 5/5, application_review_tour_link_public_path 5/5, application_review_tour_cli_ordinary_stop 5/5. Neighbours once: qt_enhance_control, application_review_tour_composition_public_path, application_review_tour_cold_import_public_path, application_review_submission_cli_ordinary, application_review_tour_cli_ordinary_native pass. Enhance was a product fault (fixed); the tour link and CLI stop courses were stale (changed to the current design), reasons in the Resolution section."}}]
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

## Resolution

- **Enhance on a Local comment (product fault, fixed).** Since BPR-2158
  retired the Notes panel, a Local comment is written in the drawer's
  composer, whose Enhance was built once as `enhance_surface::comment`. A
  Local comment's Enhance therefore told the agent it was "A draft comment or
  reply on the pull request, which its author will read", so the reviewer got
  a rewrite addressed to the author for text nobody else reads. The course saw
  the comment's answer come back for the note. The composer's Enhance binding
  now asks as a note while Local is chosen (and as its own surface again on
  Draft), for both the comment and the reply composer
  (`enhance_binding::ask_as`, set in `open_thread_card::choose_local`). The
  button and panel keep their words. `qt_enhance_control` covers the switch.
- **Tour link (course stale).** The course clicked `tourReturn` (Show in
  code), which BPR-2013 removed; it now uses the stop's file-and-lines link
  (`tour_editor_action::navigate_stop`), as the other tour courses do since
  BPR-2013. The button was not brought back.
- **CLI ordinary stop (course stale).** Restarted after the account was
  consented to in the setup, the application restores that account by itself
  (establishing, then ready) and asks nothing. The course opened the account
  setup again, which replaced its driver with one that never pressed Connect,
  so its count of consents read zero. It now leaves a restoring session alone
  and keeps the first consent; the check that no read question opened is
  unchanged.
