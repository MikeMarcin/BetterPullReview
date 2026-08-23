---
id: BPR-0442
title: No system tray under the offscreen platform on Windows
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# No system tray under the offscreen platform on Windows

`application_native_notification_attended_contract` fails on the Windows
host, uncontained as registered, at `e807a72`:

```
native notification request-cut failed:
NATIVE_NOTIFICATION_REFUSED:platform_tray_not_composed
```

`schedule_native_notification_run` (`src/app/main.cpp:4982`) looks for the
`QSystemTrayIcon` named `reviewQueueNotificationTray` under the window and
refuses the run when there is none. The shipped sink
(`src/adapters/qt/src/system_tray_notification_sink.cpp:86`) creates that
icon only when `QSystemTrayIcon::isSystemTrayAvailable()` says so, which
is the right product behaviour — a notification sink without a tray has
nowhere to display — and under `QT_QPA_PLATFORM=offscreen` on Windows it
says no. The same contract passes on the Linux lane under the same
platform, so what differs is what Qt's offscreen integration answers for
the tray on each platform, not the sink.

The other two sink constructors — the one the `CaptureSystemNotificationDisplay`
branch uses and the one handed an existing icon — create or adopt the icon
unconditionally, so the attended contract is the one path that asks the
platform.

## Required final state

- The contract's Windows answer is measured: what
  `QSystemTrayIcon::isSystemTrayAvailable()` returns under the offscreen
  platform here, and why it differs from Linux (the offscreen plugin's
  tray capability, or the Windows non-platform tray needing a native
  window).
- Either the contract composes the attended run through a boundary that
  does not depend on the platform's tray answer — the way the other
  native-notification contracts already do — or the sink's availability
  check is corrected where it is wrong; a sink that creates a tray icon the
  platform cannot show is not a repair.
- `application_native_notification_attended_contract` passes on Windows as
  registered and is unchanged on macOS and Linux.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^application_native_notification_attended_contract$'
```
