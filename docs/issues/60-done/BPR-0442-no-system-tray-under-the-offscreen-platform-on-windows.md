---
schema: "implement-rfc.card.v1"
id: "BPR-0442"
title: "No system tray under the offscreen platform on Windows"
status: "Done"
rfcs: ["RFC-BPR-0003"]
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

## Measured and repaired — 2026-08-23

The measurement: `QSystemTrayIcon::isSystemTrayAvailable()` is false under
the offscreen platform on Windows — the offscreen integration offers no
platform tray there, where Linux answers through the session bus — and the
sink's guard is the right product behaviour: a tray icon the platform
cannot show is not a notification surface. The contract's failing course
was the one course that composed through that guard:
`CaptureSystemNotificationDisplay` is `probe && !request-cut`, so the
request-cut course alone asked the platform for a tray and was refused
with `platform_tray_not_composed`.

The repair is the card's first option, scoped to the fixture: under
`BPR_BUILD_TESTING`, a fixture-driven run composes the captured display
boundary whenever the platform truthfully reports no tray. The courses
that leave capture off assert what reaches the sink — a cut request wire
delivers nothing — not the native display mechanics, and the captured
boundary counts exactly that. Where a tray exists (Linux offscreen, a real
Windows session), the condition is false and nothing changes; production
composition is untouched.

### Evidence

| claim | command | result |
|---|---|---|
| the contract passes on Windows as registered | `ctest --preset windows-dev -R '^application_native_notification_attended_contract$'` | Passed, 7.5 s, all three courses (`attended-1.log`, 2026-08-23), alongside the composition and round-trip contracts that pin the changed file |
| macOS and Linux are unchanged | the fallback triggers only when `isSystemTrayAvailable()` is false during a fixture run; where the platform has a tray, the previous path is taken verbatim | by construction; the Linux lane's next run is the check |
| production is unchanged | the fallback lives inside `BPR_BUILD_TESTING` and requires `Fixture != nullptr` | by construction |

Implemented by the integrator.

## Independent review — 2026-08-23

Reviewed read-only by `rfc-reviewer` with the session's other
capability-critical changes: confirmed sound, no blocking findings. The
review's non-blocking risks and verification gaps are recorded on
[[BPR-0036]]'s procedure and [[BPR-0053]]'s ledger.

One risk from that review is this card's own: the fixture fallback keys on
the fixture pointer, not the course, so a manual
`--native-notification-attended` run on a tray-less host now composes the
captured sink and waits out its budget where it used to refuse fast and by
name with `platform_tray_not_composed`. The automated contract is
unaffected (its three courses all pass `--native-notification-probe`).
Narrowing the guard to non-attended fixture options would restore the
named refusal.
