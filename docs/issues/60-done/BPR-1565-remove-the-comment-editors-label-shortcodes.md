---
schema: "implement-rfc.card.v1"
id: "BPR-1565"
title: "Remove the comment editor's typed label codes and their completion"
status: "Done"
rfcs: ["RFC-BPR-0048"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"remove-label-shortcodes-r1","governing_rfc":"RFC-BPR-0048","amendment":"RFC-BPR-0048 owner amendment 1 (2026-10-16)","criteria":["AM1-01"],"origin":"owner request in chat, 2026-10-16","reservation":"docs/issues/_reservations/BPR-1565--BPR-1565--55dff8f7650f.json"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"5f85ee5e5","evidence":{"kind":"command","detail":"qt_feedback_label_controls test_typed_codes_stay_text: a colon alone, then :qu:, :is:, :su:, :ni:, :pr:, :zz: and :+1: typed at the start stay as typed with no label, no list on screen and four ordinary edits each; a code before an existing label and codes elsewhere stay text; no control edit. Negative control: main's controls with the helper defaulted on fail it at 'a colon alone at the start opens nothing'. qt_conversation_cards_view: a reply's :qu: stays as typed and reaches the draft editor as typed. qt_composed_github (inline drawer 100/125/200): the reply's :qu: stays text in place and in the composer's draft. application_label_authoring_contract (composed app, label helper on through the profile preference): LABEL_AUTHORING_EVIDENCE all ones, with CodeStaysLiteral (':qu: <subject>' kept as typed, no label, no control edit) before the chip, Command+L and Publish."}},{"criterion":"AC02","outcome":"pass","revision":"5f85ee5e5","evidence":{"kind":"command","detail":"qt_feedback_label_controls: chip tooltip 'Label (Command+L)'; each menu entry shows mark and name (no code) with its description; chip and menu edit once and undo once; every label and Plain reached by arrow keys; Command+0 is Plain in the open menu and Actual Size outside it; decorations and Plain unchanged; a label chosen while a typed edit is unanswered waits for it and goes on the accepted revision. qt_conversation_cards_view: Command+L opens the reply's menu and suggestion labels it. qt_pull_request_overview_page: the Label key lists every mark by name with no :xx: code. application_label_authoring_contract: chip and Command+L label and publish the comment."}},{"criterion":"AC03","outcome":"pass","revision":"5f85ee5e5","evidence":{"kind":"command","detail":"qt_feedback_label_settings: the helper switch is off by default, dispatches the whole record, follows only the published record in every window, and its info glyph reads 'Sends the Review feedback guidance with each comment workshop request. Off: requests go without it. Labels count either way.' sqlite_contract and application suites pass unchanged (stored value and schema untouched); agent_finding_composition_public_path reads the helper from the window's published behaviour."}},{"criterion":"AC04","outcome":"pass","revision":"5f85ee5e5","evidence":{"kind":"command","detail":"RFC-BPR-0048 owner amendment 1 records the decision, the kept surfaces, existing drafts keeping codes as literal text, and that no code is a gemoji 4.1.0 shortcode (checked against BPR-1544's generated table: none of pr ni su is td qu th ch no ty po qb). docs/ux/conventional-comments-feedback.md, docs/development/settings.md, docs/ux/workbench-specification.md and docs/architecture/review-round-trip.md updated. python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK; tests/scripts/verify_documentation_tests.py runs 67 OK."}}]
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
