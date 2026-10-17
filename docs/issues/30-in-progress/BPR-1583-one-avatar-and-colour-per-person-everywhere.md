---
schema: "implement-rfc.card.v1"
id: "BPR-1583"
title: "One avatar and one colour per person on every surface"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"It's weird that people have coloured icons in Threads but they're not coloured in Overview.","related_rfcs":["RFC-BPR-0032","RFC-BPR-0069","RFC-BPR-0070"],"related_cards":["BPR-1290","BPR-0783","BPR-1405","BPR-1083"],"reservation":"docs/issues/_reservations/BPR-1583--BPR-1583--657886ed2474.json"}
---

# BPR-1583: One avatar and one colour per person on every surface

The owner said on 2026-10-17: "It's weird that people have coloured icons in
Threads but they're not coloured in Overview." Threads, the Comments drawer,
Discuss and the reply composer draw a person as an initial in a disc tinted
by [[BPR-1290]]'s per-login hue; Overview's participants and review requests
draw the same person as a letter on the neutral `Mid` fill (or the unread
accent for me); Home's account chips and group tiles ([[BPR-0783]]) and the
Settings account card draw a neutral letter as well. Three initials rules
are in use ("reviewer-four" is RF in Threads, R on Overview and Home). The
Threads tint is a pastel that sits at about 1.3:1 against the panel, so the
disc itself is barely there in Light.

No surface shows a GitHub avatar image and none fetches one; this card adds
no fetching.

## Acceptance

- AC1: One rule. The core owns a pure identity-avatar rule: a stable slot from
  the identity (the login, ASCII case-folded, so `MikeMarcin` and `mikemarcin`
  are one person), a fixed table of ten tones each with a fill and letter
  ink for light grounds and for dark grounds, and one initials rule (first
  letter of the first two words of the name split on non-alphanumerics, a
  trailing `[bot]` dropped, upper-cased, `?` for none). The ground decides
  which half of the tone is used; a custom theme whose grounds the table does
  not clear is darkened (light grounds) or lightened (dark grounds) in fixed
  integer steps until it does.
- AC2: One component. The Qt adapter has one avatar painter and one avatar
  widget built on that rule, and every place a person or a GitHub owner is
  drawn as an initial uses it: Threads and Comments drawer notes and their
  reply faces, the open thread's posts, the reply composer's own face,
  Discuss and the thread finder, Overview's participants and review requests
  (a team is a rounded square keyed on `org/slug`, initial from the slug),
  Home's account chips and its Account, Organization and Repository group
  tiles (keyed on the owner), and the Settings account card and its GitHub CLI
  account rows. Mine is ringed, as Threads already rings it. The Profiles
  sheet keeps its neutral tile: a profile is not a person. Sizes stay as each
  surface had them; the letter scales with the disc.
- AC3: Contrast. In Light, Dark, High Contrast Light and High Contrast Dark
  every tone's fill is at least 3:1 against the theme's window and base
  grounds, and its letter at least the theme's text minimum against the fill
  (4.5:1, 7:1 in high contrast); the table holds 7:1 throughout.
- AC4: The same identity is the same colour everywhere. Qt tests read the
  drawn fill of the same logins on a Threads note, an Overview participant
  row, a Home account chip and the Settings account card in all four themes
  and find them equal to each other and to the rule, with a negative control
  that a different slot differs and that [[BPR-1290]]'s pastel rule fails the
  3:1 check. Core tests pin the slot, the initials and the table's contrast.
- AC5: Gates. Focused tests, one full `ctest --preset dev` on the merged
  tree, clang-format 22, tidy and the documentation gate pass. The avatar rule
  is written once in `docs/ux/workbench-specification.md`, and the Home and
  Overview passages that called the tile neutral say what it is now.
- AC6 (human): The owner accepts the frozen candidate in Light and Dark, with
  before and after renders of Overview's participants and Threads: "A person
  has the same coloured avatar in Threads, Overview, Home and Settings."

## Implementation

The core decides the colour and the initials (`identity_avatar`, linking only
the existing contrast measure); Qt presents (`identity_avatar.hpp`). The
Threads helper `paint_avatar` keeps its signature and calls the shared
painter, so the Threads files that others are editing for navigation are not
touched. Overview's side panel is part of a design-only redesign: only its
tile painting changes, not its layout. Home's tile is no longer neutral; on
Home colour still means state for rows and bands, and a person's colour is
the person's own. No stored state, schema or QSS changes. Renders are
offscreen; never drive or capture the owner's desktop.
