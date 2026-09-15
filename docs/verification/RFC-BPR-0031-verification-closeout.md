# RFC-BPR-0030/0031 verification closeout

This live ledger covers dispatch `d14`, producer `rfc-integrator-d14`, mandate
revision 7, and the `verification-closeout` milestone of
`bpr-dif-integration-v1`. It consumes the accepted intake (`1c0d0c65`), runtime
foundation (`3036364c`), production art (`d7fc946c`), Home (`9c9f6cde`), and
context (`aa94a77a`) milestones without changing their evidence.

## Focused automated evidence

| Claim | Direct surface and result | Revision or exact binary |
|---|---|---|
| Tests-enabled/tests-disabled resource selection and core-only exclusion | `scripts/verify_dif_asset_boundaries.py` passed against `build/dev`, `build/app-release`, and `build/core-only`: each app graph contains exactly the ten accepted strips; four core-only artifacts and 453 product source files contain no prohibited Dif/runtime-art markers. The verifier explicitly does not establish a signed, notarized, installable, or certified package. | dev `sha256:ae238b6379bd2276e7ca1ab2672ba16827447980011ebb57db9fa533011ffed9`; app-release `sha256:fb3fc6725708bf202983b448d13a362a995915de74869b77414dbebad7e9296c` |
| Native motion current state | `better_pull_review_qt_motion_observer_tests --native-probe` returned `motion-native-probe:observed` on macOS 26.6.2 arm64. The registered injected observer course also passed. | unchanged observer source from `3036364c` |
| Actual composed memory/timer/paint work | The network-isolated `application_profile_multi_window_public_path` passed in 75.86 seconds with `BPR_DIF_INTEGRATION_REPORT_PATH` and production resources. It sampled Mach `TASK_VM_INFO`, the shared cache, full-motion work, reduced-motion quiescence, and comparison-surface counters. | source `62354ed6`; local report `build/dif-verification-closeout-d14/offscreen-report.json`, `sha256:bf90277975d1a9a817a1052ab0c8cfe84af668ed9c1a1a917f2f450994b7c40e` |
| Tests-disabled native application | The app-release executable ran through the normal private automated profile on Cocoa/QMacStyle and emitted `PUBLIC_PATH_OK`, `RENDER_OK`, `WORKBENCH_HOST_COMPOSED_OK`, and `DIF_PREFERENCES:shown:system:reduced`. Its dark System Home render contains accepted Dif with functional account/open controls. | app-release digest above; local render `build/dif-verification-closeout-d14/release-native/home.png`, `sha256:a7572475715f58b5711beafc87dec24d0bc782c31dbd9a3c58a4b62e080c6f58` |
| Maintainer documentation and package inventory | Live product, architecture, appearance, security, build/test, RFC-index, and packaging text now agree that both app graphs embed only the ten generated strips, maintenance/review/source inventory stays out, core-only is clean, and no installable-package certification is claimed. `verify_documentation.py`, all 64 documentation tests, and `coding-format-check` pass. | `fe12d5a9` |

The final composed report identifies `qt_platform: offscreen` and
`memory_scope: whole-process Mach TASK_VM_INFO`; it is not relabelled as native
window-backend allocation. Before showing a window the process measured
21,676,032 resident and 4,096,576 physical-footprint bytes. One composed window
measured 125,730,816 resident and 64,832,304 physical-footprint bytes; two
measured 150,716,416 and 85,738,312 respectively. The production cache accounts
for 23,855,104 pixel bytes, 23,872,384 retained bytes, and a 26,196,828-byte
conversion peak, with one shared identity and one decode of ten clips.

During a measured 1,801 ms Full interval, the active window advanced eight
frames with eight timer wakeups and eight paints while its inactive sibling
remained unchanged. Geometry rebuilds, publications, pane measurements, and
semantic-index adoptions did not change. During a 1,200 ms Reduced interval,
the active and sibling Dif statistics and all comparison counters remained
unchanged. Capture itself caused the one later paint visible in the serialized
capture list; the bounded interval samples precede that capture.

## Native qualification boundary

