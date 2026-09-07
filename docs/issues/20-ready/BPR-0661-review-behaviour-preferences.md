---
schema: "implement-rfc.card.v1"
id: "BPR-0661"
title: "Review behaviour preferences on Reviews & queue"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
---

# Review behaviour preferences on Reviews & queue

RFC-BPR-0028's Reviews & queue page holds the modality rules (which
modality a pull request opens in; which one Continue review enters; other
"when X, switch to Y" rules, none on by default), the reading behaviour
(mark a file read when fully seen or only by hand; the default comparison
on return; the comments drawer's first filter) and the saved repositories.
[BPR-0652](../60-done/BPR-0652-settings-pages-accounts-appearance-reviews.md)
put the observer rules there and left these, because no preference
authority holds them: the workbench settings store carries the layout
only, and a control the application does not honour is not a live control.

## Scope

- A durable review-behaviour record in the workbench settings store
  (schema version 4): open modality (Overview, Read, last used), Continue
  review modality (Read, Review), mark-read behaviour, default comparison
  on return, comments drawer scope; validated by the host, published with
  the layout.
- The window honours them: the modality on open and on Continue review,
  the cards' scope, the epoch line's selection on return; the ledger's
  mark-read behaviour through the receipt commands.
- The page's rows as segmented controls over those intents; resets act on
  their own group only. Saved repositories are BPR-0659's.
- Owned paths: `src/application` workbench ports and host, `src/adapters/sqlite`
  workbench settings store, `src/adapters/qt` window and page, focused
  tests and the workbench settings contract.

## Acceptance

- Each preference round-trips through the store and changes the behaviour
  it names in a composed course; the page reverts on refusal.
