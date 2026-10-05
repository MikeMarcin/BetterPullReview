---
schema: "implement-rfc.card.v1"
id: "BPR-1081"
title: "Record the selected UX and icon delivery contract"
status: "Done"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1079", "BPR-1080"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-07","milestone":"design","acceptance":["R30","R31","R32","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1081:decisions","outcome":"pass","revision":"70ce0a9fe","evidence":{"kind":"artifact","path":"docs/ux/conventional-comments-feedback.md","detail":"Sources and authority: five design files bound by revision and SHA-256 (verified by script at revision and HEAD), eight owner sessions named, every rule marked owner, existing or derived; open items (Comment menu labels, keyboard route to one Overview chip, live interoperability, headless parity) listed as open with an owning card. RFC-BPR-0050 links it as its design record."}},{"criterion":"BPR-1081:surfaces","outcome":"pass","revision":"70ce0a9fe","evidence":{"kind":"artifact","path":"docs/ux/conventional-comments-feedback.md","detail":"Composition, thread cards and detail, Overview Comments column, preflight and Settings each name their consuming classes (checked to exist) and specify textual fallback (accessible names, tooltips, Label key, Copy source), focus and keyboard (chip tab stop, Cmd-L via ShortcutOverride against homeFocusBarAction, Cmd-0 menu-local), overflow (+n fold, fixed column, narrow windows) and unknown states (four resolution marks, ? chip for unknown/not resolvable/unclassified, non-Exact projections never blank)."}},{"criterion":"BPR-1081:inventory","outcome":"pass","revision":"70ce0a9fe","evidence":{"kind":"artifact","path":"docs/ux/conventional-comments-feedback.md","detail":"Twelve labels with code and glyph (codes and names script-checked against frames v2 MONO and frames v4 OPTIONS); glyph data bound to BPR-1080-label-icons-selected.html at 759ecc692 (= reviewed GLYPHS at 44d09faa3); intended path one Qt unit feedback_label_marks.{hpp,cpp} in static better_pull_review_adapter_qt linked into src/app, no separate packaging path, asset, Qt resource, Qt SVG or dependencies.json entry; tint roles feedback_label_<id> (47 to 59 vocabulary, 45 to 57 offered). Static record; R31-R33 runtime evidence belongs to the building cards."}}]
---

# BPR-1081: Record the selected UX and icon delivery contract

Pack alias: **PACK-CC-07** (live `BPR-1081`, allocated at import).

## Outcome and work

Select and record the layouts, counter-state treatment, label identification, icon resources and required interaction cases in a live RFC-linked design record. Use an existing human gate only where actually required. Preserve partial verdicts; park only dependent work while independent semantics continue.

## Acceptance

- The selected decisions and authority are explicit, with open items not disguised as acceptance.
- Implementation surfaces, textual fallback, focus/overflow and unknown-state treatment are specified.
- The complete icon inventory is bound to the selected source revision and intended packaging path.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Delivery contract (2026-10-05)

The live design record is
[`docs/ux/conventional-comments-feedback.md`](../../ux/conventional-comments-feedback.md).
RFC-BPR-0050 links it from a new Design record section. It binds the owner's
selections to the code that builds them, for BPR-1082 to BPR-1089:

- **Sources.** It names the five design files it binds, each by revision and
  SHA-256, and the eight attended sessions behind them. Every rule is marked
  with its authority:
  - **owner**: a verdict in one of those sessions;
  - **existing**: a contract already in force, such as RFC-BPR-0048, RFC-BPR-0049
    or the Qt adapter rules;
  - **derived**: written down by this card to fill a gap in the selection.
    The building card may revise a derived rule if it records the change.
- **Vocabulary.** It covers:
  - the twelve labels, with their codes and glyphs;
  - the mark, soft and blocking;
  - which bodies get a mark: only a `recognized` prefix;
  - when the fill applies: only `Expectation == blocking`;
  - accessible names and tooltips;
  - the tints as twelve appearance roles. The core vocabulary grows from 47
    roles to 59, with stable names `feedback_label_<id>`. The offered count
    grows from 45 to 57, which RFC-BPR-0034's C5 must record when the roles
    land.
- **Icon delivery.** The inventory is the BPR-1080 selected page at
  `759ecc692`, which is the reviewed `GLYPHS` table at `44d09faa3`. It is
  intended for one Qt unit, `feedback_label_marks.{hpp,cpp}`, in the static
  `better_pull_review_adapter_qt`, which `src/app` links into the executable.
  There is no separate packaging path or asset, no Qt resource, no Qt SVG and
  no `dependencies.json` entry. A fidelity test is owed by the building card.
- **Surfaces.** For each surface it names the consuming classes, the selected
  behaviour, the textual fallback, focus and keyboard, overflow and narrow
  windows, and unknown states:
  - composition: chip, ⌘L menu, `:shortcodes:` and the helper;
  - thread cards and detail: mark under the avatar, rail, reading
    substitution, Copy source;
  - the Overview Comments column: chips, Other, "+n", Open/All, the ? chip,
    Not on a file, the Label key and drill-down;
  - preflight;
  - Settings.
- **Found while binding.**
  - ⌘L is already Home's "Go to the Open bar" (`homeFocusBarAction`, window
    context). The contract has the editor claim ⌘L through `ShortcutOverride`,
    so neither shortcut becomes ambiguous.
  - The menu's ⌘0 for Plain is the window's Home shortcut, so it is a key of
    the open menu only.
- **Open, and recorded as open.**
  - Whether Comment ▾ on the review bar lists labels. The owner selected the
    chip, so BPR-1083 asks the owner if it proposes the menu.
  - The keyboard route to a single Overview chip. The frames show only the
    pointer, so BPR-1089 records the route.
  - Live website interoperability, which uses `MikeMarcin/BPR-Test`.
  - Headless parity (BPR-1092).

No owner sign-off was asked for. The contract adds no visual or interaction
choice to the owner's selection. Its derived rules are engineering bindings or
straight readings of the selection, and are labelled as such. RFC-BPR-0050
asks for no new approval ritual. Any change to an owner rule needs a new
verdict.

**Verification.**

- A scratch script, `check_contract.py`, made 31 checks, all passing:
  - The five sources' SHA-256 matched at their revisions and at HEAD.
  - The twelve codes matched frames v2's `MONO` table, and the twelve glyph
    names matched frames v4's `OPTIONS`.
  - The Ctrl+L and Ctrl+0 bindings, the 47-role vocabulary and RFC-BPR-0034's
    45 were found in source.
  - The static adapter, the app's link to it and the absence of Qt SVG were
    confirmed.
  - The `feedback_label_marks` unit does not exist yet.
  - Every consuming class, the core symbols the contract cites
    (`decide_feedback_header_edit`, `project_feedback_counters`, `Exact`,
    `ScopeUnclassified`, `Unattributed`, the unresolved default and
    `feedback.label.typo`) and the Settings frame strings were found.
- `python3 scripts/verify_documentation.py --root .` printed
  `DOCUMENTATION_OK`.
- `python3 tests/scripts/verify_documentation_tests.py` passed 64 tests.

Limitations. This card writes the contract only. R31 to R33 are earned by the
cards that build the surfaces, with native evidence (R33). No product code
changed.
