---
schema: "implement-rfc.card.v1"
id: "BPR-1023"
title: "Reconcile the anonymous-preview Accounts course with shared sign-in's Connect, and decide whether a refused activation stops a device-capable card's Connect"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0940 integration pass, Mac integrator, 2026-09-23","found_at":"f839e26a","related_cards":["BPR-0940","BPR-0915","BPR-0772","BPR-0597"],"suite":"qt_anonymous_public_preview_scale_100/125/200"}
---

# BPR-1023: Reconcile the anonymous-preview Accounts course with shared sign-in's Connect

## Observation

`qt_anonymous_public_preview_scale_100`, `_125` and `_200` stop in
`test_accounts_offer_no_public_preview_activation`
(`tests/adapters_qt/anonymous_public_preview_tests.cpp`) at

`GitHub.com's Connect, while it is not chosen, activates exactly its native HTTPS profile`

[[BPR-0597]] recorded this as untracked and pre-existing. It is the same drift
[[BPR-0940]] reconciled in the composed GitHub suite: since [[BPR-0915]]
(`cbd50b74`), a server card's Connect on a host with a device flow chooses
that host's native profile and begins device sign-in in the shared sign-in
dialog (`workbench_window::add_server_card`, the `.Connect` gesture, through
`begin_selected_sign_in`). It no longer dispatches the activation intent that
the [[BPR-0772]] course expects, and the course's closing claim that "neither
Connect nor the GitHub CLI offer emits device sign-in" no longer holds.

A diagnostic run at `f839e26a` with the course's `expect` made non-fatal
(not committed) observed, in order:

- GitHub.com's Connect while not chosen: 0 activation intents, 1 device begin.
- Five later assertions of the same course fail for the same reason (the card
  is in the dialog's setup presentation, and Connect was the device retry).
- **With the activation command refused** (`activate_host_profile` disabled,
  `invalid_profile`), a click on GitHub.com's hidden Connect dispatched no
  activation intent but **began a second device sign-in** (device begins
  1 → 2). The enterprise card's Connect and the GitHub CLI offer dispatched
  nothing.

## Decision needed

The course's last row asks that a card's ways in obey the host activation
command state. The Connect button is hidden while activation is refused, so
a reviewer cannot press it; but its handler, reached programmatically, begins
device sign-in because `begin_selected_sign_in` clicks the device panel's
begin button, which the device flow's own commands govern. Decide one of:

1. A device-capable card's Connect is refused whenever its Connect is not
   admitted (the handler returns before choosing or beginning), matching
   the enterprise card and the GitHub CLI offer; or
2. Device begin is the core's separate command and correctly governs this
   path; the course's last row then asserts the device command's state
   rather than the activation command's.

Either way, reconcile the course to the shipped [[BPR-0915]] entry path
(Connect on GitHub.com begins device sign-in in the shared dialog) without
weakening the exact-profile checks.

## Acceptance

- AC1: The decision above is recorded with its reason, and the product or the
  course changes accordingly, with a failing-before control.
- AC2: `test_accounts_offer_no_public_preview_activation` follows the shared
  sign-in entry path, keeps its exact-profile and no-public-preview checks, and
  passes at all three scale registrations.

## Boundaries

Fixture and Qt-level only; no live GitHub, credential or device-flow request.
