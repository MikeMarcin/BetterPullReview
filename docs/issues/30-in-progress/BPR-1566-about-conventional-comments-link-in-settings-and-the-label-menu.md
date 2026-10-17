---
schema: "implement-rfc.card.v1"
id: "BPR-1566"
title: "About Conventional Comments: a link in Settings and the comment label menu"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-request","found_by":"Owner request via coordinator, 2026-10-16","governing_rfc":"RFC-BPR-0048","related_cards":["BPR-1083","BPR-1088","BPR-1338","BPR-1565","BPR-1568"],"reservation":"docs/issues/_reservations/BPR-1566--BPR-1566--47437684c550.json","acceptance":["BPR-1566:menu","BPR-1566:settings","BPR-1566:path","BPR-1566:gates","BPR-1566:owner"]}
acceptance: []
---

# BPR-1566: About Conventional Comments: a link in Settings and the comment label menu

## Owner request

Owner, 2026-10-16, verbatim: "add a small thing to the settings page and
dropdown picker menu for the comment labels that links to
https://conventionalcomments.org/ for people who don't know where those come
from or how to use them."

## Design

- **Label menu.** Under a rule, the menu's last item is "About Conventional
  Comments", in the palette's link colour, with the tooltip and accessible
  description "Where these labels come from and how to use them. Opens
  conventionalcomments.org in your browser." It is not a label choice: no
  edit, no draft command, the menu closes and focus returns to the comment.
  Arrow keys reach it below the custom decoration field; on a plain comment
  Down from the grid's last row passes the disabled decorations and field.
  ⌘0 stays Plain anywhere in the open menu. Shown only where a window bound
  the dispatch.
- **Settings › Reviews & queue › Comment labels.** First line under the
  heading: "These labels follow a widely used format for review comments." in
  the quiet ink, then the same link. Not a preference (Reset leaves it alone).
  A page that did not open says why under the link; an open says so in the
  window's status bar.
- **Path.** Qt names `core::reference_page::conventional_comments`; the
  window's dispatch goes to composition, which calls
  `application::open_reference_page` with the window's browser port. The core
  (`admit_reference_page`) admits the page as host `conventionalcomments.org`
  and path `/`, `unavailable` without a browser and `refused` for an unnamed
  value. The application builds `https://conventionalcomments.org/`, requires
  it to pass `address_matches_verification_authority` (exact host, HTTPS
  only) and hands it to the browser port, whose production opener validates
  it again. No address, `QUrl` or opener exists in Qt.

## Acceptance

- **BPR-1566:menu** The label menu shows the item last, under a rule, with
  its accessible name and description; arrow keys and Tab reach it from a
  plain and a labelled comment; Return or a click names the page exactly
  once, closes the menu and changes neither text, label nor draft; ⌘0 and
  the existing keyboard behaviour are unchanged; unbound, the item is absent
  and Down stops where it did (negative control).
- **BPR-1566:settings** Comment labels shows the sentence and link under its
  heading, above the rows, reachable by keyboard with accessible name and
  description; activation names the page once and dispatches no preference;
  failure, unavailable and refused say so under the link; Reset leaves it.
- **BPR-1566:path** Through the real composition and the recording browser,
  the Settings link and the label menu's request each open exactly
  `https://conventionalcomments.org/`; the core and application refuse an
  unnamed page and report no browser and a failing browser.
- **BPR-1566:gates** Focused rows, composition contracts, architecture
  boundaries, clang-format 22, clang-tidy and the documentation gate pass;
  one full dev ctest before landing.
- **BPR-1566:owner** The owner accepts the look in Light and Dark.

## Evidence

Recorded at finish.
