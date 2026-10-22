---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH4"
title: "Publish in-pane suggestions on BPR-Test, measure CRLF, and take the owner's look"
status: "Parked"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-PENDING-AH3"]
priority: "P2"
platforms: ["mac","win","linux"]
parked_on: "human-review"
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M4","live_pull_request":"MikeMarcin/BPR-Test#123","look_session":{"id":"7dba4a1f1dbf08a0ad93da313b35fbcd","state":"/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0090-look-v001","status":"six candidates ready at event cursor 18, host serving"},"wizard_session":{"id":"882aa75a-b47d-49a0-ab01-340f0ca80646","state":"/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001","status":"active, waiting on step commit-batch"},"frozen_build":{"root":"/Users/jaeger/bpr-rfc0090-in-pane-v001.t2govxlb","commit":"b13164f02fa980ec881a55ce2c4fdd0f5a044bc6","executable_sha256":"34a0eb21f6ed5c8962405acca55c83becd4075c6054a6c85cb73e6f5860c51c0"}}
---

# BPR-PENDING-AH4: Publish in-pane suggestions on BPR-Test, measure CRLF, and take the owner's look

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

## After the owner

Run `measure`, record AC1; read the look's decisions, record AC2; then the
course's `cleanup` closes #123 and deletes both branches. The course script
and its state are kept with the wizard, in
`/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001/course/`. Remove the frozen
root with `scripts/freeze_attended_build.py remove`.

## Rebase

Rebased onto main `7075954df` on 2026-10-21. The frozen build and the look's candidates are `b13164f02`, as built; on this branch that commit is `a7b8b0f6d`, which differs from it only in main's own files. The plan printer is `9cf0d41b2`.
