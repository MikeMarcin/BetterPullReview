---
schema: "implement-rfc.card.v1"
id: "BPR-1206"
title: "The tab strip's plus activates the new tab, and tabs fill the strip, shrink to a minimum, then scroll"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1132"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner decision with note on attended candidate BPR-1132-plus, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1132-plus", "decision": "c1a81e29-5ee4-4163-bed7-d2dabf843af0", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-05", "AC-11"], "related_cards": ["BPR-1132", "BPR-1131", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
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

## Acceptance

- AC1 The plus duplicates the active tab's exact view (file, comparison and
  reading position) and makes the duplicate the active tab with focus in it.
  Cmd-click still opens a background tab.
- AC2 Tabs share the strip's width equally and expand to fill it. As tabs are
  added they shrink to a minimum width. Past that the strip overflows and
  scrolls (wheel or trackpad, and a keyboard route to hidden tabs), keeping the
  active tab in view and the plus reachable. Names still elide from the left
  within a tab.
- AC3 `qt_document_tabs_scale_{100,125,200}` cover 1, 3, 12 and 40 tabs at
  1280, 1100 and the width floor, with focus and accessible names; controls
  (a background duplicate; fixed-width tabs) fail. The width sweeps are rerun.
- AC4 The owner looks at the built strip on macOS: one question.

Not settled by the note: the RFC's "at a platform's width floor the strip
shows the active tab and a count of the rest". If scrolling replaces it, or
the two cannot both hold, ask the owner that as its own question before
building it.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].
