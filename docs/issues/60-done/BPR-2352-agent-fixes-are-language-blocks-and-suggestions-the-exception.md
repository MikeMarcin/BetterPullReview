---
schema: "implement-rfc.card.v1"
id: "BPR-2352"
title: "An agent writes a fix as a block in the file's language, and a suggestion only for a small, exact, obviously correct fix"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"prompt","target":"0.5.0","origin":"owner, 2026-11-21","related_cards":["BPR-2219","BPR-2298","BPR-2323","BPR-2059"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4"],"provisional_id":"BPR-XXXX-A","branch":"claude/suggestion-guidance-and-composer","reservation":"docs/issues/_reservations/BPR-2352--BPR-2355--081e6197acbb.json","attended_review":[{"session":"4708604404524750554d23a4282d7890","decision":"rejected","date":"2026-11-21"},{"session":"772f868fd02ed420c8889a2d055f5fab","candidate":"fix in the suggested comment","decision":"accepted","date":"2026-11-22"}]}
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

## Owner review (2026-11-21)

A frozen build of this branch merged with
`claude/context-aware-code-highlighting` was reviewed in session
`4708604404524750554d23a4282d7890`. The review-only branch
`claude/review-composer-and-context` made the fixture's blocking fix a
```` ```cpp ```` block. The verdict was **Reject**: "The code block didn't
appear in the suggested comment."

- **Cause.** The finding had an agent card with its own `suggestedComment`.
  In that path `report_findings` appended only a ```` ```suggestion ````
  block, never a fix in the file's language. The report's "Suggested
  comment" line also showed the card's raw text, escaped as one paragraph.
- **Fix.**
  - When the agent's comment holds no code block of its own,
    `report_findings` appends the item's fix (each code block after the
    quote) to it. Draft comment, Draft all and Go to draft all draft that
    text.
  - The report's "Suggested comment" (and "Suggested comment, updated")
    shows the comment Draft comment drafts. Its words are text, and each
    fenced block is the report's own code box (`comment_html` in
    `report_page.cpp`).
- **Tests.**
  - `core_agent_report`: the card's comment gets the fix, and a comment that
    already holds a code block is kept as written.
  - `qt_agent_report_view`: the suggested comment shows its words and the
    fix in its own box, with no fence as text.
  - `qt_item_side_chat` still passes.

## Acceptance

- AC1. Each shipped instruction that asks for a proposed change asks for a
  block in the file's language. It keeps a suggestion for a small, exact,
  self-contained, obviously correct fix of exactly the named lines. No
  instruction still asks for a suggestion for every fix.
- AC2. A review finding whose fix is a language block drafts that block at
  its place, with or without the agent's own comment. The report's
  Suggested comment shows it as a code box. The quote of the lines at fault
  is left out, and nothing is read as a suggested change.
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

The composition tests that pin `suggested-comment-v4`, `qt_settings_window`,
`application_tour_cli` and `application_agent_invocation_composition` ran in
the landing round below.

## Owner acceptance

- 2026-11-21, session `4708604404524750554d23a4282d7890`: **Reject** ("The
  code block didn't appear in the suggested comment"), fixed as above.
- 2026-11-22, session `772f868fd02ed420c8889a2d055f5fab`: the fix in the
  suggested comment, **Accept**. The build judged drew the suggested
  comment's fix as the report page's own plain code box, without Copy or
  Wrap.

## Landing evidence (main, macOS, Debug)

- Allocated on main `084b8ff88` (`BPR-2352`..`BPR-2355`). The branch
  `claude/suggestion-guidance-and-composer` was rebased onto it and each
  commit rewritten so the provisional `BPR-XXXX-A`, `-B`, `-D` and `-E`
  became `BPR-2352`, `BPR-2353`, `BPR-2354` and `BPR-2355` in the cards,
  code and test comments, docs and commit subjects. The review-only fixture
  commits of `claude/review-composer-2` were not brought.
- Merge with [[BPR-2351]]: a report's suggested comment draws its fenced
  blocks through `code_box_html`, the page's own plain HTML box that the owner
  accepted, never a collected code view. It needs no anchor, because page
  HTML is not coloured. `application_fixture_agent_every_ask` counts the
  report's dressed blocks against its code views, and
  `qt_agent_report_view` now expects the item's two views plus the fix
  shown in the page.
- The production app manifest digest needed no reseal:
  `application_review_round_trip_composition_contract` passed over the
  merged tree as it stood.
- Full dev build in the lane tree succeeded. Passed:
  `application_review_round_trip_composition_contract`,
  `application_agent_invocation_composition_contract` (its review skill pin
  moved to version 7 with this change), `tour_cli_composition`,
  `tour_cli_process_commands` and `skill_pr_tour_package` (in a side tree
  configured with `BPR_BUILD_TOUR_CLI=ON`), `qt_conversation_cards_view`,
  `qt_agent_report_view`, `core_agent_report`, `core_review_skill`,
  `application_writing_delivery`, `application_review_skill_host`,
  `application_agent_launch_policy`, `qt_settings_window`,
  `qt_composed_github_scale_100`, `qt_workbench_window_scale_100`,
  `qt_item_side_chat`, `qt_tour_side_chat`,
  `application_review_report_public_path`,
  `application_follow_up_public_path`,
  `application_fixture_agent_every_ask` and `architecture_boundaries`.
  `coding-format-check` and the documentation gate passed.
