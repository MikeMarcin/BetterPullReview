---
schema: "implement-rfc.card.v1"
id: "BPR-2262"
title: "Next and Previous comment visit my drafts and leave them their words"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2262--BPR-2262--07158c3df77f.json","origin":"owner, 2026-11-18","related_cards":["BPR-2260","BPR-2158"]}
acceptance: []
---

# Next and Previous comment visit my drafts and leave them their words

## Origin

The owner, 2026-11-18, after BPR-2260: "Probably the same issue but next
comment seems to also break the draft comment", and then: "I expect next
comment and prev comment to visit drafts".

## Scope and acceptance

- **AC-a.** From my draft being written in the drawer, Next and Previous
  comment go on to the comment after or before its line, and the draft
  keeps its note with my name and its words.
- **AC-b.** Next and Previous comment visit my drafts in file and line order
  alongside threads, within a file and across files, both ways, and landing
  on a draft opens it as clicking it does.
- **AC-c.** Every path that moves the drawer's open card off my draft ends
  writing it.
- **AC-d.** Qt tests fail without the fix and pass with it.
