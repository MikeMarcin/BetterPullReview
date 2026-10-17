---
schema: "implement-rfc.card.v1"
id: "BPR-1575"
title: "Refresh shows it is working and what it found"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-17","owner_note":"I wish that refresh on both Home and PR was more visible as an action. It's kind of hard to tell it's doing anything.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0032","RFC-BPR-0026"],"related_cards":["BPR-0646","BPR-0990","BPR-1475","BPR-1567","BPR-1576"],"reservation":"docs/issues/_reservations/BPR-1575--BPR-1575--13a6199698e6.json"}
---

# BPR-1575: Refresh shows it is working and what it found

The owner asked on 2026-10-17: "I wish that refresh on both Home and PR was
more visible as an action. It's kind of hard to tell it's doing anything."

The refresh glyph in the title row ([[BPR-0646]]; Home's discovery refresh,
[[BPR-0990]]) is disabled while a read runs and greys out; nothing else says a
read started, finished, found something or failed.

Governing design: the title row of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md), Home of
[RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md) and
discovery of
[RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md),
recorded in the [workbench specification](../../ux/workbench-specification.md).
The panes' own waits are [[BPR-1576]]'s pattern.

## Acceptance

- AC1: The lifecycle is decided in the core from the read's own stages (the
  pull request's read delta, Home's discovery state): started by the
  reviewer, by the application itself (after publishing or resolving a
  thread, arriving at Home) or handed over by another window; under way;
  updated with what changed between the two accepted reads (new comments,
  reviews, a push and its commits, a state change; new pull requests on Home)
  or nothing; failed as offline, rate limited (with GitHub's wait), signed out
  or other, keeping what was shown. Ages read "just now", "2 min ago" and on.
- AC2: In the same turn as the gesture the button reads as engaged and its
  glyph turns (a still mark under reduced motion), and a note under it says
  "Refreshing the pull request…" or "Finding my pull requests…"; a slow read
  says what it waits on. It ends in a brief tick and a note saying what the
  read brought ("Updated: 2 new comments and 1 new push.", "Up to date: nothing
  new.") or why it failed, with Retry; a failure keeps a dot on the button and
  "Not refreshed · Retry" in the status bar, which otherwise says how old the
  content is and ages by itself. An automatic or handed-over update says
  "Updating…" and speaks only if it brought news. No layout moves.
- AC3: A screen reader hears the reviewer's start, every result and any
  failure once, politely; the button's description says the state and the age.
- AC4: The refresh key is the platform's Refresh (⌘R on macOS), listed in the
  keyboard shortcuts catalogue ([[BPR-1567]]) as "Refresh the pull request";
  Home's refresh keeps no key of its own ([[BPR-0990]]).
- AC5: Core, Qt and composed courses cover AC1 to AC3 for both surfaces with
  stubbed reads (slow, changes, no change, offline, rate limited) and a
  negative control with no read stage; renders in Light and Dark. Format,
  tidy, architecture, documentation and composition contracts pass.
- AC6 (human): The owner accepts the frozen candidate, Light and Dark, with a
  slow refresh to press: "Refresh shows it is working, and says what it found
  or why it failed".

## Implementation

Core decides: `bpr/core/refresh_feedback.hpp`. The host carries the core's
read delta in the transition projection. Qt presents: the strip button's
busy, tick and failure looks, `qt::refresh_note`, the status bar's freshness
and the announcements. The composition marks its own rereads automatic.
Fixtures can slow refreshes and script their outcomes for the frozen build.
Never drive or capture the owner's desktop.
