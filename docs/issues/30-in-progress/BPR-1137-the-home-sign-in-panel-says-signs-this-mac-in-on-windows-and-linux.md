---
schema: "implement-rfc.card.v1"
id: "BPR-1137"
title: "The home sign-in panel says 'signs this Mac in' on Windows and Linux"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-0653's station capture of 2026-09-25 (docs/verification/BPR-0653-2026-09-25, rest-window.png, Windows 11 at 150%)","reservation":"docs/issues/_reservations/BPR-1136--BPR-1137--8ee928da6b11.json","reservation_note":"Reserved in 444f3e10, whose commit message garbled both titles; this is the correct title. BPR-1136 is bpr-main-win's (the strip's Application menu and UI Automation).","related_cards":["BPR-0775","BPR-0773","BPR-0653"],"related_rfcs":["RFC-BPR-0032"]}
---

# BPR-1137: The home sign-in panel says 'signs this Mac in' on Windows and Linux

## Why

Signed out, Home's first-launch card says:

> A device code in the browser signs this Mac in once; the session is
> remembered here. Paste a pull request link to start a review.

on every platform. [[BPR-0653]]'s station capture shows it on Windows 11
([rest-window.png](../../verification/BPR-0653-2026-09-25/rest-window.png)).
The sentence is [[BPR-0775]]'s accepted first-launch copy. It came from the
macOS mockup, and `workbench_landing.cpp` spells the noun as a literal.

The product already has a rule for naming the machine. [[BPR-0773]] set it in
the device sign-in row ("Waiting for the browser to authorize this Mac.") and
the server card ("remembered on this Mac"): **"this Mac" on macOS, "this
computer" on Windows and Linux.** The landing card never took the noun from
that rule. Each of those two files has its own private copy of the helper, so
nothing pointed the landing at them.

## Scope

- The card's sentence takes the machine's name from the product's rule:
  "this Mac" on macOS, as accepted; "this computer" on Windows and Linux. The
  rest of the accepted sentence does not change.
- The rule lives in one place inside the Qt adapter. The device sign-in row
  and the server card keep saying exactly what they say now.
- A Qt test asserts the card's exact sentence for the platform the test is
  built for, and that off macOS it does not say "Mac". A focused row runs it
  alone.
- Owned paths: `src/adapters/qt/src/workbench_landing.cpp`, the adapter's
  internal machine-name helper and its two existing users
  (`device_authorization_panel.cpp`, `server_card.cpp`), the composed Qt test,
  and its registration.

## Acceptance

- AC1 Signed out, Home's first-launch card says "A device code in the browser
  signs this Mac in once; the session is remembered here. Paste a pull request
  link to start a review." on macOS, and the same sentence with "this
  computer" on Windows and Linux. It takes the noun from the product's one
  machine-name rule, which the device sign-in row and the server card also
  use, unchanged.
- AC2 A Qt test asserts the exact sentence for its build platform and, off
  macOS, the absence of "Mac". It runs in the full composed run and alone as
  `qt_composed_home_sign_in`. It fails with the old wording where the wording
  differs.
- AC3 On macOS, the Qt Home and sign-in rows pass, the new row passes, and the
  manifest gate passes. The touched C++ files are formatted.
