---
schema: "implement-rfc.card.v1"
id: "BPR-0974"
title: "Dispose Windows Defender finding and complete native Settings qualification"
status: "Blocked"
rfcs: []
depends_on: ["BPR-0730"]
priority: "P1"
platforms: ["win"]
blocked_by: "security-owner-disposition"
metadata: {"origin_rfc":"RFC-BPR-0018","origin_card":"BPR-0593","mandate_revision":"r2","disposition":"owner-deferred standalone follow-up; not an RFC0018 closeout dependency","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","transferred_criteria":["AC-01 Windows full native menu/shortcut/window-lifetime course","AC-11 current Windows inventory refresh"],"related_cards":["BPR-0960","BPR-0962"]}
---

# Dispose Windows Defender finding and complete native Settings qualification

## Owner transfer, distinct from observations

Owner amendment r2 to run `rfc-0018-0026-closeout-20260920` explicitly transfers
the remaining Windows security disposition, full Settings native course and
current inventory refresh from RFC-0018 terminal acceptance. This standalone
card remains **Blocked**, not accepted. The
[amended RFC](../../rfcs/RFC-BPR-0018-robust-settings-center.md#owner-amendment-r2--2026-09-21)
and [native evidence](../../verification/BPR-0593-native-settings-continuation.md)
retain the original scope and results. No detection is called a false positive.

## Exact finding and dependency

Configured native station `stn-a5155427` / `bpr-win`, reached through SSH
`aito-main-win`, is `C:/code/git/bpr-windows` on `windows-lane`. Its observed
HEAD is `b9cf4edb316dceaf8c7ea75f4c7321ce353651e8`; its existing dirty MASK lock
was preserved. The task-owned source was clean at the candidate above.

The MSVC 19.51 / Qt 6.11.2 x64 Release, tests-OFF, distribution-ON candidate
passed the View/Settings, Ctrl-comma, modeless, Escape and reopen prefix, then
stopped at a missing Profiles menu. The next launch found no image. Defender
quarantined these exact paths:

- `C:/Users/mikem/bpr0018-native-280167ac/installed/better_pull_review.exe`
- `C:/Users/mikem/bpr0018-native-280167ac/b/src/app/better_pull_review.exe`

Threat: `Trojan:Win32/Bearfoos.A!ml`, ID `2147731250`. Detection timestamps are
`2026-09-22T04:07:40.551Z` and `2026-09-22T04:09:45.239Z`. Defender reports
successful action; the staged image is absent. The previously generated SPDX
records SHA-256 `341e25b20111f48b7882d8c0a288de477cbcfd41eefc24b3bbbf861f1281e768`;
this is not a post-quarantine executable hash. `IsActive=false` and
`DidThreatExecute=false` do not establish that no earlier prefix ran.

The external prerequisite is a security-owner disposition of this exact finding
and a cleared, provenance-bound candidate authorized for execution. Do not
restore quarantine, add exclusions, disable security, rename/rebuild around the
detection, or execute a different image as a substitute. A vendor submission or
other external disclosure requires its own authorization. The missing-menu
observation remains unclassified while the intervention overlaps the course.
BPR-0960 and BPR-0962 own broader native release/security qualification; their
full closure is not a fabricated dependency of this narrowly scoped follow-up.
BPR-0730 is the already-Done baseline inventory prerequisite.

## Acceptance

- AC1: Record the authorized security disposition with exact source, build,
  dependency provenance, detection identity and cleared executable digest.
  Preserve any unresolved security finding; a successful rebuild alone is not
  a disposition or false-positive determination.
- AC2: On the cleared tests-disabled candidate in the actual ordinary-user
  Windows input desktop, complete the native menu/Ctrl-comma, modeless focus,
  singleton, Escape, same-profile sibling close/reopen and Quit course. Bind
  every observation to the image/PID and require terminal process exit 0.
  Classify and correct any reproduced product failure, including Profiles,
  without substituting Qt unit tests or the earlier prefix for the full course.
- AC3: Capture all seven current pages and compare their actionable names,
  enabled/check states and ordering to the comparable Mac/Linux evidence.
  Explicitly inventory later updater, CLI-capability and seven maintenance
  rows; document only real structural capability differences. Retain missing-
  control and changed-state negative controls in the comparison.
- AC4: Preserve private-profile isolation and unchanged live-station hashes;
  account for every app/observer/task, release the lease and retain no writer.
  No sign-in, provider mutation, updater installation or destructive maintenance
  is authorized by this Settings course. Full release qualification remains
  RFC-0038, not a consequence of this card's success.

## Ready commands and retained evidence

Only after AC1 supplies an authorized cleared image, rebind the task-local
launcher to that exact source/digest and a fresh output path. The retained guard
uses `station_lease.py` plus `qualification_guard.py`, a limited interactive
scheduled task, a 180-second deadline and waited cleanup. It currently names the
quarantined path and must **not** be run before clearance:

```sh
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_windows_guard.ps1
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/tests/scripts/settings_inventory_comparison_tests.py
```

The checked-in observer is
[`windows_settings_inventory.ps1`](../../../tests/scripts/windows_settings_inventory.ps1).
For a new current capture, extend the comparison with the exact newly recorded
path and platform capability assertions; the existing eleven passing tests
alone do not cover that new Windows candidate. The historical launch used the
existing `--render-evidence` flag without an output value for its private
profile, not an ordinary no-argument launch or a new application test hook.

The inert root `C:/Users/mikem/bpr0018-native-280167ac` and local
`build/rfc0018-windows-{fixed-course,fixed-course2,defender}.log`,
`build/rfc0018-windows-final-probe.json` retain the precise partial course and
security evidence. No Windows process is left running. Recheck station identity
and leases before any future execution; no old scheduled task is authorized to
continue. This card supplies no security verdict or native pass at creation.

## Station refresh — bpr-main-win, 2026-09-23

Rechecked from the `bpr-main-win` station, an agent session in the owner's
interactive desktop (session 1, `JaegAW\mikem`, not elevated) with the
`main` checkout, under the owner's mandate relayed by the Mac integrator (run
`bpr-main-win-20260923`). The card stays **Blocked** on
`security-owner-disposition`: AC1 is the security owner's decision, and AC2
and AC3 run only on the image that decision clears. This station can run the
course, but it cannot supply the disposition. It did not restore quarantine,
add an exclusion, change Defender, SmartScreen or any other setting, build a
tests-disabled image, or run one.

