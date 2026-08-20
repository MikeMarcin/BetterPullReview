# BPR-0168 native notification delivery operator review

Status: **superseded by
[`native-notification-delivery-v005`](../native-notification-delivery-v005/operator-packet.md);
do not run or approve.**

This packet is preserved as the superseded v003 procedure. It was never run and
the executable digest it binds no longer exists; v004 rebound the same course to
a rebuilt executable. The bundled-application course v003 introduced is the one
still in force. No response to this version can satisfy [[BPR-0168]].

This packet presents the one target-native human decision required by BPR-0168.
It does not infer Notification Center display from the submitted request, grant
permission by automation, authorize a live provider mutation, or close
RFC-BPR-0010.

## Why v003 exists, and why v002 could not have succeeded

v002 was attempted and produced no banner. The owner then checked System
Settings → Notifications directly and found **no BetterPullReview entry at all**.

[[BPR-0392]] established the mechanism: `build/dev/src/app/better_pull_review`
was a bare Mach-O with no bundle and no `Info.plist`, and macOS Notification
Center registers applications by bundle identity. An unbundled process cannot
request authorization, never gains a Settings entry, and its delivery requests go
nowhere. So the v002 procedure asked a person to click a banner no process
identity could have produced. Attendance was never what was missing.

The tests-enabled macOS build is now an application bundle with its own identity,
deliberately distinct from the shipped application's so that a test build cannot
grant or consume its Notification Center permission:

| | Value |
|---|---|
| Bundle | `better_pull_review.app` |
| `CFBundleName` | `BetterPullReview (tests)` |
| `CFBundleIdentifier` | `io.github.MikeMarcin.BetterPullReview.tests` |

**The executable path has therefore changed** and is nested inside the bundle.
The course also now refuses to run without identity, reporting
`NATIVE_NOTIFICATION_REFUSED:bundle_identity_absent` before it arms anything, and
prints its identity so the contract can require the exact expected one.

Two things this does not establish, stated plainly because they bear on whether
the procedure can succeed: the bundle is **not signed or notarized**, and
`macdeployqt` does not run. Bundle identity is necessary for Notification Center
registration; whether macOS then chooses to display a banner for an unsigned
local build is exactly what only a person looking at the screen can answer. If no
banner appears and no Settings entry exists even now, that is a real result and
should be reported as a rejection rather than retried.

## Decision requested

Review and either accept or reject all of these facts as one decision:

1. The exact macOS executable below starts from the quiet first accepted head,
   enrolls `octo-org/review-repository` through the shipped observer-rule
   action, and drives the shipped Refresh action to the second accepted head.
2. macOS displays one banner whose title is exactly
   `octo-org/review-repository #17` and whose body is exactly
   `The code moved after you reviewed it.` The banner contains no source path,
   source line, comment body, private note, draft, or credential.
3. Clicking that banner causes the real tray `messageClicked` signal to return
   through the notifier and selects and focuses exactly
   `github.com / octo-reviewer / octo-org/review-repository #17` in the local
   landing queue.
4. The terminal reports the bundle identity, the quiet baseline, exact request,
   native activation, exact landing selection, and clean shutdown. It does not
   emit `QSystemTrayIcon::setVisible: No Icon set` or a
   `NATIVE_NOTIFICATION_REFUSED` marker.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `sha256:6ad6ad39b7c7b593b222c2c7c20ee567e78331f3034f73ce8d48a6f201bf8753` | 51,043,648 |
| `docs/issues/60-done/BPR-0251-compose-attended-native-notification-course.md` | `sha256:9576aef7f125a62d88c0f1e7928357906d47d7a1a8a8b48617f09945f4110be6` | 5,183 |

The bound artifact digests, rather than the mutable repository branch head, are
the packet authority. BPR-0168 and BPR-0392 are the mutable decision targets and
are deliberately not digest authorities: recording this run must update those
cards without invalidating the human statement.

