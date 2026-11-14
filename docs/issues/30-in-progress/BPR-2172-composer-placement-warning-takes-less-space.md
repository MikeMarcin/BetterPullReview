---
schema: "implement-rfc.card.v1"
id: "BPR-2172"
title: "The composer's placement warning takes less space"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2172--BPR-2175--e382f3fed4fa.json", "origin": "owner aside, look v007 of RFC-BPR-0127 (attended session efe2da80b42f60b91e24da153dc8ea5b, handed back 2026-11-14T01:50:15Z)"}
---

# BPR-2172: The composer's placement warning takes less space

The owner, on `agent-draft-only` (Accept), with a screenshot of the composer of a draft the agent made ([image](../../verification/RFC-BPR-0127-v007-asides/agent-draft-only.png)):

> Can we make this warning take less space, maybe it's just a warning sign on the row above and expands a disclosure on click or something?

The warning is the composer's notice "GitHub may not place a comment on this line. If it refuses, the comment is posted as a file comment that links to the line.", three lines of words under the label chip and Enhance.

## Outcome

The notice is a warning mark on the composer's tool row (beside the label chip); pressing it (or keyboard activation) discloses the sentence, and it is named for assistive technology. The composer's height does not grow by three lines for it. Owner look on a frozen build with a draft on a line GitHub may not place.


## Progress (2026-11-14, branch `claude/parallel-backlog-2156-2172-2173`)

`draft_composer_presentation::NoticeFolds`: the window folds a target notice
that leaves Publish offered (where the comment goes) to `threadDraftNoticeMark`,
a warning mark beside the label chip; press or Space discloses the sentence
under the row and hides it again; its accessible name is "Placement warning"
and its description the sentence. Publication notices and blocking notices
keep their lines. Checked in `qt_conversation_cards_view`
(`test_placement_notice_is_a_warning_mark`). Every non-blocking target notice
folds, including "Goes on the head at line N": the look asks whether that is
right. Pending: the owner's look.

## Owner look (session `9afc3306d0cf788e0c2eaaec43d5b882`, frozen `200ba998fa15`, handed back 2026-11-14)

- `BPR-2172-placement-mark`: **Reject**. The owner, verbatim: "I can't see what you're saying is there. Next review round show me in screenshots."
- `BPR-2172-what-folds`: **Reject**. The owner, verbatim: "Show me in screenshots."

Next round: offscreen renders of the composer with each notice, mark closed
and open, in the candidates' content; one question per criterion.

## Screenshots for the next look (2026-11-14)

`test_placement_notices_fold_into_the_composer_mark` (`qt_workbench_line_draft`)
drives each note through the window's own placement path and, with
`BPR_NOTICE_RENDER_ROOT`, renders the composer with the mark closed and open:
may not place, can't place, the file-comment reason, head line, base line and
earlier commit fold; "Working out where GitHub puts a comment…" (which stops
Publish) keeps its lines.
