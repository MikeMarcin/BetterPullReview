---
schema: "implement-rfc.card.v1"
id: "BPR-0815"
title: "The device authorization panel course no longer begins the flow"
status: "In Progress"
rfcs: ["RFC-BPR-0016", "RFC-BPR-0032"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"BPR-0807 focused selectors; confirmed at a clean worktree","found_at":"53a0a7477264c71423b11fa3bc85d2e668b389b4","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0815: The device authorization panel course no longer begins the flow

## Defect

`application_device_authorization_panel_course`
(`tests/end_to_end/CMakeLists.txt:631`) fails on macOS. The run was a clean
scratch worktree at `53a0a747`, before [[BPR-0807]] and [[BPR-0808]], with a
fresh dev build. It failed 2 of 2 runs (6.29 s and 5.51 s), reporting:

    DEVICE_FLOW_PANEL_BEGAN:false
    DEVICE_FLOW_PANEL_COMPOSED:true

The course's pass expression needs `DEVICE_FLOW_PANEL_BEGAN:true`. The
registration comment says what that marker protects: "a window that stopped
mounting the panel, or a composition that stopped wiring it, reports
`DEVICE_FLOW_PANEL_BEGAN:false`". Composition still reports the panel
composed, so the likelier break is the window gesture that begins the flow.
The marker is printed at `src/app/main.cpp:3956`. BPR-0807's slice owner traced
it to the Home "Sign in to GitHub" route. Recent changes to that surface
include:

- `2729b93a` (Home state panels)
- `aee19b26` and `51c443d2` (server cards)
- `b1f89533` (run-time test boundary)

RFC-BPR-0016 closed Auto-Verified with this course green, so this is a
regression against a closed RFC's evidence. The architecture gate had been
failing at its first finding, and the nested worktree had been breaking
selectors, which is probably why nobody noticed.

## Scope

- Find the first bad revision and the mechanism: the panel is no longer
  mounted, the course drives a gesture that moved, or the begin edge is no
  longer wired.
- Fix the product if a person can no longer begin the device flow from the
  panel. Update the course only if the gesture legitimately moved under an
  accepted RFC-BPR-0032 decision, and keep its negative twin able to catch an
  unwired panel.

## Acceptance

- `application_device_authorization_panel_course` and its negative twin
  (the registration below it) pass on macOS, and the cause is named on this
  card.
- `qt_device_authorization_scale_*` and `qt_composed_github_scale_100` still
  pass.
