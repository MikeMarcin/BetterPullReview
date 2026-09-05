---
schema: "implement-rfc.card.v1"
id: "BPR-0328"
title: "Name the macOS release executable"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0325"]
---

# Name the macOS release executable

The production bundle declared `CFBundleDisplayName=BetterPullReview`, but VS
Code starts its inner executable directly under LLDB. macOS therefore exposed
the executable basename `better_pull_review` as the application-process name
and used it for the Dock tooltip despite the correct bundle metadata.

## Required final state

- Name the tests-disabled macOS application bundle and executable
  `BetterPullReview`.
- Point the checked-in VS Code release launch configurations at that exact
  bundle executable.
- Preserve the bundle identifier, display name, icon binding, and platform
  names outside the tests-disabled macOS graph.
- Prove the built bundle metadata and executable path agree.

## Completion record

- The macOS bundle target overrides its output name to `BetterPullReview`, so
  both the bundle directory and executable basename use the product name even
  when LLDB launches the executable directly.
- Both checked-in VS Code `app-release` launch configurations use
  `BetterPullReview.app/Contents/MacOS/BetterPullReview`.
- The focused source contract, clean production-shaped build, generated bundle
  metadata inspection, production self-test, and documentation gate pass.

## Non-goals

- Do not rename Linux, Windows, tests-enabled, or designated-review capsule
  executables.
- Do not alter application behavior, signing, installation, or release
  readiness.
