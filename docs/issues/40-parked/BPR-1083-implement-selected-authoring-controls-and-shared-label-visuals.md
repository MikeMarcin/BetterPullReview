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

## Owner verdicts, session `8bf44458` (v001), 2026-10-05

Handed back at sequence 23. Verbatim:

- `BPR-1083-comment-menu-v001`: **Accept**, no note. Comment ▾ keeps no
  labels; the reservation in `docs/ux/workbench-specification.md` and the
  comment in `workbench_window.cpp` are withdrawn.
- `BPR-1083-appearance-tints-v001`: **Accept**. "Aside: These section labels in
  settings are super ugly, needs some frontend design love." Filed as
  [[BPR-1288]].
- `BPR-1083-mark-style-v001`: **Accept**. "Aside: I really want one of those
  full settings search/filter boxes like this codex screenshot. I think we had
  some fuzzy text filtering code in SiftThroughLogs we might be able to port
  over. / This reviews and queues settings page is incredibly ugly and needs
  some frontend design love. / The setting is there and appears to work
  although it'd be nice to see a preview." The asides are [[BPR-1289]] and
  [[BPR-1288]]; the preview is built here.
- `BPR-1083-chip-menu-v001`: **Reject**. "aside: this is what happens when you
  go to Read without a file open. We need to not show a blank diff here and
  instead have some clear direction to open a file. / I'm not able to leave a
  comment on this review fixture so I cannot tell if this is working." The
  aside is [[BPR-1287]]; the rejection is the review setup, fixed below.
- `BPR-1083-shortcodes-v001`, `BPR-1083-thread-marks-v001`: unanswered (the
  owner could not comment).

## Round v002, 2026-10-05

`5793e0759`: the v001 launch opened on Home, whose bar reads in public
preview, read-only. `--attended-open-file` now signs in with the fixture's
account and opens `src/engine/renderer.cpp` in Read; the
`--label-authoring-probe` course (`application_label_authoring_contract`)
drives Comment, `:qu:`, the chip, ⌘L, Publish and the refreshed mark. It found
that the shortcode's replacement was refused as stale while the closing
colon's edit was still with the draft owner; the editor now holds it until
the answer. Reviews & queue shows six marks beside Label marks. Details and
checks are in the [execution ledger](../../verification/RFC-BPR-0047-0051.md).

## Owner verdicts, session `c2a2442c` (v002), 2026-10-05

Handed back at sequence 27 (cursor 20 to 27 read). Verbatim, line breaks
shown as " / ":

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1083-chip-menu-v002` | Accept (`0df6d634`) | "Functionality works as expected. / I hate this full screen comment page being the default. I want to be able to still see the code I'm commenting on. Maybe it has an expand button to get something similar to this. / I notice we have style for blocking but noting for non-blocking or if-minor" |
| `BPR-1083-mark-preview-v002` | Accept (`d53987f5`) | "There should be individual tooltips instead of one giant tooltip." |
| `BPR-1083-shortcodes-v002` | Accept (`2963585b`, superseding an earlier Accept with no note, `8ad550bc`) | "I feel like since it's only the first character that does this replacement just typing : should be enough to open suggestions." |
| `BPR-1083-thread-marks-v002` | **Reject** (`d077b020`) | "Marks don't show on inline comments. / I also think I've identified why comments drawer feels so clunky, that one tab is trying to serve 2 masters. I think we need to split the inline comments and the comment tree into separate ttabs in the drawer." |
| `BPR-1287-read-empty-v001` | Accept (`bb63ddaf`) | (none); [[BPR-1287]] is Done |

The two screenshots are
[`BPR-1083-owner-ref-full-page-composer.png`](../../design/owner-refs/BPR-1083-owner-ref-full-page-composer.png)
(chip-menu: the comment page filling the comparison area) and
[`BPR-1083-owner-ref-drawer-thread-cards.png`](../../design/owner-refs/BPR-1083-owner-ref-drawer-thread-cards.png)
(thread-marks: the drawer's "Inline thread · @tbutler" entries with a rail
and no mark), downscaled to 1392 px with metadata dropped.

Where each part goes:

- **The composer inline beside the code, with an expand button** is
  [[BPR-1212]]'s "new inline comment" flow; it is recorded there as owner
  amendment AC5, not as a new card.
- **Splitting the drawer** into an inline comments tab and a comment tree tab
  belongs to [[BPR-1290]], whose implementer records it on that card. This
  card does not split the drawer.
- **Marks on the drawer's entries, the decorations, the lone colon and the
  preview's tooltips** are built here, as round v003.

## Round v003, 2026-10-05

Designed with the `frontend-design` skill (coordinator, relaying the owner).
The [delivery contract](../../ux/conventional-comments-feedback.md) records
each rule.

- **Marks on inline comments** (`thread-marks-v002` Reject). The drawer's
  entries are compact cards, which drew their root's rail but no mark. Each
  now leads its first line with the root post's 16 px mark, right of the
  rail, with the text after it; inline threads and my draft's entry alike.
  The mark's tooltip is the root post's name ("tbutler, issue (blocking)").
- **Non-blocking and if-minor** (`chip-menu-v002` note). The decorations form
  a scale of weight, and the rails borrow road markings: a solid line is not
  crossed, a broken one may be. Blocking: filled mark, 4 px solid rail.
  Undecorated: soft mark, 2 px solid rail. If-minor: the soft mark with its
  top-left corner squared off and filled in the tint, 2 px dotted rail.
  Non-blocking: an open mark (the panel inside a dashed border), 2 px dashed
  rail. All four differ by shape as well as colour. The corner clears every
  glyph and both letters. They apply on every surface: thread cards and
  drawer entries, the post list, the reader, the chip and the preview.
- **A colon alone** (`shortcodes-v002` note) opens completion with all twelve
  labels in catalog order at the start of a comment; letters narrow it. Esc,
  a colon anywhere else, and pasted text behave as before.
- **One tooltip per preview mark** (`mark-preview-v002` note). Each of the six
  marks shows its own name and catalog description, and the gaps show none.
  The preview now also shows suggestion (if-minor) and nitpick
  (non-blocking).

Checks and the negative controls are in the
[execution ledger](../../verification/RFC-BPR-0047-0051.md).

## Round v003's session

One attended session asked one Accept or Reject per candidate, on a frozen
build of `3cddd0cdd` (archived before the rebase from `e8c9dc65c`, the same
product tree; `/Users/jaeger/bpr1083-authoring-review-v003.AnLWd2`,
executable SHA-256 `2158e3d0…8ad0f77`).
`python3 /Users/jaeger/bpr1083-authoring-review-v003.AnLWd2/launch.py` opens
`src/engine/renderer.cpp` in Read, signed in as the fixture's account. Session
`5674c48efbb188e1183f72e4099c8545`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1083-authoring-v003`,
cursor 16:

