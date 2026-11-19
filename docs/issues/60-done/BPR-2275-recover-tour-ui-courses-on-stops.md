---
schema: "implement-rfc.card.v1"
id: "BPR-2275"
title: "Re-cover the tour UI courses that ran on the Problem and Solution, on a stop"
status: "Done"
rfcs: []
depends_on: ["BPR-2274"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"test-debt","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2275--BPR-2275--1eb0c0ff7e7b.json","found_by":"BPR-2274, 2026-11-18","mandate_revision":"v0-5-0-prerelease-targets-r1"}
---

# BPR-2275: Re-cover the tour UI courses that ran on the Problem and Solution

## Why

[[BPR-2274]] removed the tour's Problem and Solution. Several courses used
those two fields as the vehicle for behaviour that is not about them: the
side chat's quoting, prompts, Update link, Copy and Resend; the live typing
handoff between the desktop and `bpr tour`; and website reconciliation of
published text. BPR-2274 trimmed what could not be moved in the same change.

## Trimmed

- `qt_tour_side_chat`: the main scenario (sections 1 to 4 and the part chats)
  ran on the Solution's chat; only the clock check remains.
- `application_review_tour_publication_reconciliation`: the course reconciled
  a website edit of the description's brief; removed from the course list
  with `application_review_tour_publication_description`, which tested the
  description write that no longer exists.
- `application_profile_tour_handoff` (`profile_tour_handoff_contract.py`): the
  live typing, caret, input-method and assist checks on the Problem field.
- `application_review_tour_composition_public_path`: the IME, undo and
  restored-editor checks on the Problem and Solution editors.

## Acceptance

- AC1. Each trimmed behaviour that still exists is covered again on a stop's
  text or chat: quoting, prompts, Update, Copy, Resend and Delete in the side
  chat; reconciliation of a website edit to a published stop; typing in a
  stop while `bpr tour` edits it.
- AC2. The restored courses pass on macOS.

## Re-covered

- `qt_tour_side_chat`: the whole scenario runs again, on a second stop's
  chat where it ran on the Solution's: the glyph and its count, the quoted
  passage and the selected-block prompt, the readiness gate, Return, the
  live run line and activity block, retirement, Copy and Copy chat, the
  Update link and its proposal, code colours and boxes, references,
  switching to Stop 1 and the whole tour, Resend and Delete on a failed turn
  (Stop 1), the Draft Tour line, the session foot, Enhance and Ask more, the
  revision card and reference links.
- `application_review_tour_publication_reconciliation` is back in the course
  list. A website edit to a published stop meets a local edit to the same
  stop: one conflict, the local text kept, the website's unrelated raw
  prefix and route order adopted, the conflict surviving a restart, Keep my
  proposed version, a cold profile importing the website's text, and one
  narrow comment edit with the tour comment and the description untouched.
- `application_profile_tour_handoff`: the desktop adds the course's stop
  first; live typing, the caret, input-method preedit (`editor_busy`), a
  stale CLI edit, an independent field (the stop's title) with a duplicate
  request, the shared undo, and assist on a quoted passage with a stale
  answer, all on that stop's text.
- `application_review_tour_composition_public_path`: Tab to the assistance
  control and back, input-method preedit and commit, paste, native undo and
  redo across the asynchronous save, preview, revisit, the restored editor,
  the failure and owner-cleared states and the removed-wire control, on a
  stop's text.

## Verification

macOS, dev preset, after [[BPR-2274]] on main: the four courses above pass,
with all 72 rows matching `tour`, `side_chat` or `handoff`; the build,
`coding-format-check` and `verify_documentation.py` pass.
