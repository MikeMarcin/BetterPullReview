---
schema: "implement-rfc.card.v1"
id: "BPR-0861"
title: "The native notification probe cannot find the tray icon since the runtime owns the sink"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; reproduced on macOS by dsp-004; scouted","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0861: The native notification probe cannot find the tray icon since the runtime owns the sink

## Defect

`application_native_notification_attended_contract` fails with
`NATIVE_NOTIFICATION_REFUSED:platform_tray_not_composed` on Linux at
`a1864fa2` and on macOS at `20d99914`, in about 1 s.

`schedule_native_notification_run` (`src/app/main.cpp` ~7293-7299) looks up
the tray with
`aApplication.window().findChild<QSystemTrayIcon*>("reviewQueueNotificationTray")`.
The sink creates its icon as its own child
(`system_tray_notification_sink.cpp:88`).

`826fb871` ("Finish BPR-0705", 2026-09-08) moved the sink into the profile
runtime with a null parent (`github_application.cpp` ~4774-4785). Since then,
no lookup scoped to the window can find the icon, on any platform. The scout's
confidence is high.

This is not the old environment limit. [[BPR-0442]]'s fixture fallback had made
the row pass on all three platforms. [[BPR-0843]]'s note that calls this
failure "pre-existing, the WSLg limit" misattributes it.

## Scope

- Let the probe reach the runtime's tray through the sink or the fixture access
  surface, not through `window().findChild`. Keep the fallback for a platform
  with no tray.
- Confirm that the product still presents notifications with the runtime-owned
  sink. If it does not, fix the product.
- A `src/app` change reseals the round-trip pins with a recorded review.

## Acceptance

- The row passes on macOS and Linux, and on Windows if it is registered there.
- The card names whether any product notification behaviour had regressed.
