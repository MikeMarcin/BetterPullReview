---
schema: "implement-rfc.card.v1"
id: "BPR-1729"
title: "Publish in-pane suggestions on BPR-Test, measure CRLF, and take the owner's look"
status: "Done"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1728"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M4","live_pull_request":"MikeMarcin/BPR-Test#123","look_session":{"id":"7dba4a1f1dbf08a0ad93da313b35fbcd","state":"/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0090-look-v001","status":"closed 2026-10-22: handed back at event 55, all six accepted, released, handed back and its host stopped"},"wizard_session":{"id":"882aa75a-b47d-49a0-ab01-340f0ca80646","state":"/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001","status":"complete 2026-10-22: commit-batch acknowledged by the owner, measure recorded by the agent, host stopped through the script"},"frozen_build":{"root":"/Users/jaeger/bpr-rfc0090-in-pane-v001.t2govxlb","status":"removed 2026-10-22 with freeze_attended_build.py remove","commit":"b13164f02fa980ec881a55ce2c4fdd0f5a044bc6","executable_sha256":"34a0eb21f6ed5c8962405acca55c83becd4075c6054a6c85cb73e6f5860c51c0"}}
acceptance: [{"criterion": "AC1: RFC AC03 (live), with the CRLF measurement recorded", "outcome": "pass", "revision": "a7b8b0f6d8a745d14c9383b328bee81e582817f4", "evidence": {"kind": "command", "command": "python3 /Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001/course/live_course.py publish, then the owner's Commit suggestions (wizard 882aa75a-b47d-49a0-ab01-340f0ca80646), then live_course.py measure", "detail": "BPR-Test #123: the core's five planned suggestions were accepted at their planned lines and read back identical (body, start_line, line, side, path, commit). The owner committed them as one batch: d3eb2ce6b 'Apply suggestions from code review' on head 2cbe94871. At that head rfc-0090/lf.cpp equals the core's edited file byte for byte (20 lines, no CR), and rfc-0090/crlf.cpp equals it byte for byte with CRLF on all 20 lines, including the suggested line 10: GitHub wrote the block's LF line with the file's CRLF ending, as the core expects. The course then closed #123 and deleted both branches."}}, {"criterion": "AC2: RFC AC10, from the owner", "outcome": "pass", "revision": "a7b8b0f6d8a745d14c9383b328bee81e582817f4", "evidence": {"kind": "observation", "detail": "Attended session 7dba4a1f1dbf08a0ad93da313b35fbcd, frozen build of b13164f02 (on main as a7b8b0f6d, which differs only in main's own files), handed back at event 55: the owner accepted all six questions (entry and continuity 7967e7b0, fixed lines 0becd990, growth and deletion 0dcafafd, drawer link 938ce4e6, held suggestion and Convert 98f535ad, one pane b01228b2). Two notes: dim the read-only code outside the range (a refinement, taken in this card), and a stray highlight in one pane after a selection and Esc outside Suggest change (BPR-1734)."}}]
---

# BPR-1729: Publish in-pane suggestions on BPR-Test, measure CRLF, and take the owner's look

RFC-BPR-0090 M4 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)).

## Scope