Before starting, verify the bound inputs:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  docs/issues/60-done/BPR-0251-compose-attended-native-notification-course.md
```

Do not proceed unless both digests exactly match the table.

The deterministic preflight is
`application_native_notification_attended_contract`. It runs the shipped
executable through the same rule and Refresh course, substitutes only native
display for the positive and activation-cut runs, and uses the real Cocoa tray
constructor for the request-cut run. It now also requires the exact bundle
identifier, which is the one precondition it previously could not check and the
one that was false. It still cannot prove facts 2 or 3; the owner must see and
click the banner.

## Operator procedure

1. In System Settings, leave Notifications available for BetterPullReview. The
   app has a new bundle identity, so macOS should treat this as a first launch and
   may prompt. If it asks, grant permission manually. Do not pre-authorize it with
   a script. Note that the Settings entry will read **BetterPullReview (tests)**,
   which is deliberate.
2. From the repository root, run the exact executable and keep both the terminal
   and workbench visible:

   ```sh
   set -o pipefail
   bpr_notification_root=$(mktemp -d /tmp/bpr-native-notification-v003.XXXXXX)
   bpr_notification_log=/tmp/bpr-0168-native-notification-v003.log
   ./build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
     --github-fixture github-com \
     --epoch-fixture-root "$bpr_notification_root" \
     --epoch-fixture-phase seed \
     --native-notification-attended \
     --automated-run-budget-ms 300000 \
     2>&1 | tee "$bpr_notification_log"
   ```

3. Wait for `NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK`. Read the actual
   macOS banner and compare it with fact 2. Click that banner itself; do not
   click a test helper or select the queue row manually.
4. Confirm the landing row is selected and focused, the pipeline exits zero,
   and the terminal contains all of these markers:

   ```text
   NATIVE_NOTIFICATION_BUNDLE_IDENTITY:io.github.MikeMarcin.BetterPullReview.tests
   NATIVE_NOTIFICATION_QUIET_BASELINE_OK
   NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK
   NATIVE_NOTIFICATION_PAYLOAD:octo-org/review-repository #17|The code moved after you reviewed it.
   NATIVE_NOTIFICATION_ACTIVATION_OK
   NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK
   NATIVE_NOTIFICATION_SHUTDOWN_OK
   NATIVE_NOTIFICATION_ATTENDED_RUN_OK
   ```

Do not run this under `QT_QPA_PLATFORM=offscreen`, automate the click, script the
permission grant, reuse a v001 or v002 artifact path, or rerun against a rebuilt
binary under this packet.

## Operator response

To approve, reply with this exact statement:

> I approve BPR-0168 native notification delivery for executable sha256:6ad6ad39b7c7b593b222c2c7c20ee567e78331f3034f73ce8d48a6f201bf8753 and completed implementation record sha256:9576aef7f125a62d88c0f1e7928357906d47d7a1a8a8b48617f09945f4110be6: the run reported NATIVE_NOTIFICATION_BUNDLE_IDENTITY:io.github.MikeMarcin.BetterPullReview.tests, NATIVE_NOTIFICATION_QUIET_BASELINE_OK and NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK; I saw the macOS banner “octo-org/review-repository #17” / “The code moved after you reviewed it.” with no source path, source line, comment body, private note, draft, or credential; I clicked that banner, and BetterPullReview selected and focused github.com / octo-reviewer / octo-org/review-repository #17 with NATIVE_NOTIFICATION_ACTIVATION_OK and NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK; the pipeline exited zero with NATIVE_NOTIFICATION_SHUTDOWN_OK and NATIVE_NOTIFICATION_ATTENDED_RUN_OK, without QSystemTrayIcon::setVisible: No Icon set or any NATIVE_NOTIFICATION_REFUSED marker.

To reject, identify the banner, click, selection, focus, warning, or marker fact
that failed, and return the complete unedited log. **"No banner appeared and no
Settings entry exists" is a specific and useful rejection**, not a retry
condition: it would mean bundle identity is necessary but not sufficient, and
signing is the next question rather than attendance.

Rejection leaves BPR-0168 pending and makes no RFC closeout claim. This response
applies only to BPR-0168's target-native display and activation gate. BPR-0169's
live GitHub non-mutation check and every RFC closeout remain separate gates.
