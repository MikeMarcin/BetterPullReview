---
schema: "implement-rfc.card.v1"
id: "BPR-2171"
title: "Discuss reads cleanly on a busy pull request"
status: "In Progress"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0085"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2171--BPR-2171--2de3ab5edceb.json","design":"docs/design/discuss-busy-pr-v1/index.html","origin":"owner-directed redesign, 2026-11-13; option B of the Discuss mockups with the owner's decisions"}
---

# BPR-2171: Discuss reads cleanly on a busy pull request

Amends the Discuss modality of [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) and the closed note of [RFC-BPR-0085](../../rfcs/RFC-BPR-0085-drawer-design-system.md). Accepted design: [discuss-busy-pr-v1](../../design/discuss-busy-pr-v1/index.html), option B with the decisions below.

## The owner's words (2026-11-13, verbatim)

- "Can we update the design of the discussion tab a little. It's just a bit noisy on a busy pr."
- "it's only the collapsed cards with formatting issues, on a full screen window it just looks like a mess"
- Choosing B: "I think I like B although I dunno how you detect bots, and I think treating bot comments differently should be a quick filter at the top not always true. Do we need the ability to mark a user as a bot and store it as a repo setting?"
- Column width: "should be a setting, Centered I think."
- Threads under a review keep their author name and avatar: "yes it needs them".

Decisions recorded with them: bots are detected from GitHub's author type (a GitHub App's `[bot]` login) automatically, and any author can be marked or unmarked as a bot, stored as a per-repository local setting. Bot folding is a quick filter at the top, off by default, beside Hide resolved (which it does not replace). An open card may grow wider than the column for code blocks and suggested changes. The preview names a block with a word (List, Quote, Image, Table, the code's language, Suggestion). Review's Comments and Overview's Comments share the new preview. The "Pull request | File" choice becomes one "Only this file" toggle.

## Acceptance

- AC1 Collapsed preview: a closed note shows its first paragraph as plain prose, Markdown removed (headings, lists, emphasis, links, HTML, tables, images), code fences handled, one quiet word token per kind of non-prose block in order of appearance, an ellipsis only when words were left out, measured by advance. Discuss, Review's Comments and Overview's Comments share it.
- AC2 Readable width: Discuss's list is a centred column (820 by default) whose width is a setting; the header's chips align to the column's edges; reply, new and resolved marks keep fixed slots; an open card holding code or a suggested change may grow wider than the column.
- AC3 Each file is named once over its threads, each thread carries its line number, and threads under a review keep their author name and avatar.
- AC4 Folding: a file's resolved threads fold into a count on its row; runs of bot comments fold to one row when the Fold bots quick filter is on (off by default, beside Hide resolved). Bots: `[bot]` logins automatically, plus a per-author Mark as bot / Not a bot kept per repository on this computer.
- AC5 "Only this file" is one toggle replacing the "Pull request | File" choice.
- AC6 Native renders at 1280x800, 1728x1117 and 2560x1440, light and dark, on `conversations-v1`; one attended look, one Accept/Reject question per item 1 to 5.