Two stronger unattended Cocoa attempts were kept as negative evidence. The
composed multi-window executable stopped at `ordinary Full motion activates
Home playback`, and the native appearance probe reported
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` followed by
`NATIVE_APPEARANCE_PRECONDITION_REFUSED:owner_focus_required:none`. In both
cases the process was not the active owner window. The product's activation
guard therefore refused animation instead of weakening lifecycle policy for a
test. These failures do not contradict the deterministic counter course or the
successful tests-disabled native static render; they leave active native motion
and appearance judgment to the attended pass.

The current macOS motion value is Reduced and the native source read it. The
injected delivery, duplicate suppression, unsubscribe fencing, and shutdown
course passes. No command changed the real Accessibility setting, so actual
System Settings Full-to-Reduced and Reduced-to-Full delivery remains pending.
Windows and Linux were not available in this milestone. Their selected
implementation truthfully reports `unsupported`, which resolves to Reduced;
neither platform is represented as executed or passing.

## Acceptance reconciliation

- A1-A7 remain passed by the immutable d10 source/art/derivative evidence.
- A8 has complete mutable source, application, documentation, packaging, and
  history handoff. Formal RFC status still waits on BPR-0768/BPR-0769 and the
  repository's canonical closeout rules.
- B1-B3 and B5-B8 remain passed by unchanged accepted resource, runtime, Home,
  and context evidence.
- B4 has native macOS current-state and injected lifecycle evidence; the actual
  OS preference-change gesture is pending attendance. Other targets are
  explicitly unexecuted/unsupported.
- B9 has deterministic hidden/inactive/reduced/modal/minimized/suspended/
  destruction checks plus a successful native reduced render. Active Cocoa
  behavior remains pending owner focus.
- B10 has actual process memory, exact cache accounting, bounded timer/paint
  intervals, inactive-sibling suppression, one decode, and unchanged diff-work
  counters. The Mach numbers are process-wide under the offscreen backend; a
  native-window allocation delta is not fabricated.
- B11 retains accepted 1.0/1.25/2.0 fixture renders and accessibility/focus
  evidence, plus the new tests-disabled native System/Dark/Reduced render. The
  final real-display appearance, placement, and actual-speed judgment is
  pending BPR-0768.
- B12 now has aligned mutable docs, package inventory, selected-graph evidence,
  and this reconciliation. It remains partial until the attended decision,
  independent review, card closure, and formal-status disposition are recorded.

## Attended decision and formal closeout

BPR-0768 uses a fresh external attended-review state. Its one final integrated
candidate binds the exact developer executable, the unchanged accepted art,
and one real-display criterion covering native appearance/placement,
actual-speed Full motion, static Reduced motion, noninterference, and the real
macOS motion-setting transition. Static captures and automated results are
supporting evidence, not a substitute for that answer.

The prepared state is
`/Users/jaeger/.mask/bpr-dif-d14-20260914/attended-review`, session
`fc60f0ba8c2bbff96a6db35cb939c3d2`, sequence 3. Candidate
`BPR-0768-native-rendered-integration-d14-v001` is ready and protected with
fingerprint
`4a3b7b7f9e33de88b1b3f42a999bf60bbe41ec034ac200b6df61d2503d221eda`.
Its [sanitized manifest](dif-verification-closeout-v001/attended-manifest.json)
contains the exact absolute launch path, digest, isolated fixture state and
one criterion. No prior attended decision was copied into this state.

RFC-BPR-0030 and RFC-BPR-0031 remain `Proposed` while BPR-0768 is pending and
BPR-0769 is dependency-ineligible. No RFC or index status is advanced from a
partial human/native result. The final coordinator must consume the exact
candidate fingerprint and decision, rerun only invalidated focused evidence,
then adjudicate BPR-0769 and the canonical formal-status rule.

## Execution record

The d14 measurement worker owned only
`tests/end_to_end/profile_multi_window_public_path_tests.cpp`; three read-only
scout/utility slices inventoried native/resource, docs/package, and attended
surfaces. No delegated writer committed. All children returned. Generated role
configuration and self-reported banners are retained as routing evidence only;
effective serving telemetry and runtime child-allowlist enforcement were not
exposed. No GitHub/GHES mutation, credential access, art generation, push, or
publication occurred.
