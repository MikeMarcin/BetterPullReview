---
schema: "implement-rfc.card.v1"
id: "BPR-2298"
title: "An agent's suggested change is drafted on one line, not on the lines its suggestion replaces"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2298--BPR-2298--6dce476a9a73.json","origin":"owner report 2026-11-19 (\"I think we're getting a bunch of comments from reviews in agents that have suggestion blocks, but the comments themselves are only on one line instead of the entire block that needs to be replaced by the suggestion which makes GitHub's suggestion feature not work properly here.\")","related_cards":["BPR-1212","BPR-1753","BPR-1861","BPR-2219","BPR-2268","BPR-2286"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# An agent's suggested change is drafted on one line, not on the lines its suggestion replaces

## What happened

The owner: "I think we're getting a bunch of comments from reviews in agents
that have suggestion blocks, but the comments themselves are only on one line
instead of the entire block that needs to be replaced by the suggestion which
makes GitHub's suggestion feature not work properly here."

GitHub applies a ```` ```suggestion ```` block by replacing exactly the
comment's lines, `start_line` to `line` on `RIGHT`. A block that rewrites
lines 12 and 13, drafted on line 12 alone, writes line 13 a second time when
applied.

## Where the range was lost

1. **Never requested.** The review skill (`bpr-pr-review`, version 5) asked
   for `startLine`/`endLine` without saying that a suggestion's lines must be
   exactly the lines it replaces, and the report prompts said only "It
   replaces the lines the item names". Agents named the line at fault.
2. **Dropped when drafting.** `act_on_agent_card`
   (`src/app/src/github_application/window_agent_setup.cpp`), the one path
   for a finding card's Draft, the report's Draft comment ([[BPR-2268]]),
   Draft all and a card whose comment a follow-up updated ([[BPR-2286]]),
   anchored the draft at `finding.Lines.Begin` and selected that one line:
   `finding.Lines.End` was read nowhere, so even a correct `startLine..endLine`
   or `path:first-last` became a one-line draft. The draft store and
   publication already carried `StartLine` (`start_line`/`start_side`,
   [[BPR-1212]]); nothing gave a finding's draft one.

## Fix

- **Prompt.** The review skill (now `bpr-version: 6`) says that with a
  suggestion block `startLine` and `endLine` are exactly the head lines the
  block replaces, a kept line included, never only the line at fault; the
  report prompts (`report-templates.md`, `review-latest-upload.md`,
  `review-explain-file.md`, `review-proposed-fix.md`) say the same of the
  item's place (`path:first-last`).
- **Core.** `core::decide_agent_suggestion_range`
  (`src/core/include/bpr/core/agent_suggestion_range.hpp`) reads the
  comment's one suggestion block against the head lines near the agent's
  range: widened when the block repeats verbatim the head lines just before
  or after the range (one of them substantive, so a lone brace or blank line
  never moves it), or rewrites line for line a window of its own length
  around it (each line equal or with the same indentation and leading
  identifier); kept when the block is no longer than the range or keeps all
  its lines and only adds; flagged `unverified` when a longer block is
  explained by nothing. `core::promote_private_text_on_lines` writes the
  text and the draft's first line in one edit.
- **Drafting.** `act_on_agent_card` takes the finding's first to last line,
  widened by the core, and, where `decide_comment_range` takes it (one hunk,
  one side), opens the draft on the last line with its first line: one GitHub
  multi-line comment. Otherwise the first line as before, and the card says
  why.
- **UI.** The card says where it drafted when that needs saying: "Drafted on
  lines 12–13, the lines its suggestion replaces.", or that a flagged
  suggestion should be checked, or that GitHub takes several lines only in
  one changed region. The draft card shows its range ("Your draft on lines
  12–13") as any range draft does.

## Acceptance

- **AC1.** The review skill and the report prompts ask for the exact lines a
  suggestion replaces.
- **AC2.** The core keeps, widens or flags deterministically; applying the
  suggestion on the decided lines gives the file it means, and on the agent's
  one line duplicates a line (the defect).
- **AC3.** A finding naming line 12 whose suggestion rewrites 12 and carries
  13 drafts from its card on lines 12–13 and publishes through the fixture
  GitHub with `start_line` 12, `line` 13, `start_side` and `side` `RIGHT`.
- **AC4.** Applying the published suggestion on the received lines writes no
  line twice.
- **AC5.** The card names the drafted range.

## Evidence

Candidate `9518db7d9` (the fix, the prompts, the tests), macOS arm64, dev
(Debug), rebased on main `d45895493`.

| Criterion | Check | Result |
| --- | --- | --- |
| AC1 | `core_review_skill` (the shipped skill is version 6), `application_review_skill_host`, `application_writing_delivery`, `core_prompt_macro` | Passed |
| AC2 | `core_agent_suggestion_range` (`tests/core/agent_suggestion_range_tests.cpp`): widened after, before and both ways, insertions and a lone closing brace kept, a block no longer than its lines kept, an unexplained longer block `unverified`, lines outside the document, the document window, and `promote_private_text_on_lines` in one edit; each decided range applied as GitHub does gives the intended file, and the agent's one line duplicates `Height` | Passed |
| AC3, AC4, AC5 | `application_agent_suggestion_range_public_path` (`tests/end_to_end/agent_suggestion_range_public_path.cpp`, the suggestions fixture agent's probe on nested-folders-v1): card "Drafted on lines 12–13, the lines its suggestion replaces."; draft anchor 12 with first line 11 (zero-based); fixture GitHub received `line` 13, `start_line` 12, `side` and `start_side` `RIGHT`; the published block applied on 12–13 equals the head with only the width clamped | Passed |
| AC3 (negative) | the same course with only `act_on_agent_card`'s old one-line anchor restored | Failed, as it should: card "Drafted. It is a private draft until you publish it." |
| Regression | `application_agent_keep_action_public_path`, `application_review_report_public_path`, `application_agent_finding_composition_contract`, `qt_agent_finding_cards_retained`, `qt_agent_finding_cards_restart`, `core_revisioned_review_draft`, `architecture_boundaries`, `core_boundary_contract` | Passed |

Format check passed on the touched C++ files; clang-tidy passed on
`agent_suggestion_range.cpp`, `revisioned_review_draft.cpp`,
`window_agent_setup.cpp`, `window_agent_chat.cpp` and the three test files.
`verify_documentation.py` passed. The `core-only` preset was not rebuilt
(disk); the core library and its test are the same sources in `dev`.

Not covered: a follow-up's Update draft (the open draft's field replaced,
[[BPR-2286]]) keeps the draft's range as it is and does not re-read a longer
suggestion it brings; a comment with two or more suggestion blocks is not
checked; a range that crosses changed regions drafts on its first line, as
before, and the card says so.
