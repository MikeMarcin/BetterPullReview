# BPR-0168/BPR-0392 native notification delivery operator review

Status: **pending operator decision; not RFC closeout or live GitHub mutation approval**

This packet presents the one target-native human decision still required by
BPR-0168 and BPR-0392. It does not infer Notification Center display from the
submitted request, grant permission by automation, authorize a live provider
mutation, or close RFC-BPR-0010.

## What the earlier course established, and what it did not

The v005 attended course reached a real macOS banner, its activation returned
through the tray signal, the exact landing row was selected, and the process
exited zero with all required markers. Its recorded log has SHA-256
`307ec1f34a307a8d24e07d6165015ee1d40c5e8c3b70e8059301c5c911fbe9d4`.
That evidence remains valid for the executable v005 bound.

The later owner response was:

> I didn't see any notice a notification banner but I wasnt looking.

That is neither approval nor rejection of the banner-content criterion. The
owner explicitly was not observing what macOS drew, so the required firsthand
banner text and absence of extra private content remain unrecorded.

The authorized BPR-0409/BPR-0410/BPR-0334 batch subsequently relinked the
executable. v006 therefore supersedes v005 as the live final-head question and
offers a fresh exact run against the new bytes. It preserves the old exit-zero
course as historical evidence rather than stretching it across either the
missing human observation or the changed executable identity.

## Decision requested

Review and either accept or reject all of these facts as one decision:

1. The exact macOS executable below starts from the quiet first accepted head,
   enrolls `octo-org/review-repository` through the shipped observer-rule
   action, and drives the shipped Refresh action to the second accepted head.
2. macOS displays one banner whose title is exactly
   `octo-org/review-repository #17` and whose body is exactly
   `The code moved after you reviewed it.` The owner personally reads those
   visible words. The banner contains no source path, source line, comment body,
   private note, draft, or credential.
3. Clicking that banner causes the real tray `messageClicked` signal to return
   through the notifier and selects and focuses exactly
   `github.com / octo-reviewer / octo-org/review-repository #17` in the local
   landing queue.
4. The terminal reports the exact bundle identity, quiet baseline, request,
   native activation, landing selection, and clean shutdown. It exits zero and
   emits neither `QSystemTrayIcon::setVisible: No Icon set` nor any
   `NATIVE_NOTIFICATION_REFUSED` marker.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28` | 51,383,520 |
| `docs/issues/60-done/BPR-0251-compose-attended-native-notification-course.md` | `sha256:9576aef7f125a62d88c0f1e7928357906d47d7a1a8a8b48617f09945f4110be6` | 5,183 |

The artifact digests, rather than the mutable repository branch head, are the
packet authority. BPR-0168 and BPR-0392 are mutable decision targets and are
deliberately not digest authorities: recording the verdict must be able to
update those cards without invalidating the human statement.

Before starting, verify the bound inputs:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  docs/issues/60-done/BPR-0251-compose-attended-native-notification-course.md
```

Do not proceed unless both digests exactly match the table. The deterministic
`application_native_notification_attended_contract` preflight passed against
these exact executable bytes. It proves the shipped rule, Refresh, request,
bundle-identity, and activation/selection wires; it cannot see or click a real
banner.

## Operator procedure

1. In System Settings, leave Notifications available for BetterPullReview. If
   macOS prompts, grant permission manually. Do not pre-authorize it with a
   script. The Settings entry reads **BetterPullReview (tests)** under bundle
   identity `io.github.MikeMarcin.BetterPullReview.tests`; that separation from
   the shipped application is deliberate.
