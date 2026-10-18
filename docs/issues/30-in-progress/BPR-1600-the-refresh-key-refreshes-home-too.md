---
schema: "implement-rfc.card.v1"
id: "BPR-1600"
title: "The Refresh key refreshes Home too"
status: "In Progress"
rfcs: ["RFC-BPR-0026"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner decision 2026-10-17","owner_note":"⌘R (QKeySequence::Refresh / Ctrl+R) should refresh Home too, reversing BPR-0990's choice that Home's refresh has no key.","governing_rfc":"RFC-BPR-0026","related_cards":["BPR-0990","BPR-1567","BPR-1575","BPR-1580"],"reservation":"docs/issues/_reservations/BPR-1600--BPR-1600--5a60520b8335.json","review":"lands without an owner review once the tests prove it (a key binding for an existing action)"}
acceptance: []
---

# BPR-1600: The Refresh key refreshes Home too

The owner decided on 2026-10-17 that ⌘R (`QKeySequence::Refresh`, Ctrl+R or
F5 off macOS) should refresh Home too, reversing [[BPR-0990]]'s choice that
Home's refresh has no key. The amendment is recorded against
[RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md),
which governs Home's explicit Refresh action.

## Scope

- One window shows Home or a pull request; the Refresh key does whichever
  refresh is showing. On Home it triggers Home's existing
  `refreshQueueAction`, the same path as its strip button, with
  [[BPR-1575]]'s feedback ("Finding my pull requests…", the result note,
  failure and Retry). On a pull request it stays `refreshPullRequestAction`.
- The key is on one action at a time (`workbench_window::apply_refresh_key`),
  taken off one before it is put on the other, so no press is ambiguous.
- The pull request's binding does not step aside while typing, so Home's does
  not either: ⌘R in Home's Open bar refreshes Home and leaves the text alone.
- Help → Keyboard Shortcuts ([[BPR-1567]]) lists "Refresh my pull requests"
  (On Home) beside "Refresh the pull request" (With a pull request showing).
  [[BPR-1580]]'s View menu and Home's refresh tooltip do not mention a key and
  are unchanged.

## Acceptance

- `BPR-1600:home`: on Home, signed in, ⌘R sends exactly one discovery
  refresh, the reviewer's, with the note, a failure with Retry and a result;
  a second press while it runs starts nothing.
- `BPR-1600:pull-request`: on a pull request ⌘R refreshes the pull request
  once and not Home; with Home over the review the key is Home's again.
- `BPR-1600:typing`: neither key steps aside while typing; ⌘R from the Open
  bar refreshes Home and leaves the bar's text and focus.
- `BPR-1600:negative`: signed out on Home, ⌘R refreshes nothing.
- `BPR-1600:catalogue`: the catalogue drift check passes in Review and on
  Home; no key is bound twice.
- `BPR-1600:gates`: clang-format 22, tidy on the changed lines, the
  documentation gate.
