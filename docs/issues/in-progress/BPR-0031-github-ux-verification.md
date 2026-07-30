---
id: BPR-0031
title: Verify RFC-BPR-0002 rendered UX and accessibility
status: In Progress
rfc: RFC-BPR-0002
depends_on: [BPR-0028, BPR-0029, BPR-0030]
---

# Verify RFC-BPR-0002 rendered UX and accessibility

Exercise deterministic UI automation through the real application, capture and
inspect rendered states, and verify focus, keyboard access, accessible names,
active host/account visibility, loading, success, each failure class, identity
switching, stale-completion rejection, and capability-disabled actions.

Include a deliberate presentation-polish pass over hierarchy, spacing,
density, typography, control grouping, empty/loading/error states, and
conversation readability. Verify the real native application under the
system-default light and dark appearances. The application must inherit the
active system appearance at startup without forcing a light palette and must
update while running after a live system appearance change. The deterministic
offscreen fixture with a forced cross-platform style is layout evidence only
and cannot satisfy native visual or dark-mode acceptance.
Full user-selectable light/dark/high-contrast appearance profiles remain
RFC-BPR-0006 scope.
This RFC-BPR-0002 window remains an interim provider/read-model integration
surface. RFC-BPR-0005 owns its integration into the complete native workbench
and virtualized review layout.

## Result

- Replaced the functional integration layout with a compact responsive
  connection, locator, status, pull-request, description, and conversation
  hierarchy. Secondary capability/rate and exact commit-identity facts remain
  available through keyboard-operable disclosures.
- Added explicit disconnected, connecting, empty, loading, error, cancelled,
  loaded, refreshing, loaded-error, and loaded-cancelled presentation states.
  A new target uses an empty activity/failure state; refresh retains the last
  accepted projection through loading, failure, or cancellation.
- Added a native-style conversation delegate that wraps literal multiline
  text without elision or clipping, while leaving background, selection, and
  focus painting to the active style.
- Preserved stable selection, keyboard focus, and user-controlled conversation
  expansion through accepted incremental refreshes and appearance changes.
- Kept the window palette, style, body font, and QSS unset. Qt palette, font,
  style, and theme events refresh derived title fonts, standard icons,
  delegate geometry, and responsive layout.
- Added one combined polite accessible status-plus-diagnostic announcement per
  published update, with identical repeats suppressed.
- Applied a conservative simplification pass without changing the verified
  behavior. GitHub Actions has no role.

## Evidence

- The scale-specific Qt cases pass at 1.0x, 1.25x, and 2.0x and directly cover
  all presentation states, disclosures, responsive layout, literal wrapping,
  retained and empty failure/cancellation paths, selection, focus, collapsed
  expansion, palette/font inheritance, and accessible announcements. Each
  scale produces minimum, loaded-success, disconnected, loading,
  first-load-error, session-cancelled, first-load-cancelled,
  retained-refresh-error, and retained-refresh-cancelled rasters under the
  ignored build tree; the 1.0x set was inspected directly.
- The standalone conversation property corpus remains green, including 3,000
  mixed updates, 5,000 permutations, both 50,000-row rotations, exact roles,
  minimal moves, persistent indexes, and zero resets.
- The final macOS `dev` matrix passed 44/44 in 70.17 seconds and `core-only`
  passed 17/17 in 17.29 seconds. The three scale cases plus property corpus
  passed 4/4 in 41.12 seconds under
  AddressSanitizer/UndefinedBehaviorSanitizer.
- The native macOS 15.7.7 arm64 application loaded Cocoa and QMacStyle,
  inherited the real system-default Dark appearance, and rendered the complete
  GitHub.com fixture with `PUBLIC_PATH_OK`,
  `GITHUB_FIXTURE_OK:github.com`, and `RENDER_OK`. A controlled loaded-process
  Dark-to-Light color-scheme response retained QMacStyle, and settled
  13-to-17-point font growth remained legible. An ad hoc
  QMacStyle-to-QFusionStyle probe is excluded from visual acceptance because
  its inherited dark control palette did not provide acceptable input
  contrast.
- In an Ubuntu 26.04 x86_64 full-system QEMU guest, a fresh Release
  tests-enabled GCC 15.2 build completed all 84 build steps. The focused
  architecture, documentation, three-scale, property, both public-fixture,
  and application-render matrix passed 11/11 in 202.34 seconds. GCC rejected
  three out-of-order designated initializers in new test fixtures; they were
  put into canonical declaration order and the same fresh guest build then
  completed without a production portability change. Linux normal, minimum,
  loading, and retained-error rasters were inspected directly.
- The controlled responder probes do not substitute for a real system-Light
  startup and live operating-system appearance change. That final native
  acceptance remains open; this card must not move to `done/` until it is
  directly observed and the original desktop setting is restored.
- Windows target-native verification remains BPR-0036 and is nonblocking for
  this available-platform implementation and test work.
