# BPR-0168 native notification delivery operator review

Status: **superseded by
[`native-notification-delivery-v005`](../native-notification-delivery-v005/operator-packet.md);
do not run or approve.**

This packet is preserved as the superseded v001 procedure. It was never run and
the dev executable it binds no longer exists at its recorded digest; v002
rebound the same course to a rebuilt executable. No response to this version can
satisfy [[BPR-0168]].

This packet presents the one target-native human decision required by BPR-0168.
It does not infer Notification Center display from the submitted request, grant
permission by automation, authorize a live provider mutation, or close
RFC-BPR-0010.

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
4. The terminal reports the quiet baseline, exact request, native activation,
   exact landing selection, and clean shutdown. It does not emit
   `QSystemTrayIcon::setVisible: No Icon set` or a
   `NATIVE_NOTIFICATION_REFUSED` marker.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review` | `sha256:3148de09f50cae85c375ad4df8fabf5c659e60e675b0b8e49acdfeed57c691b3` | 50,026,800 |
| `docs/issues/done/BPR-0251-compose-attended-native-notification-course.md` | `sha256:9576aef7f125a62d88c0f1e7928357906d47d7a1a8a8b48617f09945f4110be6` | 5,183 |

The deterministic preflight is
`application_native_notification_attended_contract`. It runs the shipped
executable through the same rule and Refresh course, substitutes only native
display for the positive and activation-cut runs, and uses the real Cocoa tray
constructor for the request-cut run. It rejects the exact no-icon warning and
lets process exit status, not a CTest success regex, decide the result. That
preflight cannot prove facts 2 or 3; the owner must see and click the banner.

## Operator procedure

1. In System Settings, leave Notifications available for BetterPullReview. If
   macOS asks during the run, grant permission manually. Do not pre-authorize it
   with a script.
2. From the repository root, run the exact executable and keep both the terminal
   and workbench visible:

   ```sh
   set -o pipefail
   bpr_notification_root=$(mktemp -d /tmp/bpr-native-notification-v001.XXXXXX)
   ./build/dev/src/app/better_pull_review \
     --github-fixture github-com \
     --epoch-fixture-root "$bpr_notification_root" \
     --epoch-fixture-phase seed \
     --native-notification-attended \
     --automated-run-budget-ms 300000 \
     2>&1 | tee /tmp/bpr-0168-native-notification-v001.log
   ```

3. Wait for `NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK`. Read the actual
   macOS banner and compare it with fact 2. Click that banner itself; do not
   click a test helper or select the queue row manually.
4. Confirm the landing row is selected and focused, the pipeline exits zero,
   and the terminal ends with all of these markers:

   ```text
   NATIVE_NOTIFICATION_QUIET_BASELINE_OK
   NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK
   NATIVE_NOTIFICATION_PAYLOAD:octo-org/review-repository #17|The code moved after you reviewed it.
   NATIVE_NOTIFICATION_ACTIVATION_OK
   NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK
   NATIVE_NOTIFICATION_SHUTDOWN_OK
   NATIVE_NOTIFICATION_ATTENDED_RUN_OK
   ```

## Operator response

To approve, reply with this exact statement:

> I approve BPR-0168 native notification delivery for executable sha256:3148de09f50cae85c375ad4df8fabf5c659e60e675b0b8e49acdfeed57c691b3 and completed implementation record sha256:9576aef7f125a62d88c0f1e7928357906d47d7a1a8a8b48617f09945f4110be6: the shipped observer-rule and Refresh course reported NATIVE_NOTIFICATION_QUIET_BASELINE_OK and NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK; I saw the macOS banner “octo-org/review-repository #17” / “The code moved after you reviewed it.” with no source path, source line, comment body, private note, draft, or credential; I clicked that banner, and BetterPullReview selected and focused github.com / octo-reviewer / octo-org/review-repository #17 with NATIVE_NOTIFICATION_ACTIVATION_OK and NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK; the pipeline exited zero with NATIVE_NOTIFICATION_SHUTDOWN_OK and NATIVE_NOTIFICATION_ATTENDED_RUN_OK, without QSystemTrayIcon::setVisible: No Icon set or any NATIVE_NOTIFICATION_REFUSED marker.

To reject, identify the banner, click, selection, focus, warning, or marker fact
that failed. Rejection leaves BPR-0168 pending and makes no RFC closeout claim.

This response applies only to BPR-0168's target-native display and activation
gate. BPR-0169's live GitHub non-mutation check and every RFC closeout remain
separate gates.
