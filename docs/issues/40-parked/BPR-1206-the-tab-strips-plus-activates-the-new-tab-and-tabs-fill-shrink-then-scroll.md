---
schema: "implement-rfc.card.v1"
id: "BPR-1206"
title: "The tab strip's plus activates the new tab, and tabs fill the strip, shrink to a minimum, then scroll"
status: "Parked"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1132"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner decision with note on attended candidate BPR-1132-plus, 2026-09-25","session":"06ad5734ed9f7c2fd6d0a62c8328fa0a","candidate":"BPR-1132-plus","decision":"c1a81e29-5ee4-4163-bed7-d2dabf843af0","reviewed_revision":"ba9d272339ad235f1d99688c5c1af198b0c9c7ef","reviewed_executable_sha256":"a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83","criteria":["AC-05","AC-11"],"related_cards":["BPR-1132","BPR-1131","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1206-tabs","criterion":"AC4","fingerprint":"e32861be696d42dbd52ec3dd339c19db20add466f3b220fa7584ee44c7bc28f4","decision_id":"7cbfb275-a06a-4faf-b668-26ef6d210a1b","outcome":"accept","note":"","saved_at":"2026-09-26T21:00:46.208912+00:00","actor":"human"}]}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f080d9c471feb3b69f8d571b27ba6235c15ca240","evidence":{"kind":"command","detail":"Core/host and document-tab matrix pass; final tab/focus/window 9/9, evidence/1206-final-tests.log. Card records negative controls."}},{"criterion":"AC2","outcome":"pass","revision":"f080d9c471feb3b69f8d571b27ba6235c15ca240","evidence":{"kind":"command","detail":"Core/host and document-tab matrix pass; final tab/focus/window 9/9, evidence/1206-final-tests.log. Card records negative controls."}},{"criterion":"AC3","outcome":"pass","revision":"f080d9c471feb3b69f8d571b27ba6235c15ca240","evidence":{"kind":"command","detail":"Core/host and document-tab matrix pass; final tab/focus/window 9/9, evidence/1206-final-tests.log. Card records negative controls."}},{"criterion":"AC4","outcome":"pass","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1206-tabs accept 7cbfb275-a06a-4faf-b668-26ef6d210a1b. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}}]
---

# BPR-1206: The tab strip's plus activates the new tab, and tabs fill the strip, shrink to a minimum, then scroll

## Owner decision

