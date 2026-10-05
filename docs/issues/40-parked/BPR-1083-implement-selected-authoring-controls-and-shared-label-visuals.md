---
schema: "implement-rfc.card.v1"
id: "BPR-1083"
title: "Implement selected authoring controls and shared label visuals"
status: "Parked"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0050"]
depends_on: ["BPR-1081", "BPR-1082"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-09","milestone":"authoring","acceptance":["R09","R10","R31","R32","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "7513758b0", "evidence": {"kind": "command", "path": "docs/verification/RFC-BPR-0047-0051.md", "command": "ctest --test-dir build/dev -R 'qt_feedback_label_controls|qt_review_draft_editor'", "detail": "In the composed review_draft_editor with its revisioned dispatch: the chip/menu and toggles leave 'suggestion (blocking): ...' with the same dispatched body as the header typed by hand, and recognize_feedback_prefix gives the same label, expectation and subject span for both; a chip choice is one whole-body edit equal to plan_feedback_draft_edit's command and one undo step. macOS arm64 Debug."}}, {"criterion": "AC2", "outcome": "pass", "revision": "7513758b0", "evidence": {"kind": "command", "path": "docs/verification/RFC-BPR-0047-0051.md", "command": "ctest --test-dir build/dev -R 'qt_feedback_label_marks|qt_feedback_label_threads'", "detail": "One Qt unit, feedback_label_marks, holds the twelve glyphs as compiled QPainterPath construction; all 29 elements equal tests/fixtures/feedback/bpr-1080-selected-glyphs.txt, which equals the selected page at 759ecc692. Every surface (chip, menu, completion, cards, post list, reader) draws through it. Names: menu buttons by label with description and code; marks 'issue (blocking)'; posts 'tbutler, issue (blocking)' and the model's accessible text."}}, {"criterion": "AC3", "outcome": "pass", "revision": "7513758b0", "evidence": {"kind": "command", "path": "docs/verification/RFC-BPR-0047-0051.md", "command": "ctest --test-dir build/dev -R 'qt_feedback_label_controls|qt_feedback_label_threads|qt_review_draft_editor'", "detail": "The controls emit only edit commands through the editor's typed path; refusals change nothing and dispatch nothing; the menu and completion are popups outside the editor's layout and add no scroll area to it; the rail adds no width (every post keeps its text edge); the reader shows one body, from the subject, in the existing reader; no publish, resolve or review decision is reachable from a label."}}]
parked_on: "human-review"
---

# BPR-1083: Implement selected authoring controls and shared label visuals

Pack alias: **PACK-CC-09** (live `BPR-1083`, allocated at import).

## Outcome and work

Implement the selected optional controls, public/root/reply label badges and shared icon resource lookup in current native editors/cards/detail. All twelve and Plain are reachable. Preserve raw body source, normal undo and inherited multi-caret/virtual-space behavior; avoid reformatting incomplete typing.

## Acceptance

- Controls and manually typed syntax show the same draft and projected intent.
- All twelve icons use one resource vocabulary and have accessible identity.
- Labels do not create clipped duplicate composers, new scroll traps or publish effects.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Owner amendment, 2026-10-05

The owner, in chat, relayed by the coordinator, verbatim: "i think settings
should also have a toggle to switch between the icons and the shortcode icons,
because i liked both of them." Built as Reviews & queue › Comment labels ›
Label marks, Icons (the default) or Letters, and recorded in the
[delivery contract](../../ux/conventional-comments-feedback.md).

## Implementation evidence, 2026-10-05

Product commits `b43cd74a5` (twelve "Label …" tint roles, per-theme defaults,
Appearance › Colours 57; RFC-BPR-0034 C5 amended), `37927cd14`
(`feedback_label_marks`), `6ef0c5272` (chip, menu, toggles, ⌘L/⌘0, shortcodes)
and `7513758b0` (thread marks and rails, post list, reader, the Label marks
setting, the `conventional-comments-v1` fixture). The
[execution ledger](../../verification/RFC-BPR-0047-0051.md) records the
decisions, the checks, the negative controls and the unrelated failures.

- **AC1–AC3** pass on the automated evidence in the front matter.
- **Header-only chip** (BPR-1082's open point): the chip shows the label of
  `issue: ` and says it is waiting for its subject.
- **Checks.** `dev` whole tree; the feedback, editor, appearance, settings,
  storage, fixture-security and round-trip rows pass; `core-only` 237 of 238.
  clang-format 22 and 23 agree; strict tidy clean on the changed units.

## Pending owner decisions

One attended session asks, one Accept or Reject each, on a frozen build of
`7513758b0` (`/Users/jaeger/bpr1083-authoring-review-v001.klH6zi`, launched by
`python3 /Users/jaeger/bpr1083-authoring-review-v001.klH6zi/launch.py`):

1. `BPR-1083-comment-menu-v001`: keep the review bar's Comment ▾ free of labels
   (the contract's open question).
2. `BPR-1083-chip-menu-v001`: the chip, its menu and the toggles.
3. `BPR-1083-shortcodes-v001`: the leading shortcodes.
4. `BPR-1083-thread-marks-v001`: thread marks and rails.
5. `BPR-1083-appearance-tints-v001`: the twelve tints on Appearance › Colours.
6. `BPR-1083-mark-style-v001`: the Icons/Letters toggle.

Limitations: macOS only (Linux and Windows station runs owed); R33's native
matrix is not run; the Overview chips and Label key (BPR-1087, BPR-1089) must
draw through `feedback_mark_theme`; the helper's Settings row is BPR-1088.