1. `BPR-1083-thread-marks-v003`: the drawer's thread entries, and my draft's
   entry, lead with their first comment's mark.
2. `BPR-1083-decorations-v003`: non-blocking and if-minor each look distinct
   and quieter than blocking at 16 px.
3. `BPR-1083-colon-trigger-v003`: a single `:` at the start opens the label
   suggestions.
4. `BPR-1083-preview-tooltips-v003`: one tooltip per preview mark.

Accepted in v002 and not asked again: the chip, menu, toggles and Publish
(`chip-menu-v002`), the shortcodes (`shortcodes-v002`) and the preview
(`mark-preview-v002`).

## Owner verdicts, session `5674c48e` (v003), 2026-10-05

Handed back at sequence 25 (cursor 16 to 25 read). Verbatim, line breaks
shown as " / ":

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1083-colon-trigger-v003` | Accept (`eaa28196`) | (none) |
| `BPR-1083-thread-marks-v003` | Accept (`326221de`) | (none) |
| `BPR-1083-decorations-v003` | **Reject** (`6c45adf5`, superseding two earlier Rejects with the same note cut shorter, `756337bf` and `d96b2838`) | "These are good, but I think we've. gone a bit of a wrong direction. Decorrators according to https://conventionalcomments.org/ are org specific and multiple decorators can be applied to a comment. like (non-blocking, if-minor).  Obviously something like (blocking, non-blocking) is non-sense so don't worry about it. / Here's what I think. I think we keey normal, blocking, non-blocking sytles for easy scanning. We can also support if-minor style as just filling in the top left corner.  / I think we always display the decorators text out in the comments so we can default support custom decorators and people don't have to learn our styles, its just a bonus helpful ux." |
| `BPR-1083-preview-tooltips-v003` | **Reject** (`4bc1c16e`, superseding `ba01bdfd`, withdrawn at sequence 22) | "The tooltip feels strangely far away." |

The rejection's screenshot is
[`BPR-1083-owner-ref-preview-tooltip-far.png`](../../design/owner-refs/BPR-1083-owner-ref-preview-tooltip-far.png):
the praise mark's tooltip, "praise / Recognize something genuinely
valuable.", opening well below and right of the six preview marks. It is the
owner's attachment at its own 790 px, with its colour profile, EXIF and text
chunks dropped.

The coordinator relayed the direction for round v004 (owner, in chat, on
handback): marks and rails keep three styles, normal, blocking and
non-blocking; if-minor is an overlay that fills the top-left corner of any of
them; several decorations are allowed and kept exactly as written, and any
custom decoration is accepted; the decoration text is always shown in the
comment, as in "issue (non-blocking, if-minor):", in thread cards, the reader
and the composer; the chip menu's decorations combine and take a custom one;
and a mark's tooltip sits right next to the mark, directly below or above
it, everywhere marks have tooltips.

Limitations: macOS only (Linux and Windows station runs owed); R33's native
matrix is not run; the Overview chips and Label key (BPR-1087, BPR-1089) must
draw through `feedback_mark_theme` and `feedback_mark_emphasis`; the helper's
Settings row is BPR-1088.