In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1132-plus` on the
frozen m2 app (`ba9d2723`), the owner selected "Plus should duplicate the
active view" twice. The later decision (`c1a81e29-5ee4-4163-bed7-d2dabf843af0`,
sequence 47, `actor: human`) supersedes the first (`f9bfb3f9`, sequence 46,
no note) and adds this note, verbatim:

> Plus must switch focus to the new tab. Tabs should work like xcode/ghostty where they expand to fiill the available space and shrink to a minimum size before overflowing and allowing scrolling through.

Three reference screenshots are kept with the decision in the session's
external state: Xcode's tab bar with six and with three tabs sharing the full
width equally, and Ghostty with two tabs each taking half.

Duplication is accepted as built. The new tab's focus and the strip's sizing
are new requirements, recorded as an owner amendment on RFC-BPR-0052.

## Decision under the owner's delegation, 2026-09-25

The note left one point open: the RFC's "at a platform's width floor the
strip shows the active tab and a count of the rest", and whether it survives
once tabs scroll. In chat, relayed by the coordinator, the owner answered,
verbatim: "pick one example and run with it, don't ask me". The implementer
chose under that delegation. No owner question is open on this point, and
none is to be asked.

**The one reference is Xcode's editor tab bar on macOS.** Of the four
candidates (Xcode, Sublime Text, Chrome and Ghostty), it fits best. Its tabs
are documents in an editor's header, as this strip's are, and the owner named
it first and attached two screenshots of it. Ghostty's tabs are macOS window
tabs, one terminal window each, not documents in a pane. Sublime Text sizes
each tab to its name instead of sharing the strip equally. Chrome shrinks
tabs toward their icons instead of stopping at a readable minimum. Neither of
those two matches the note.

The strip takes this behaviour from it:

- **One rule at every window width, the width floor included.** No
  presentation is reserved for the floor.
- Tabs share the room the strip gives them equally and grow to fill it. One
  tab fills it alone. When the strip shows [[BPR-1197]]'s comparison status,
  the tabs fill the room beside it, and the status keeps its complete tooltip
  and accessible text.
- As tabs are added, each shrinks to a fixed minimum width. No tab is ever
  narrower than that minimum.
- When the tabs no longer fit at the minimum, the strip scrolls sideways with
  the trackpad or the mouse wheel, and every tab keeps the minimum width.
- Whenever a tab becomes active, the strip scrolls until the whole of it is
  visible. A tab becomes active by a click, the plus, Open in New Tab,
  Ctrl+Tab or Ctrl+Shift+Tab, the close of its neighbour, or restoration.
- There is no count and no overflow button. The built strip's collapse to
  the active tab with a "+N" menu (`documentTabOverflow`), which it takes
  whenever the tabs do not fit, is removed. Ctrl+Tab, Ctrl+Shift+Tab and the
  arrow keys on the focused strip reach every tab, including tabs scrolled out
  of view, and bring each one into view.
- The plus stays at the strip's right end, outside the scrolling tabs, so it
  is always reachable. That placement is this card's own requirement (AC2),
  not something taken from Xcode.
- Within a tab, names still elide from the left, and a tab with its own pair
  keeps its pair label.

Scrolling therefore replaces "the active tab and a count of the rest" at the
width floor. The minimum width is an implementation constant in
device-independent pixels. It is chosen so that a tab at the minimum still
shows its close button, its pair label when it has one, and the end of its
file name. The scale tests state it and pin it.

## Acceptance

- AC1 The plus duplicates the active tab's exact view (file, comparison and
  reading position) and makes the duplicate the active tab with focus in it.
  Cmd-click still opens a background tab.
- AC2 Tabs share the strip's width equally and expand to fill it. As tabs are
  added they shrink to a minimum width. Past that the strip overflows and
  scrolls (wheel or trackpad, and a keyboard route to hidden tabs), keeping the
  active tab in view and the plus reachable. Names still elide from the left
  within a tab. The same rule holds at every width, the width floor included,
  as decided above: no count and no overflow button.
- AC3 `qt_document_tabs_scale_{100,125,200}` cover 1, 3, 12 and 40 tabs at
  1280, 1100 and the width floor, with focus and accessible names. Three
  controls fail: a background duplicate, fixed-width tabs, and a strip that
  collapses to the active tab and a count. The width sweeps are rerun.
- AC4 The owner looks at the built strip on macOS: one question.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].

## Engineering evidence — 2026-09-26

Decision: a 220 device-independent-pixel minimum leaves the close control,
pair pill and filename ending readable; a separate scroll viewport keeps the
plus reachable. Tabs divide remaining space equally, stop shrinking at that
minimum and scroll by wheel/trackpad. Keyboard activation reveals the whole
active tab. Plus dispatches the existing duplicate transition and focuses
the strip; background Cmd-click retains its existing core semantics.

The core and host tab courses pass, and the Qt matrix covers 1, 3, 12 and 40
tabs at 1280, 1100 and the width floor at 100/125/200. Injected background
duplication, fixed 100-pixel tabs and active-only collapse fail their intended
predicates. The scale courses, focus-order courses and full window width
sweeps pass 9/9 on the final candidate. The owner look remains pending.

Integration corrected the older-pair window expectation left by BPR-1205 and
focus on BPR-1203's no-draft page: Back receives focus when its body is hidden.
The first full-window run then found its fixed-shell ratio at 1201 widgets
versus a 1200-widget threshold. Diagnostic counts are identical for 60, 6000
and 60000 source lines, with 37 diff-surface widgets at every size. The strict
whole-window equality remains; the two-orders-of-magnitude ratio now measures
the diff surface, so adding fixed shell controls cannot masquerade as
source-dependent allocation. No growing-widget check was removed.

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC4 — Accept**, candidate `BPR-1206-tabs`, decision `7cbfb275-a06a-4faf-b668-26ef6d210a1b`. No note.
