---
schema: "implement-rfc.card.v1"
id: "BPR-1565"
title: "Remove the comment editor's typed label codes and their completion"
status: "In Progress"
rfcs: ["RFC-BPR-0048"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"remove-label-shortcodes-r1","governing_rfc":"RFC-BPR-0048","amendment":"RFC-BPR-0048 owner amendment 1 (2026-10-16)","criteria":["AM1-01"],"origin":"owner request in chat, 2026-10-16","reservation":"docs/issues/_reservations/BPR-1565--BPR-1565--55dff8f7650f.json"}
---

# BPR-1565: Remove the comment editor's typed label codes and their completion

The owner, 2026-10-16, verbatim: "I think we can get rid of our custom
shortcodes for the conventional comment tags... they don't add much over just
typing it out yourself and the dropdown picker is convenient enough."
Recorded as [RFC-BPR-0048 owner amendment 1](../../rfcs/RFC-BPR-0048-feedback-authoring-native-github-integration.md#owner-amendment-1-2026-10-16-no-typed-label-codes).

What "custom shortcodes" means here: the leading `:xx:` codes of
[[BPR-1083]] (`:qu:` became `question: ` on its closing colon) and the
completion list a colon at the start of a comment opened, in the composer and
in a reply written in place ([[BPR-1290]]). They were the only typed label
shortcut. The Letters mark style ([[BPR-1083]], the owner's "shortcode
icons") draws each label's two letters and is a display style, not an input
mechanism; it stays. GitHub emoji shortcodes ([[BPR-1544]]) are the Markdown
reader's and are untouched; none of the twelve codes is a gemoji name.

## Acceptance

- **AC01 (AM1-01):** Typing `:qu:`, `:is:`, `:su:`, `:ni:`, `:pr:`, `:zz:` or
  `:+1:` at the start of a comment, or a colon alone, leaves the text as typed,
  with no label, no completion list and no control edit, in the composer, in
  the reply written in place and in the composed application with the label
  helper on (negative control for the removed behaviour).
- **AC02:** The chip, its menu, ⌘L, ⌘0 (Plain in the open menu), the
  decoration toggles and keyboard access to all twelve labels and Plain work
  as before; the menu and the Overview Label key no longer show codes.
- **AC03:** The Label helper preference keeps its stored value and its only
  remaining effect (the Review feedback guidance on workshop requests); its
  Settings sentence no longer mentions `:shortcodes:`.
- **AC04:** UX, settings, workbench and architecture documentation describe
  the change; existing drafts that hold a code keep it as literal text.
