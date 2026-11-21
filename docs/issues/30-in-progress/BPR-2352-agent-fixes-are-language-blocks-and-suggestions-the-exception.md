---
schema: "implement-rfc.card.v1"
id: "BPR-2352"
title: "An agent writes a fix as a block in the file's language, and a suggestion only for a small, exact, obviously correct fix"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"prompt","target":"0.5.0","origin":"owner, 2026-11-21","placeholder_id":"allocate after the 0.5.0 freeze; replace BPR-2352 everywhere on the branch","related_cards":["BPR-2219","BPR-2298","BPR-2323","BPR-2059"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2352: An agent writes a fix as a block in the file's language, and a suggestion only for a small, exact, obviously correct fix

## The owner's words

2026-11-21: "Our code reviews are way over using actual github
```` ```suggestion ```` blocks. Unless something is a clear and obvious fix
we should favor just writing language blocks like ```` ```cpp ```` instead.
Reserve actual github suggestion blocks for very clear and obviously correct
fixes."

## Cause

Every shipped instruction that asked for a proposed change asked for a
suggestion. The shared report rule said "To propose a change, put a fenced
block marked suggestion". The review template said "Write a fix of a few
lines as a fenced block marked suggestion ... Never write a fix in a plain
code block", and its example held a ```` ```suggestion ```` block. The
suggested-comment format and the review skill named no other form.

The app drafted a fix only from a suggestion. When a finding had no comment
of its own, `report_findings` kept the item's paragraphs and a suggestion
block and dropped every other code block. A fix written in the file's
language was therefore lost from the draft.

## Change

- **The instructions.** Every shipped instruction now asks for a proposed
  change as a fenced block in the file's language. It marks the block
  `suggestion` only when the fix is small, exact, self-contained and
  obviously correct, and replaces exactly the named lines in a file the pull
  request changes. A sketch, a fix that reaches other lines or files, one
  that needs another change to build, or one on lines the pull request does
  not change stays a language block. The instructions are:
  - the shared report rule in `report-templates.md`,
    `review-latest-upload.md`, `review-proposed-fix.md` and
    `review-explain-file.md`;
  - the review form and its template in `report-templates.md`;
  - the suggested-comment format, `suggested-comment.md`;
  - the review skill, `packaging/agent-skills/bpr-pr-review/SKILL.md`.

  Versions moved with their text: `report-templates-v3`,
  `review-latest-upload-v3`, `review-proposed-fix-v3`,
  `review-explain-file-v3`, `suggested-comment-v4` and review skill
  version 7.
- **The draft.** When a finding has no comment of its own, `report_findings`
  still leaves out the item's first code block, the quote of the lines at
  fault, which the comment sits on. It now keeps every code block after the
  quote, as written, in a fence longer than any backtick run in it.
- **Unchanged.** Suggest the change (`review-suggest-change.md`) is the
  reviewer's own request for an exact replacement of their lines. It still
  returns one. The editable body of Review this pull request (BPR-2219) says
  nothing about the form of a fix. That form belongs to the always-added
  report format, so an edited prompt cannot lose the rule.

## Nothing depends on a suggestion

These were checked:

- A finding without a suggestion is still a finding in a review. It drafts
  at its place, and the existing `report_findings` tests cover that.
- `decide_agent_suggestion_range` ([[BPR-2298]]) reads only a
  ```` ```suggestion ```` block. A language block keeps the agent's range.
- The report renders a language block as code. Only a suggestion gets the
  Suggested change diff.
- `classify_review_body` marks only a suggestion `requires_line_subject`. A
  language block can therefore go as a file comment when its lines are off
  the diff, as in [[BPR-2323]] D. The new rule asks for no suggestion on
  lines the pull request does not change, which removes the case that
  BPR-2323 D hit.
- The fixture agent's review keeps its one suggestion: clamping the height
  as well as the width is a small, exact, obviously correct fix of exactly
  lines 12 to 13.

## Acceptance

- AC1. Each shipped instruction that asks for a proposed change asks for a
  block in the file's language. It keeps a suggestion for a small, exact,
  self-contained, obviously correct fix of exactly the named lines. No
  instruction still asks for a suggestion for every fix.
- AC2. A review finding whose fix is a language block drafts that block at
  its place. The quote of the lines at fault is left out, and nothing is
  read as a suggested change.
- AC3. Versions moved with their texts, and every test that pins a version
  follows it.
- AC4. `core_agent_report`, `core_review_skill`,
  `application_writing_delivery`, `application_review_skill_host`,
  `application_agent_launch_policy`, `architecture_boundaries`,
  `coding-format-check` and the documentation gate pass.

## Verification

See the branch's commit for the exact results. Run on macOS, dev preset, in
a task build tree.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `application_writing_delivery`: the four report instructions carry the rule and not the old one, the review template shows a language block, and the suggested-comment format carries the rule. `core_review_skill`: the skill carries it, version 7 | see commit |
| AC2 | `core_agent_report`: a review finding with a quote and a `cpp` fix drafts the fix in a four-backtick fence, without the quote, and holds no suggestion block | see commit |
| AC3 | `application_review_skill_host`, `application_agent_launch_policy`, `application_writing_delivery` | see commit |

Not run here (disk and scope), for the landing round: the composition tests
that pin `suggested-comment-v4`, `qt_settings_window`,
`application_tour_cli` and `application_agent_invocation_composition`.
Their pins were updated as text only.