### Current record, read-only

Read from `Get-MpThreatDetection` and `Get-MpComputerStatus` at
2026-09-23T17:03:51Z:

- The finding is unchanged. Threat `2147731250` at the two paths and times
  above, both with `ActionSuccess` true. No other Defender detection names
  BetterPullReview on this host, including during the day's tests-enabled
  Debug `better_pull_review.exe` runs for [[BPR-0980]].
- Defender client 4.18.26080.4, signatures 1.459.343.0 (updated
  2026-09-22T20:50Z), real-time protection on, tamper protection on.
- `C:/Users/mikem/bpr0018-native-280167ac` is still inert: it holds no
  `better_pull_review.exe`, and no scheduled task names BetterPullReview.

### What the owner decides (AC1)

Any one of these would unblock the card:

1. Authorize a fresh tests-disabled Release image at a named `main` commit,
   built in a new task root and scanned on demand by Defender. If Defender
   does not detect it, authorize running that exact digest for AC2/AC3. The
   old finding stays recorded and is not called a false positive.
2. Authorize a false-positive submission of the quarantined sample to
   Microsoft. That is an external disclosure, and its answer becomes the
   disposition.
3. Leave the disposition to RFC-0038's release-security qualification
   ([[BPR-0960]]/[[BPR-0962]]), and keep this card Blocked until then.

### Ready path, revised

The commands under "Ready commands and retained evidence" run from a Mac
worktree through the SSH station `bpr-win` and a scheduled task. On
`bpr-main-win` a cleared image needs neither: run the course in the session
itself. Before
AC2's exit-0 Quit can be judged, two things in
[`windows_settings_inventory.ps1`](../../../tests/scripts/windows_settings_inventory.ps1)
need attention:

- Its `Focus` calls `SetForegroundWindow` from a background process. On this
  host the foreground lock timeout reads 2147483647 ms, so that call takes
  the foreground only when the caller already holds the foreground right.
  [`windows_profile_activation_course.ps1`](../../../tests/scripts/windows_profile_activation_course.ps1)
  shows a launcher method that works in session.
- It chooses File > Quit through UI Automation. [[BPR-0980]] found that this
  path destroys the window while the strip's stack-allocated menu is still
  executing. The Debug CRT asserts `_CrtIsValidHeapPointer(block)` in 4 of 4
  probes. In a tests-disabled Release image the same deletion corrupts the
  heap. Fix that defect, or drive Quit another way, before treating a
  nonzero Quit exit as a finding about Settings.

One observation bears on the unclassified missing-Profiles-menu stop. On the
tests-enabled Debug build at current `main`, the strip's Application menu
opens Profiles > New Window through the same posted click and UI Automation
path in every [[BPR-0980]] run. That is not AC2 evidence: it was not the
cleared tests-disabled image, and it was not the full course.
