---
schema: "implement-rfc.card.v1"
id: "BPR-0028"
title: "Implement GitHub host and pull-request Qt presentation"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0018","BPR-0027"]
completed: "2026-07-30"
---

# Implement GitHub host and pull-request Qt presentation

Add accessible profile selection, read-only active host/account presentation,
pull-request open and refresh events, normalized metadata/conversation
projection, capability-driven action state, and clear authentication,
permission, network, API-version, and capability errors.

Widgets and Qt models receive only the provider-free application presentation
contract and must not receive credentials, provider records, transports, host
capability ports, raw requests, or accepted-completion internals.

## Result

- Added one provider-free application presentation contract. The application
  host maps accepted session, pull-request, conversation, capability,
  rate-limit, command, and diagnostic state inside its exception boundary and
  publishes one atomic update without effect requests or accepted-completion
  internals.
- Added an accessible GitHub window with profile-only identity selection,
  read-only active host/transport/account output, pull-request open and
  refresh intents, normalized metadata and conversations, capability
  tri-state presentation, fixed bounded diagnostic wording, shortcuts, focus
  handling, and minimum-size scrolling.
- Kept remote title, body, author, path, and comment values as literal plain
  text. No provider, vault, credential, transport, URL, request,
  authentication material, or completion type can cross the Qt boundary.
- Made identity changes and new-target opens immediately clear stale account,
  pull-request, conversation, and rate state. Refresh retains the last
  accepted content through loading, failure, cancellation, and stale
  completion.
- Added a stable conversation model for reviews, threads, nested review
  comments, and pull-request comments. Incremental insert, remove, minimal
  move, and data-change notifications preserve persistent indexes and exact
  order without model reset.
- Kept all configuration, builds, tests, rendering, and verification local or
  target-native. GitHub Actions has no role.

## Evidence

- The final fresh macOS arm64 Qt-enabled configure, clean build, and complete
  CTest run passed 38/38. The focused host, three-scale Qt, and architecture
  matrix passed 6/6.
- A fresh Qt-free `core-only` build passed 17/17. The focused host and
  three-scale Qt matrix passed 4/4 under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- A tests-disabled Release build passed with public self-test and offscreen
  render reporting `PUBLIC_PATH_OK` and `RENDER_OK`; binary-dependency,
  runtime-surface, and test-seam-absence inspection also passed.
- The 1.0x, 1.25x, and 2.0x macOS raster sets were inspected directly. The
  exact selected targets also compiled under Ubuntu 26.04 x86_64, GCC 15.2,
  Qt 6.10.2, and warnings-as-errors; architecture checks passed 2/2 and the
  host/three-scale matrix passed 4/4.
- A direct Linux backtrace isolated a Qt offscreen Fusion cached-painter crash
  at 1.25x. The headless CTest environment now selects Qt's basic Windows
  style; the fractional-scale test then passed 20 consecutive runs and its
  target-generated rasters were inspected. Native-style public-application UX
  certification remains BPR-0031.
- Adversarial model testing covered 3,000 mixed cases, 5,000 arbitrary
  100-row permutations, persistent indexes, model invariants, sanitizers, and
  both 50,000-row rotations as one move.
- Documentation and immutable source-pack validation, tracked and on-disk
  workflow-absence scans, and `git diff --check` passed. Independent contract,
  portability, model, and skeptical reviews found no remaining P0/P1 defect.
- Windows 10/11 x64 target-native verification remains open in BPR-0036. It
  blocks only affected Windows claims and final RFC-BPR-0002 Auto-Verified
  closeout, not later implementation or verification on available targets.
