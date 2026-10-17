---
schema: "implement-rfc.card.v1"
id: "BPR-1580"
title: "Home arranges and filters from one View menu whose groups are labelled"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"I wonder if we should collapse these 3 into one. I don't understand what half these options on Filter are even meant to do.","governing_rfc":"RFC-BPR-0032","related_rfcs":["RFC-BPR-0066","RFC-BPR-0028","RFC-BPR-0024"],"related_cards":["BPR-0783","BPR-1567","BPR-1573","BPR-1575"],"reservation":"docs/issues/_reservations/BPR-1580--BPR-1580--b54fb616e5aa.json"}
---

# BPR-1580: Home arranges and filters from one View menu whose groups are labelled

The owner said on 2026-10-17, of Home's strip: "I wonder if we should
collapse these 3 into one", and of Filter: "I don't understand what half
these options on Filter are even meant to do". The three are [[BPR-0783]]'s
**Group: None ▾**, **State headers** and **Filter ▾**. Filter's four groups
(My role, Added by, Quiet items, Sort within a group) are `QMenu::addSection`
titles, and the native macOS menu draws a section as a bare separator, so on
the owner's Mac the menu is twenty unlabelled choices. Nothing in it says what
Participant, No role, an observer rule or a quiet item is.

Governing design: Home's strip, Group, State headers and Filter of
[RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md)
as [RFC-BPR-0066](../../rfcs/RFC-BPR-0066-home-authored-pull-requests.md)
amends it, the strip of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md), and
the per-window view state of
[RFC-BPR-0024](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md).
What each choice does, what is stored and where stays as [[BPR-0783]] left
it; this card changes how the choices are reached, named and summarised.

## Acceptance

- AC1: One control. While Home is shown and an account can fill its list, one
  **View** menu button stands where Group, State headers and Filter stood,
  before the refresh glyph; signed out and on the review strip it is absent.
  Its menu reads, top to bottom: **Group by: ‹value›** ▸ (None, Organization,
  Repository, Account), **Sort by: ‹value›** ▸ (Priority, Repository, Pull
  request number), **State headers** (checkable), a separator, **My role:
  ‹value›** ▸ (Any, Author, Reviewer, Requested, Participant, No role),
  **Added by: ‹value›** ▸ (Any, Repository rule, Author rule, Path rule),
  **Quiet items: ‹value›** ▸ (Show, Hide, Only quiet), a separator, **Reset
  to defaults**, a separator, the footer "Nothing here changes GitHub.
  Observer rules are in Settings › Reviews & queue." and **Open Reviews &
  queue**. Every group's label is an ordinary menu item's text, so the native
  macOS menu draws it; the menu uses no section titles. Each submenu is one
  exclusive choice. Every choice carries a tooltip and status tip that says
  in plain words what it shows, including Participant, No role, each rule
  kind and what makes an item quiet.
- AC2: Added by is shown only when the profile has an observer rule, or when
  the window is already filtered by one (so a filter is never out of reach).
- AC3: The button reads **View** at the defaults. When any choice in the menu
  differs from its default it carries a small neutral count of how many do,
  drawn from the palette, and its accessible description and tooltip name
  each such choice. **Reset to defaults** is enabled exactly then and in one
  gesture restores Group None, State headers on, My role Any, Added by Any,
  Quiet items Show and Sort Priority; the profile hears the filter part as it
  hears a filter choice.
- AC4: When the queue has rows and the view hides all of them, the list says
  so in one sentence with a **Reset view** link that Tab, Return and Space
  reach, instead of an empty page; on Home the status line does not repeat
  it. An empty queue keeps its own sentence.
- AC5: Behaviour is kept: each choice dispatches the same typed queue intent
  and per-window view as before, two windows still filter independently, the
  stored view and preferences are unchanged (no schema change), the button is
  in the strip's Tab order and opens by keyboard, and the submenus are
  reachable by arrow keys and named for assistive technology.
- AC6: Qt and composed tests cover every option reached and applied, labels
  as item text, Added by hidden without rules, the indicator, reset, the
  filtered empty sentence and persistence, with a negative control that fails
  on section-title groups; focused tests and one full `ctest --preset dev` on
  the merged tree, clang-format 22, tidy, the documentation gate and the
  resealed composition contract pass.
- AC7 (human): The owner accepts the frozen candidate in Light and Dark:
  "Home's View menu replaces Group, State headers and Filter, its groups are
  labelled and explained, and a changed view is visible on the button."

## Implementation

Qt presents: `workbench_window` builds the one menu and button; `home_list`
gains the filtered sentence. No core or host change, no QSS, no new stored
state. The composition's strip-order check names the one View action, so the
round-trip composition contract is resealed. BPR-1567's shortcut catalogue
lists none of these controls and no key is added. The other Home cards
([[BPR-1573]]'s Close review, [[BPR-1575]]'s refresh feedback) are not
touched. Never drive or capture the owner's desktop; renders are offscreen and
the owner checks the frozen build themselves.
