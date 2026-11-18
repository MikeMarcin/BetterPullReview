---
schema: "implement-rfc.card.v1"
id: "BPR-2258"
title: "Draft comment from a report stays on the report"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2258--BPR-2258--c3a513113ed6.json","origin":"owner, 2026-11-18"}
---

# Draft comment from a report stays on the report

## Origin

The owner, 2026-11-18: "Can we make drafting a comment from a report stop
switching to that file? It's disorienting id rather stay on the report". No
look is needed.

Draft comment on a report's finding ([[BPR-2219]]) went through the finding
card's path, `act_on_agent_card`, which opens the finding's file in Review,
chooses its line and opens the line's draft there. From a report that put the
file in front, and took the file tab nearest before the report for it.

## Scope and acceptance

- **AC-a.** Draft comment (and Keep local on a file) from a report makes the
  draft (or the Local comment) at the finding's file and lines while the
  report stays the active tab and on screen; the reviewer's file tabs and
  their files are as they were.
- **AC-b.** The finding then reads Drafted and offers Go to draft, which is
  the only thing that moves to the file.
- **AC-c.** Keyboard focus stays on the report, and a screen reader hears
  that the draft was made (or why not).
- **AC-d.** A composed course asserts the report stays shown, the tabs and
  the active tab do not change, and the draft exists.
