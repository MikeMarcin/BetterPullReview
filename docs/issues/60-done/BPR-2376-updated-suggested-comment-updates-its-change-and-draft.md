---
schema: "implement-rfc.card.v1"
id: "BPR-2376"
title: "An accepted side-chat update moves the finding's Suggested change and its unchanged draft with the comment"
status: "Done"
rfcs: []
depends_on: ["BPR-2286", "BPR-2331"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2376--BPR-2376--9ef935cb56d6.json","related":["BPR-2322","BPR-2352","BPR-2325","BPR-2370"],"acceptance":["BPR-2376:core","BPR-2376:page","BPR-2376:course"]}
---

# BPR-2376: An accepted side-chat update moves the Suggested change and the draft with the comment

## Report

The owner, 2026-11-24, paraphrased: a report finding with a Suggested comment
and a Suggested change (the old lines in red, the replacement in green) had
already been drafted. Ask follow-up, then Update suggested comment and Use.
The finding then read "Suggested comment, updated:" with the new comment, and
that comment carried a new code suggestion. The Suggested change block, and
the draft that Go to draft opens, still showed the old replacement lines.

## Cause

A follow-up's Use ([[BPR-2286]]) stored only the comment text
(`FollowUpRevisions`), and only two places read it: the report's "Suggested
comment, updated:" box and the next Draft comment. Two other things still
held the old text.

- The report's Suggested change block is the item's `suggestion` block, read
  from the report's Markdown by `core::dress_report`. The page drew it from
  the dressed report as written, so an update never reached it.
- The draft that Draft comment had already made was never revisited. It kept
  the body made from the earlier comment.

## Fix

- Core (`agent_report.hpp`):
  - `comment_suggestion_lines` returns the replacement lines of a comment's
    first fenced block whose info string is `suggestion`.
  - `revise_report` gives each revised finding the Suggested change its
    updated comment makes, and keeps the lines it replaces. A comment with no
    suggestion drops the block. A suggestion the item did not have is added
    and replaces the lines its quoted code holds.
  - `decide_finding_draft_revision` decides what happens to the draft
    (`finding_draft_body` gives the drafted body):
    - `replace` when the draft is still what the earlier comment drafted.
      Whitespace around the body does not count.
    - `current` when the draft already says the update.
    - `edited` when the reviewer changed it.
    - `none` when the draft is no longer being written.
- Recognition: only a fence whose info string is `suggestion` (backticks or
  tildes, lower case, as `core::markdown_code_language_of` reads it) is a
  Suggested change. GitHub posts and applies that fence as one. A plain code
  block (`cpp` and the like) is posted as code, so it is not a change to the
  lines. The report already shows such a fix as written in the comment
  ([[BPR-2352]]). An updated comment whose change is only a plain block shows
  no Suggested change. The Update suggested comment prompt now asks the agent
  to keep a change to the commented lines as a suggestion block.
- Qt: `report_page_html` draws findings from `core::revise_report` with the
  follow-up revisions. A drafted finding whose changed draft was kept says
  "The draft differs from the updated suggested comment" and offers
  **Update draft** (`report_link::update_draft`).
- Application: on Use, the report finding's draft follows the update when
  the core decides `replace`: one revisioned edit, so a draft changed
  meanwhile is refused, not overwritten. On `edited` the draft is left as it
  is and the finding offers Update draft. Update draft writes the updated
  body the same way. The side chat's status says which case happened.

## Acceptance

- **BPR-2376:core** `core_agent_report`
  (`test_revised_comment_moves_the_suggested_change`,
  `test_finding_draft_follows_an_unchanged_draft_only`). Checks the
  suggestion lines (first block, tilde fence, empty suggestion, plain block
  and none). Checks the Suggested change moved, dropped and added, with the
  same finding rows and deterministic output. Checks the draft decision:
  replace, current, edited, never a draft no longer being written, and never
  replace without the earlier comment.
- **BPR-2376:page** `qt_item_side_chat`
  (`test_an_updated_comment_moves_the_suggested_change`). The page shows the
  updated comment's suggestion and not the old one. A comment with no
  suggestion shows none. Update draft and its words appear only for a
  drafted finding marked outdated.
- **BPR-2376:course** `application_follow_up_public_path`, fixture agent and
  no real agent. The review's renderer finding is drafted, then Ask follow-up,
  Update suggested comment and Use with a proposal that carries another
  suggestion. The report's Suggested change shows the new lines and not the
  old ones, the unchanged draft holds the new suggestion, and the status says
  both were updated. Then the reviewer changes the draft and a second update
  is used. The Suggested change moves again, the changed draft is kept, the
  finding says it differs and offers Update draft, and Update draft replaces
  it. Nothing is published, and the anonymized log holds none of the new
  words. Before the fix the course failed at "the finding's Suggested change
  shows the updated comment's lines, not the old replacement". The finding's
  Suggested change code box still held the old replacement lines. A run that
  skipped that check failed at "the unchanged draft follows the updated
  comment", because the draft still held the old comment.
- Evidence revision: c417bc89b (built on main 63d37a212, which has BPR-2374's
  fixture repair and BPR-2370's report side chat workspace).
  `application_follow_up_public_path` (unrelaxed), `core_agent_report`,
  `qt_item_side_chat` and `coding-format-check` passed there.
  `core_agent_follow_up`, `core_boundary_contract` and
  `architecture_boundaries` passed at 9f14cefd0 (main 5aaf92f97).
  `architecture_boundary_contract` passed one rebase before that. The only
  difference since is the ID in comments and the reseal.
- The production app manifest digest in
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`
  is resealed. The digest the contract's own print mode reports matches the
  new pin. The full-application and mutation-branch digests are unchanged,
  because `github_application.cpp` is not touched.
- Docs: `docs/architecture/review-tours.md` (the item side chat paragraph).

## Limits

- A follow-up opened from an Agent tab finding card that no open report
  holds updates the card's comment but not a draft that card made. Review
  findings are the report's ([[BPR-2331]]), so this applies only to finding
  cards of other answers.
- The Outdated offer is kept for the window's session. After a restart the
  changed draft stays as it is, and no offer is shown.
