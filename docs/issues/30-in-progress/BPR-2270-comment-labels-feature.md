---
schema: "implement-rfc.card.v1"
id: "BPR-2270"
title: "Comment labels: an editable Feature, so drafted comments carry Conventional Comments labels again"
status: "In Progress"
rfcs: ["RFC-BPR-0129"]
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2270--BPR-2270--0a26e2f42bfc.json","found_by":"owner, 2026-11-18","related_cards":["BPR-2264","BPR-2267","BPR-2059","BPR-1861","BPR-2258"]}
---

# BPR-2270: Comment labels, an editable Feature, so drafted comments carry Conventional Comments labels again

## Observed

Owner, 2026-11-18: "Make sure when we do the suggested comments fixes that
we're still adding the conventional comment tags. I'm seeing a lot of
comments I draft from a review right now not have any." Refined: "Maybe the
comment tag is another editable feature defaulted to our conventional
comments", and "I don't think we need that safety net, could be a user just
blanks that out because they want all plain comments".

## Acceptance

- AC1. A second editable Feature, `/bpr:comment-labels` ("Comment labels"),
  edited under Settings > Prompts > Features and carried by the settings
  exchange, ships the Conventional Comments convention with BPR-2267's
  decoration rule.
- AC2. The one Feature helper attaches it to every request that writes a
  comment meant to be posted, once however often it is named; an override
  changes what is sent; a blank override sends nothing.
- AC3. The suggested-comment format asks for a label only when Comment
  labels is sent. A drafted comment is the agent's text: its label kept and
  canonicalized, no label added by the app.
- AC4. RFC-BPR-0129 notes the amendment.
- AC5. Focused tests, format and tidy on the changed files, and
  `python3 scripts/verify_documentation.py --root .` pass.
