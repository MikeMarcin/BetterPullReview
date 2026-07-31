---
id: BPR-0031
title: Verify RFC-BPR-0002 rendered UX and accessibility
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0028, BPR-0029, BPR-0030]
completed: 2026-07-30
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
- Added a test-build-only real-application appearance probe. It admits only a
  loaded GitHub.com fixture under the native Cocoa platform in system Light,
  retains the initial render in memory, and requires both Qt's color-scheme
  signal and an independent read-only observation of the macOS current-user
  global appearance changing from Light to Dark. It accepts only a settled
  Dark palette that preserves style, platform, selection, focus, collapse
  state, and zero model/window resets. The two captures must have equal
  geometry and device-pixel ratio but different pixels; only then are two
  fresh, non-aliased PNGs created and reloaded. Failed probes leave no render
  artifacts. The test seam never changes the operating-system appearance and
  is absent from tests-off production builds.
- Documented the owner-authorized native gate as a rollback-safe System Events
  protocol. It waits for both Light readiness markers before changing the
  desktop, requires every transition/state/pixel/render success marker, and
  restores the original appearance through an exit/signal trap.
- Completed that authorized live gate from an original system-Dark desktop.
  The public fixture reached independent Qt and global Light readiness before
  either image existed, then the same process observed the real operating
  system changing to Dark. It retained QMacStyle, platform, focus, selection,
  collapsed conversation state, model identity, and raster geometry; persisted
  and reloaded distinct Light and Dark pixels; shut down cleanly; and restored
  the original Dark setting.
- Applied a conservative simplification pass without changing the verified
  behavior. GitHub Actions has no role.

## Deferred final-workbench presentation obligation

Direct inspection of the native-Dark render confirms system appearance
inheritance, but it also confirms that this provider integration window is not
the final product surface. It remains dominated by connection and locator
forms, nested borders, mixed icon treatment, excess summary whitespace, weak
content hierarchy, and an equal-width content split that does not prioritize
the review task. It contains no central diff workbench.

When RFC-BPR-0005 reaches its normal planning slice, allocate a distinct final
workbench presentation-polish and native visual-acceptance card. That card must
inspect the real integrated workbench at supported widths and at 1.0x, 1.25x,
and 2.0x under native system Light and Dark. Acceptance must require coherent
hierarchy, spacing, density, typography, grouping, and icon treatment;
readable conversation and empty/loading/error states; visible keyboard focus;
and no clipping, overlap, or low-contrast controls. Deterministic geometry and
state assertions must be paired with direct native render inspection.
RFC-BPR-0006 retains custom System/Light/Dark/high-contrast profiles, syntax
colors, and configurable font/theme work.

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
- The final macOS `dev` matrix passed 45/45 in 63.26 seconds and `core-only`
  passed 17/17 in 15.38 seconds. The three scale cases plus property corpus
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
- The appearance-probe contract rejects incomplete arguments, GHES selection,
  identical and symlink-aliased destinations, existing and dangling-symlink
  destinations, and an offscreen platform without producing either render.
  Its focused CTest passed together with the unchanged GitHub.com public
  fixture and a clean tests-off Release composition scan. A direct native
  invocation under the current system Dark setting reported both Qt and global
  `Dark->Dark`, `cocoa`, and `QMacStyle`, then failed the signal, state,
  pixel-difference, and changed-render claims exactly as intended. It created
  no artifact and did not launder the existing Dark startup into the required
  Light-to-Dark evidence.
- In an Ubuntu 26.04 x86_64 full-system QEMU guest, a fresh Release
  tests-enabled GCC 15.2 build completed all 84 build steps. The focused
  architecture, documentation, three-scale, property, both public-fixture,
  and application-render matrix passed 11/11 in 202.34 seconds. GCC rejected
  three out-of-order designated initializers in new test fixtures; they were
  put into canonical declaration order and the same fresh guest build then
  completed without a production portability change. Linux normal, minimum,
  loading, and retained-error rasters were inspected directly.
- The authorized live macOS probe reported every required exact marker:
  `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, `RENDER_OK`, both Light
  readiness markers, both `Light->Dark` transitions, `cocoa`, `QMacStyle`,
  and the signal, state, pixel-difference, and changed-render success markers.
  The 2200x1440 Light and Dark PNG SHA-256 values were respectively
  `43d2e14345d84ef64e276eb5153754c8a2c3c0d8d886e4398f744dccd978a0ce`
  and
  `49244d85e0b6cf604aee2f2cf9e1a860ccb8dab0b583193afa6e11aa8a05031d`.
  Direct inspection confirmed readable native contrast and the same loaded
  GitHub.com identity, selected collapsed conversation row, focus, and
  content. The shell recorded original `true` and restored `true`; the
  transient images and log remain local inspection evidence and are not
  checked in.
- Windows target-native verification remains BPR-0036 and is nonblocking for
  this available-platform implementation and test work.
