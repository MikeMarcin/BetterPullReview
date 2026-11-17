---
schema: "implement-rfc.card.v1"
id: "BPR-2237"
title: "A prompt edited before sending shows what was sent"
status: "Done"
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

## Delivered

`core::prompt_sent_unedited` decides whether what was sent is the prompt's
own expansion as it was put in the field (white space at the ends aside).
The Agent tab keeps that on the turn (`Edited`): an edited turn shows its
text as written under the prompt's name and glyph, folded past six lines
behind Show all as a typed question is; an unedited one shows the name
alone with Show prompt. A Shift-click followed by words written before the
prompt's text counts as edited. An edited turn's name is not remembered for
a restart, so after one it reads back as typed: the reviewer's words are
never hidden behind a name.

## Acceptance

| Criterion | Check | Result |
|---|---|---|
| AC-a | `qt_prompt_macro_chips` (AC10 block): Review this pull request Shift-clicked, ", focusing on the cache" added, sent; the turn's text is visible under the name with Show all, and Show all shows the added words; the kept turn says Edited. `core_prompt_macro` (`test_a_prompt_sent_unedited`) | Pass |
| AC-b | `qt_prompt_macro_chips`: the same prompt Shift-clicked and sent unchanged shows its name alone with Show prompt | Pass |

Shown to the owner in the batched look.
