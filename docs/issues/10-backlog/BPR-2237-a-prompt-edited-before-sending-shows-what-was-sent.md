---
schema: "implement-rfc.card.v1"
id: "BPR-2237"
title: "A prompt edited before sending shows what was sent"
status: "Backlog"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2236--BPR-2238--777d083d72cc.json","origin":"BPR-2219's look review-report-v002, 2026-11-16"}
---

# A prompt edited before sending shows what was sent

## Origin

In the Agent tab a turn sent from a prompt shows the prompt's name, with Show prompt for what was sent ([[BPR-2219]], AC-h). A prompt put in the question field with Shift-click and edited there is still sent under the prompt's name (`agent_tab_view`, where the pending name is kept whenever a prompt is being sent), so the reviewer's own words are behind Show prompt too.

## Scope and acceptance

- **AC-a.** A turn whose text is not the prompt's own expansion shows that text as typed, with the prompt's name above it as before round 2.
- **AC-b.** A turn sent unedited shows the name alone, with Show prompt.
