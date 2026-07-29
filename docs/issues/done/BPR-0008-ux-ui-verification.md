---
id: BPR-0008
title: Verify the rendered RFC-BPR-0001 native UI
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0006, BPR-0007]
completed: 2026-07-29
---

# Verify the rendered RFC-BPR-0001 native UI

Exercise the actual application with deterministic Qt automation and rendered
inspection. Verify native empty-state content, command translation, projection
updates, keyboard/focus behavior, accessibility labels, and supported macOS
launch behavior. Record Windows/Linux CI coverage separately.

## Result

- Added a public `--render-evidence` path that first exercises the real refresh
  workflow, then captures the actual application window and fails if either
  behavior or image persistence fails.
- Added CTest render coverage at 1.25× scale and verified 640×420 and 960×640
  logical layouts.
- Directly verified primary-action visibility, keyboard focus, accessible
  names/descriptions, positive central geometry, and zero full resets.
- Inspected both the deterministic offscreen rendering and the native macOS
  rendering. Both show the toolbar action, empty state, and non-secret
  version/platform/configuration facts without clipping.
- Added pinned three-platform CI jobs for Ubuntu 24.04, Windows 2025, and macOS
  15 using Qt 6.11.1, plus a separate Qt-free core job.

## Evidence

```text
ctest --preset dev
8/8 tests passed

build/dev/src/app/better_pull_review \
  --render-evidence build/dev/native-foundation-window.png
PUBLIC_PATH_OK
RENDER_OK
```

Local inspection artifacts remain under ignored `build/dev/` and are not
checked in as implementation proof. The workflow is configured but has not
run on the unpushed branch, so Windows and Linux remain explicitly unverified
for RFC closeout.
