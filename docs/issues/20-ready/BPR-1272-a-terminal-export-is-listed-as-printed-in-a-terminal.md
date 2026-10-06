---
schema: "implement-rfc.card.v1"
id: "BPR-1272"
title: "A terminal export is listed as Printed in a terminal, which is the one case never recorded"
status: "Ready"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0061"]
depends_on: []
priority: "P3"
metadata: {"pack": "bpr-review-cycles-v1", "found_by": "BPR-1166", "related_issues": ["BPR-1161", "BPR-1167"], "acceptance": ["RC56-07", "RC61-06"]}
---

# BPR-1272: A terminal export is listed as "Printed in a terminal", which is the one case never recorded

## What the two-round course saw

[[BPR-1166]]'s course exports F1 with `bpr cycle export RC-1 --round F1 --output -`
twice, into a pipe the course reads. The share sheet for F2 then lists:

```
F1 · Printed in a terminal · 2,675 bytes · 2 times
```

`docs/development/cycle-cli.md` ("Exports and what is recorded") says the
opposite of those words: `--output -` to a pipe or file writes the exact bytes
and records `retrieved` by `terminal_output`; to a terminal it shows the text
with controls neutralized, which are not the exact bytes, and records nothing.
So the only observation this line can stand for is bytes written to standard
output for some program or file, and the line names the one case that is never
recorded.

The words come from `delivery_text` in
`src/adapters/qt/src/review_cycle_share_sheet.cpp`, whose comment says the
observation words were accepted as `BPR-1161-delivery-words-v001` (session
`cbd48b91…`, event 859) and are kept verbatim. Nothing in the record shows the
owner was shown this particular state, which only a `bpr cycle` export makes.
Because the comment ties the words to an owner acceptance, BPR-1166 did not
change them.

## Outcome

Choose words for `retrieved` by `terminal_output` that say only what happened,
for example "Written to standard output by bpr cycle export". Ask the owner as
one Accept/Reject question with the proposed words, then change the one string
and the rows that assert it (`qt_review_cycle_view_*`, the desktop course and
`docs/development/cycle-cli.md` if it quotes the words). The other observation
words are unchanged.

## Acceptance

- The owner accepts the words, recorded verbatim with session and event.
- A course exports a round with `--output -` into a pipe and the share sheet
  shows the new words; a negative control restoring the old words fails it.
