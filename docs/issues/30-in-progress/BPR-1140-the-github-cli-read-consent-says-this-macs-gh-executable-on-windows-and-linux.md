---
schema: "implement-rfc.card.v1"
id: "BPR-1140"
title: "The GitHub CLI read consent says \"this Mac's gh executable\" on Windows and Linux"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-1137's 'Found alongside, not changed here' section (ba521357)","reservation":"docs/issues/_reservations/BPR-1138--BPR-1140--96a55e982e06.json","related_cards":["BPR-1137","BPR-0773","BPR-0933","BPR-0803"],"related_rfcs":["RFC-BPR-0015"]}
---

# BPR-1140: The GitHub CLI read consent says "this Mac's gh executable" on Windows and Linux

## Why

The GitHub CLI read opt-in, the question behind Use GitHub CLI
(`make_github_cli_read_consent`, [[BPR-0933]]'s short decision), says this on
every platform:

> BetterPullReview will read as the account gh is signed in to, and that
> account can change outside this app. You trust this Mac's gh executable,
> configuration and certificates as they are set up. Connecting for reads
> does not authorize publishing; each review or comment asks separately.

`github_cli_consent.cpp` spells "this Mac's" as a literal. On Windows and
Linux the reviewer is asked to trust a Mac they are not using. The GitHub CLI
way in is offered there as well: [[BPR-1114]] hardened its Windows launches.

The product names the machine by one rule, which [[BPR-0773]] set: "this Mac"
on macOS, and "this computer" on Windows and Linux. [[BPR-1137]] moved that
rule into one place, `src/adapters/qt/src/this_machine_text.hpp`, and found
this sentence while doing so. It left the sentence alone, because it was
outside that card's scope.

## Scope

- The consent's informative text takes the machine's name from
  `this_machine_text.hpp`. On macOS it stays byte for byte as it is. On
  Windows and Linux it says "this computer's gh executable". The headline,
  the Show Details text, the buttons and their roles do not change.
- A Qt test asserts the exact informative text for the platform the test is
  built for. Off macOS it also asserts that the word "Mac" appears nowhere in
  the question. A focused row runs that test alone, so that each platform
  confirms its own wording without the full composed run.
- Owned paths: `src/adapters/qt/src/github_cli_consent.cpp`,
  `tests/adapters_qt/composed_github_tests.cpp` and the row's registration in
  `tests/adapters_qt/CMakeLists.txt`.

## Acceptance

- AC1 The GitHub CLI read consent's informative text reads "BetterPullReview
  will read as the account gh is signed in to, and that account can change
  outside this app. You trust this Mac's gh executable, configuration and
  certificates as they are set up. Connecting for reads does not authorize
  publishing; each review or comment asks separately." on macOS. On Windows
  and Linux it is the same sentence with "this computer's". The noun comes
  from `this_machine_text.hpp`. Nothing else in the question changes.
- AC2 A Qt test asserts the exact informative text for its build platform.
  Off macOS it also asserts that no word "Mac" appears in the headline, the
  informative text or the details. The test runs in the full composed run,
  and alone as `qt_composed_cli_read_consent`. It fails with the old literal
  where the wording differs.
- AC3 On macOS, `qt_composed_cli_read_consent`, `qt_composed_github_scale_100`
  (which also runs [[BPR-0933]]'s `test_cli_read_consent_is_a_short_decision`)
  and `test_network_isolation_manifest` pass. The touched C++ files are
  formatted, `tidy.py` reports nothing on a changed line, and the
  documentation gate passes.

## Boundaries

This is wording in the Qt adapter. It changes no consent behaviour, no host,
account or trust rule, and nothing in the GitHub adapter. The Windows and
Linux runs of the new row are owed to those stations after this card closes,
as [[BPR-1137]]'s runs are.
