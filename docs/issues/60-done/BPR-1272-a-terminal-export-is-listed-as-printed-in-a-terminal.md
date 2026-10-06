---
schema: "implement-rfc.card.v1"
id: "BPR-1272"
title: "A terminal export is listed as Printed in a terminal, which is the one case never recorded"
status: "Done"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0061"]
depends_on: []
priority: "P3"
metadata: {"pack": "bpr-review-cycles-v1", "found_by": "BPR-1166", "related_issues": ["BPR-1161", "BPR-1167"], "acceptance": ["RC56-07", "RC61-06"]}
acceptance: [{"criterion":"BPR-1272:course","outcome":"pass","revision":"e0d7833ee","evidence":{"kind":"command","detail":"application_review_cycle_two_round_public_path (automated) exports F1 twice with --output - into a pipe and asserts F2's sheet lists exactly one 'F1 · Written to standard output · … · 2 times' and no delivery naming a terminal; with the old words restored it fails listing 'F1 · Printed in a terminal · 2,675 bytes · 2 times'. Two-round, desktop, parity, qt_review_cycle_view_1_0/_2_0, cycle_cli_documentation and architecture_boundaries pass 7/7 on bpr-main-linux."}},{"criterion":"BPR-1272:owner-words","outcome":"pass","revision":"e0d7833ee","evidence":{"kind":"observation","detail":"Owner, in chat via the integrator's question on 2026-10-06, verbatim: \"Accept\" — the words \"Written to standard output\"."}}]
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

## Linux implementation — 2026-10-06 (`bpr-main-linux`)

**Words proposed:** "Written to standard output". This is the shortest
statement of what the observation records: `bpr cycle export --output -` wrote
the exact bytes to standard output, for a pipe or file. A terminal display is
never recorded. It follows the owner's preference for minimal text; "by bpr
cycle export" is left out, because only that command makes the observation.

**Change:** the one string in `delivery_words`
(`src/adapters/qt/src/review_cycle_share_sheet.cpp`). Its comment now says
that this state's words changed and await the owner's look. No row or document
quoted the old words except the historical verification ledger, which is left
as it was. The other observation words are unchanged.

**Course:** `application_review_cycle_two_round_public_path`, in its automated
form, exports F1 twice with `--output -` into a pipe. It now asserts that F2's
share sheet lists exactly one `F1 · Written to standard output · … · 2 times`
and no delivery naming a terminal.

**Control:** with the old words restored the course fails, listing
`F1 · Printed in a terminal · 2,675 bytes · 2 times`. It passes with the new
words.

**Evidence** (`build/dev-tour-cli`, GCC 15.2, offscreen): the two-round,
desktop and parity courses, `qt_review_cycle_view_1_0` and `_2_0`,
`cycle_cli_documentation` and `architecture_boundaries` pass 7/7.
clang-format is clean.

**Open:** the owner's Accept or Reject of the words, recorded verbatim with the
session and event, run from the Mac. One question: *"A `bpr cycle export
--output -` into a pipe or file is listed as 'F1 · Written to standard output ·
2,675 bytes · 2 times'. Accept these words?"*

## Owner look — 2026-10-06

The integrator asked the owner in chat on 2026-10-06 and relayed the answer
verbatim: "Accept" (the words "Written to standard output").
