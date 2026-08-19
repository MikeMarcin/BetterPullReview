---
id: BPR-0370
title: The unbundled test build cannot register with Notification Center
status: In Progress
rfc: RFC-BPR-0010
depends_on: []
---

# The unbundled test build cannot register with Notification Center

Found by the owner attempting the v002 notification packet. The course armed
correctly — `NATIVE_NOTIFICATION_QUIET_BASELINE_OK`,
`NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK`, the exact payload, then
`NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK` — and no banner ever appeared.
The owner checked System Settings → Notifications directly: **no
BetterPullReview entry exists at all**, and the appearance run's status dump
recorded `NOTIFICATION_PERMISSION:not-determined`.

The mechanism is structural. `build/dev/src/app/better_pull_review` is a
bare Mach-O with no bundle and no `Info.plist`, and macOS Notification
Center registers applications by bundle identity: an unbundled process
cannot request authorization, never gains a Settings entry, and its
delivery requests go nowhere. BPR-0328 gave the *tests-disabled* build its
bundle; the tests-enabled build that every attended packet binds never got
one. The deterministic preflight
(`application_native_notification_attended_contract`) passes because it
deliberately substitutes native display — the one fact it cannot check is
the one that was false. BPR-0168's own refrain applies a third time:
attendance was never what was missing; a banner no process identity can
produce cannot be clicked.

## Required final state

- The attended notification course runs from an executable with real bundle
  identity, so first launch triggers the macOS authorization prompt, the
  app appears under System Settings → Notifications, and a granted request
  produces a real banner.
- Whether that is the tests-enabled app becoming a `MACOSX_BUNDLE` (mirroring
  BPR-0328's tests-disabled shape, keeping documented paths stable via
  `OUTPUT_NAME` or updated packets) or a narrower attended-course bundling
  is decided by the slice from the build's own constraints, not assumed.
- The deterministic preflight gains the check that was missing: on macOS it
  fails when the running process has no bundle identity, so an unbundled
  build can never again present an attended banner packet.
- The BPR-0168 packet is regenerated against repaired bytes.

## Verification

```sh
ctest --preset dev -R '^(application_native_notification_attended_contract|application_attended_launch_contract)$' --output-on-failure
```