2. From the repository root, run this exact block and keep both Terminal and
   the workbench visible:

   ```sh
   cd /Users/jaeger/Documents/GitHub/BetterPullReview
   set -o pipefail
   bpr_notification_root=$(mktemp -d /tmp/bpr-native-notification-v006.XXXXXX)
   bpr_notification_log=$(mktemp /tmp/bpr-0168-native-notification-v006.XXXXXX)
   printf 'BPR_NOTIFICATION_ARTIFACT_ROOT:%s\n' "$bpr_notification_root" \
     | tee "$bpr_notification_log"
   ./build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
     --github-fixture github-com \
     --epoch-fixture-root "$bpr_notification_root" \
     --epoch-fixture-phase seed \
     --native-notification-attended \
     --automated-run-budget-ms 300000 \
     2>&1 | tee -a "$bpr_notification_log"
   bpr_notification_status=$pipestatus[1]
   printf 'BPR_NOTIFICATION_EXIT_STATUS:%s\n' "$bpr_notification_status" \
     | tee -a "$bpr_notification_log"
   printf 'BPR_NOTIFICATION_LOG:%s\n' "$bpr_notification_log" \
     | tee -a "$bpr_notification_log"
   ```

3. Wait for `NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK`. Look at the actual
   macOS banner and read its title and body before clicking it. Compare the
   visible banner—not merely the terminal's requested payload—with fact 2.
4. Click that banner itself; do not click a test helper or select the queue row
   manually. Confirm that the landing row is selected and focused.
5. Return the complete contents of the path printed by
   `BPR_NOTIFICATION_LOG`, including the artifact-root, process output,
   exit-status, and final log-path lines, followed by the exact statement below.

The notification has a real display window. If it auto-dismisses before you can
read and click it, preserve and return that attempt's complete log, then run the
same block again: `mktemp` creates a fresh fixture root and log, so no attempt
is overwritten.
The approval statement applies only after one current-digest run was personally
read, clicked, and completed. Do not run offscreen, automate the click, script
permission, edit a log, reuse an earlier packet's artifact path, or run against
rebuilt bytes under this packet.

## Required log audit

The approving run must exit zero and contain all of these exact markers:

```text
NATIVE_NOTIFICATION_BUNDLE_IDENTITY:io.github.MikeMarcin.BetterPullReview.tests
NATIVE_NOTIFICATION_QUIET_BASELINE_OK
NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK
NATIVE_NOTIFICATION_PAYLOAD:octo-org/review-repository #17|The code moved after you reviewed it.
NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK
NATIVE_NOTIFICATION_TRAY_SIGNAL_OK
NATIVE_NOTIFICATION_ACTIVATION_OK
NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK
NATIVE_NOTIFICATION_SHUTDOWN_OK
NATIVE_NOTIFICATION_ATTENDED_RUN_OK
BPR_NOTIFICATION_EXIT_STATUS:0
```

It must not contain `QSystemTrayIcon::setVisible: No Icon set` or any
`NATIVE_NOTIFICATION_REFUSED` marker. These terminal facts establish the
request and activation course; only the owner's firsthand statement below
establishes what the native banner visibly said and omitted.

## Operator response

To approve, paste the complete v006 log or logs and reply with this exact
statement:

> I approve BPR-0168/BPR-0392 native notification delivery for executable sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28 and completed implementation record sha256:9576aef7f125a62d88c0f1e7928357906d47d7a1a8a8b48617f09945f4110be6: the run reported NATIVE_NOTIFICATION_BUNDLE_IDENTITY:io.github.MikeMarcin.BetterPullReview.tests, NATIVE_NOTIFICATION_QUIET_BASELINE_OK and NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK; I personally saw and read the macOS banner “octo-org/review-repository #17” / “The code moved after you reviewed it.” with no source path, source line, comment body, private note, draft, or credential; I clicked that banner, and BetterPullReview selected and focused github.com / octo-reviewer / octo-org/review-repository #17 with NATIVE_NOTIFICATION_ACTIVATION_OK and NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK; the pipeline exited zero with NATIVE_NOTIFICATION_SHUTDOWN_OK and NATIVE_NOTIFICATION_ATTENDED_RUN_OK, without QSystemTrayIcon::setVisible: No Icon set or any NATIVE_NOTIFICATION_REFUSED marker.

To reject, identify the visible banner, click, selection, focus, warning, or
marker fact that failed, and return every complete unedited log. “No banner
appeared” is a specific result rather than approval. Rejection leaves both
cards pending and makes no RFC closeout claim.