- A course on `MikeMarcin/BPR-Test`: the core's planned bodies published as
  the composition decides (a tests-enabled build holds no publication
  grant, as RFC-BPR-0089's M4 did), read back, then GitHub's Commit
  suggestion, for an LF file and a CRLF file. Everything made is cleaned up.
- A frozen build and a fresh short attended session, one question per look
  criterion.

## Acceptance criteria

- AC1: RFC AC03 (live), with the CRLF measurement recorded.
- AC2: RFC AC10, from the owner.

## Live course, 2026-10-21

`test_pull_request_suggestion --live-plans` (`9cf0d41b2`) planned five
suggestions through `decide_suggestion_range` and
`plan_pull_request_suggestion`; a course script posted each through `gh`
as a single review comment with the plan's anchor and body, on
[BPR-Test #123](https://github.com/MikeMarcin/BPR-Test/pull/123) (base
`rfc-0090-base-20261021-182601`, head `rfc-0090-head-20261021-182601` at
`2cbe948712cca3378b140d0b28a57b61d0ca85d3`). Each was read back:

| Suggestion | File | Anchor | Read back |
|---|---|---|---|
| Lines 4–5 to one line, with a label and words | `rfc-0090/lf.cpp` | `start_line` 4, `line` 5, RIGHT | body, lines, sides, path and commit identical |
| Line 7 deleted (empty block) | `rfc-0090/lf.cpp` | `line` 7, RIGHT | identical |
| Line 9 grown to three lines | `rfc-0090/lf.cpp` | `line` 9, RIGHT | identical |
| Line 12 holding three backticks (four-backtick fence) | `rfc-0090/lf.cpp` | `line` 12, RIGHT | identical |
| Line 10 replaced in a CRLF file | `rfc-0090/crlf.cpp` | `line` 10, RIGHT | identical |

GitHub accepted every comment at its planned lines; nothing was moved.
`X-RateLimit-Remaining` stayed above 4960.

The CRLF measurement so far: GitHub's `diff_hunk` for the CRLF file keeps
each line's `\r`; the body the core sent, and the body GitHub returns, hold
none, since a block's lines carry no endings. What GitHub writes into the
CRLF file when the suggestion is committed is measured after the owner's
Commit suggestions (wizard below); the agent cannot press it.

## Parked on the owner

- **Wizard** `882aa75a-b47d-49a0-ab01-340f0ca80646`, state
  `/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001`: on #123's
  Files changed, add the five suggestions to a batch and commit them. Then
  the agent runs the course's `measure`, which compares both files at the
  new head with the core's edited files and counts the CRLF file's endings.
- **Look** `7dba4a1f1dbf08a0ad93da313b35fbcd`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0090-look-v001`,
  six Accept or Reject questions, one per look criterion, at revision
  `b13164f02`, on the frozen build
  `/Users/jaeger/bpr-rfc0090-in-pane-v001.t2govxlb` (launch
  `python3 /Users/jaeger/bpr-rfc0090-in-pane-v001.t2govxlb/launch.py`; its
  own check reached `ATTENDED_FILE_SHOWN:src/engine/renderer.cpp`):
  entry and continuity, the fixed lines' refusal, growth and deletion, the
  drawer link, the held suggestion and Convert (from the composed window's
  render, since the fixture sets no commit filter), and one pane.

## The owner's look, 2026-10-22

All six questions were accepted, in session `7dba4a1f1dbf08a0ad93da313b35fbcd`
(handed back at event 55). Two came with notes, verbatim:

- Drawer link: "I wonder if we can make it more obvious which parts of the
  panes are read only. The rail is nice but maybe we could dim the
  background the rest of the code or something." Taken as a refinement of
  the accepted look: the code outside the range is dimmed while it is
  edited, and the rail stays.
- One pane: "One-pane seems to have an unreleated virtual space
  highlighting problem. The attached screenshot was after i made a
  selection and hit esc outside of the suggest mode." Its own card,
  BPR-1734.

The session was released, handed back again and its host stopped through
the script.

## What GitHub wrote, 2026-10-22

The owner added the five suggestions to a batch and committed them (wizard
step `commit-batch`). One commit, `d3eb2ce6be15a0eef1ab7c0242c25e3c4567a30b`
("Apply suggestions from code review", author MikeMarcin, committer
web-flow), followed `2cbe94871` and changed only the two files. The course's
`measure` read both at that head:

| File | Equals the core's edited file | Lines | Lines ending CRLF |
|---|---|---|---|
| `rfc-0090/lf.cpp` | yes, byte for byte | 20 | 0 |
| `rfc-0090/crlf.cpp` | yes, byte for byte | 20 | 20 |

The CRLF result: the suggested line 10 came back as
`int value_10 = 10; // crlf\r\n`. The block GitHub stored has no `\r`, and
GitHub gave the committed line the file's own CRLF ending, so the file stays
CRLF throughout, which is what the core's edited file holds. The LF-only
variant does not match. `X-RateLimit-Remaining` was 4988 before and 4984
after; the course's `cleanup` then closed #123 and deleted both branches,
and the wizard's host was stopped through its script. The frozen root was
removed with `scripts/freeze_attended_build.py remove`.

## The read-only code set back, 2026-10-22

The drawer-link note, taken as a refinement of the accepted look
(`d2db849ab`): while a suggestion is edited, the lines outside the range and
the space below the last line sit on the code gutter's ground, and in light
and dark their ink fades a quarter of the way toward it, held at the text
floor (`core::read_only_code_ink`, `core::read_only_code_fade_alpha`). The
high-contrast variants keep their ink and take only the ground. The range
keeps the code's ground, its write-blue wash and its rail.
`test_suggestion_dims_the_read_only_code` checks it in split and one pane in
all four variants (the ground above, below and past the range, the range's
own ground, and the ink's floor), and writes the renders the coordinator
was sent; `core_appearance_contrast` checks the fade and the floor. No new
look session was opened: it is a small change to an accepted look, sent to
the coordinator as before and after renders.

## After the owner

Run `measure`, record AC1; read the look's decisions, record AC2; then the
course's `cleanup` closes #123 and deletes both branches. The course script
and its state are kept with the wizard, in
`/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001/course/`. Remove the frozen
root with `scripts/freeze_attended_build.py remove`.

## Rebase

Rebased onto main `7075954df` on 2026-10-21. The frozen build and the look's candidates are `b13164f02`, as built; on this branch that commit is `a7b8b0f6d`, which differs from it only in main's own files. The plan printer is `9cf0d41b2`.
