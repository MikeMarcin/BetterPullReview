---
schema: "implement-rfc.card.v1"
id: "BPR-2326"
title: "Another author's draft waits in Following, not Needs me, until it is ready"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","found_by":"owner request, 2026-11-20: \"We need to move drafts out of needs me by default. Maybe we can stick them into following while they are in draft.\"","reservation":"docs/issues/_reservations/BPR-2326--BPR-2326--666f7b29f2e7.json","related_cards":["BPR-1683","BPR-1780","BPR-2301"],"related_rfcs":["RFC-BPR-0066","RFC-BPR-0098"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2326: Another author's draft waits in Following, not Needs me, until it is ready

## Asked

The owner, 2026-11-20: "We need to move drafts out of needs me by default.
Maybe we can stick them into following while they are in draft."

A pull request that GitHub reports as a draft is not ready for review, yet a
review request on it (or new comments, or commits since my review) puts it
in Needs me beside work that is.

## Acceptance

- AC1 Core: a pull request I did not author, whose GitHub draft status is
  read as a draft, and whose attention would place it in Needs me, is in
  Following instead. Its attention bucket, its reason sentence and its
  notification eligibility are unchanged; only its Home section moves. An
  unknown draft status changes nothing. Ignored still wins.
- AC2 Once GitHub reports it ready for review (the next refresh reads
  `isDraft` false), the same row is back in Needs me with no action by me.
- AC3 My own drafts are unchanged: they stay in My pull requests with the
  Draft next move ([[BPR-1780]], RFC-BPR-0098).
- AC4 Settings › Review gains a Home row, "Others' drafts", with "Following"
  (the default) and "Needs me". "Needs me" restores the previous placement.
  The choice applies to Home at once, in every window of the profile, and
  survives a restart (workbench settings schema step; an older store reads
  as Following).
- AC5 The choice travels in the optional Review section of the exported
  `.bprsettings` file ([[BPR-2301]]) as `others_drafts`
  (`following`, `needs_me`). A file without the member still loads and
  leaves the choice unchanged; an older reader leaves the member out and
  names it.
- AC6 Rows of such drafts carry the existing Draft chip. Tests: core policy
  (draft with a review request in Following; ready in Needs me; the setting
  on in Needs me; authored drafts and ignored rows unchanged), the Qt Home
  composition (the row's section moves live with the setting), the settings
  store and `.bprsettings` round trips; architecture boundaries, format and
  the documentation gate.
