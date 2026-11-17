---
schema: "implement-rfc.card.v1"
id: "BPR-2236"
title: "The Agent tab and the report colour the same things alike"
status: "Backlog"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2236--BPR-2238--777d083d72cc.json","origin":"BPR-2219's look review-report-v002, 2026-11-16"}
---

# The Agent tab and the report colour the same things alike

## Origin

The owner, 2026-11-16, on look review-report-v002: "it looks like we have colors in the agent tab and the report which correspond to the same elements but use different colors." And: "I think we could also tone major down a tiny bit in the process." The report page takes its three verdict colours from the theme's report roles ([[BPR-2219]]); the Agent tab's finding cards, severity and label marks, verdict text, suggested-comment blocks and report row take theirs elsewhere. Since round 2 of [[BPR-2219]] the page no longer reads the report paper, ink and code wash roles, which Settings › Appearance › Reports still lists. Not started: [[BPR-2219]]'s worker ran out of room for it.

## Scope and acceptance

- **AC-a.** Every coloured element that appears in both the Agent tab and the report page (a severity or label mark such as Major or issue (blocking), a verdict, a finding's rule or border, the counts, a suggested change) takes its colour from one shared role, decided in core, not from a constant of its surface.
- **AC-b.** Settings › Appearance lists only roles that are read, and a change to one shows on both surfaces. The report paper, ink and code wash roles are removed.
- **AC-c.** Major is a small step less intense than today, still plainly apart from Minor and readable in light and dark.
- **AC-d.** A side-by-side render of the matching elements, light and dark, with Major before and after, goes to the owner with one Accept or Reject question.
