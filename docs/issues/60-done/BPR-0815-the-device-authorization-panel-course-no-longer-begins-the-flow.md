---
schema: "implement-rfc.card.v1"
id: "BPR-0815"
title: "The device authorization panel course no longer begins the flow"
status: "Done"
rfcs: ["RFC-BPR-0016", "RFC-BPR-0032"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"BPR-0807 focused selectors; confirmed at a clean worktree","found_at":"53a0a7477264c71423b11fa3bc85d2e668b389b4","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"application_device_authorization_panel_course and its negative twin pass on macOS, and the cause is named on this card","outcome":"pass","revision":"9767145d304bb0d20016fcf6b2ba113a1ca9ca9d","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -R panel_course|panel_absent at the integrated merge 9767145d: both pass (5.97 s, 5.43 s), build/r2-logs/dsp2-int1-ctest.log. In the slice worktree at a445866d plus the fix: 2/2 in four runs (dsp2-0815-fix-course-run{1,2}.log, dsp2-0815-final-course-run{1,2}.log). Negative controls planted in begin_selected_sign_in (open Appearance; open no Settings) fail the course with PANEL_BEGAN:false (dsp2-0815-negctl-*.log). Cause on the card: first bad 826fb871 (BPR-0705); the course read the window's hidden settingsWindow child instead of the presented profile Settings window; product unchanged; fix 67691f81."}},{"criterion":"qt_device_authorization_scale_* and qt_composed_github_scale_100 still pass","outcome":"pass","revision":"9767145d304bb0d20016fcf6b2ba113a1ca9ca9d","evidence":{"kind":"command","detail":"qt_device_authorization_scale_100/125/200 and qt_composed_github_scale_100 pass 4/4 at 9767145d (build/r2-logs/dsp2-int1-ctest.log), and 4/4 in the slice (dsp2-0815-qt-acceptance.log)."}}]
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

## Cause and fix (dsp-002, run-20260916-stations)

**The course broke. The product did not.** The first bad revision is
`826fb871` ("Finish [[BPR-0705]]", one Settings window per profile under
RFC-BPR-0024). Its parent `061dfcdf` passes the course and its twin;
`826fb871` fails with `DEVICE_FLOW_PANEL_BEGAN:false`
(`build/r2-logs/dsp2-0815-bisect-061dfcdf.log`, `dsp2-0815-bisect-826fb871.log`).
The four candidates this card named all came later. The
BPR-0771/0773/0774 records already show this course red at `53de5f35`.

Mechanism: since BPR-0705, Home's Sign in mounts the window's Settings
pages into the profile's one parentless Settings shell, shows it, and
presses the device panel's begin. The window's own `settingsWindow` child
stays hidden. `begin_through_device_panel` in `src/app/main.cpp` looked
that hidden child up by name, so it always answered false.
An instrumented run at `a445866d` showed `childVisible=0` while
`aWindow.settings()` was visible and held the panel. The flow still reached
`DEVICE_FLOW_STAGE:authorized`, with one enrollment and one vault store
(`dsp2-0815-debug-a445866d.log`, `dsp2-0815-repro-a445866d.log`).

The gesture did not move, so no RFC-BPR-0032 decision is needed. A person
still begins the device flow from Home, as RFC-BPR-0032's first-launch text
says: "opens Accounts & hosts with the device flow begun". Only the course's
reading of where Settings appears changed. Commit `67691f81` makes the course ask
`aWindow.settings()`, and require the Accounts & hosts page with the pressed
begin button on it. Pass expressions, budgets and the isolation seam are
unchanged; the negative twin is untouched.

Not investigated here: `application_device_authorization_ghes_refusal`,
which BPR-0773/0774 also recorded red. It uses a different helper,
`begin_through_ghes_device_panel`, which has no Settings check.
