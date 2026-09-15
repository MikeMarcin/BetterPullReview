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
| Actual composed memory/timer/paint work | The network-isolated `application_profile_multi_window_public_path` passed in 75.82 seconds with `BPR_DIF_INTEGRATION_REPORT_PATH` and production resources. It sampled Mach `TASK_VM_INFO`, the shared cache, full-motion work, reduced-motion quiescence, inactive-sibling stability, and comparison-surface counters. | report source `62354ed6`, tightened ceiling `0df6f24c`; local report `build/dif-verification-closeout-d14/offscreen-report-v002.json`, `sha256:b4d68bac000e4f72af441d250d34f390fe5ac16ee0bd49ad86d460c8ce69ef12` |
| Tests-disabled native application | The app-release executable ran through the normal private automated profile on Cocoa/QMacStyle and emitted `PUBLIC_PATH_OK`, `RENDER_OK`, `WORKBENCH_HOST_COMPOSED_OK`, and `DIF_PREFERENCES:shown:system:reduced`. Its dark System Home render contains accepted Dif with functional account/open controls. | app-release digest above; local render `build/dif-verification-closeout-d14/release-native/home.png`, `sha256:a7572475715f58b5711beafc87dec24d0bc782c31dbd9a3c58a4b62e080c6f58` |
| Maintainer documentation and package inventory | Live product, architecture, appearance, security, build/test, RFC-index, and packaging text now agree that both app graphs embed only the ten generated strips, maintenance/review/source inventory stays out, core-only is clean, and no installable-package certification is claimed. `verify_documentation.py`, all 64 documentation tests, and `coding-format-check` pass. | `fe12d5a9` |

The final composed report identifies `qt_platform: offscreen` and
`memory_scope: whole-process Mach TASK_VM_INFO`; it is not relabelled as native
window-backend allocation. Before showing a window the process measured
21,676,032 resident and 4,096,576 physical-footprint bytes. One composed window
measured 126,173,184 resident and 65,274,672 physical-footprint bytes; two
measured 150,831,104 and 86,115,120 respectively. The production cache has
23,855,104 exact decoded pixel bytes. Its 23,872,384 retained-byte and
26,196,828 conversion-peak values are conservative arithmetic estimates using
a fixed 16-KiB allowance for opaque Qt/allocator overhead, not allocation
measurements. One identity is shared and ten clips decode once.

During a measured 1,800 ms Full interval, the active window advanced eight
frames with eight timer wakeups and eight paints while its inactive sibling
remained unchanged. The test ceiling follows the 84 ms minimum timer interval,
with a two-wakeup scheduling allowance. Geometry rebuilds, publications, pane
measurements, and semantic-index adoptions did not change. During a 1,200 ms Reduced interval,
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

The current macOS motion value was Reduced and the native source read it. The
injected delivery, duplicate suppression, unsubscribe fencing, and shutdown
course passes. The attended System-motion candidate then changed the real
Accessibility setting to the opposite value and back while the Cocoa owner
window was focused; the human accepted both directions, the lack of queued or
repeated greeting work, and restoration of the original setting. Windows and
Linux were not available in this milestone. Their selected
implementation truthfully reports `unsupported`, which resolves to Reduced;
neither platform is represented as executed or passing.

## Acceptance reconciliation

- A1-A7 remain passed by the immutable d10 source/art/derivative evidence.
- A8 has complete mutable source, application, documentation, packaging, and
  history handoff. Formal RFC status still waits on BPR-0769, B10, and the
  repository's canonical closeout rules.
- B1-B3 and B5-B8 remain passed by unchanged accepted resource, runtime, Home,
  and context evidence.
- B4 passes on the claimed macOS target: current-state and injected lifecycle
  evidence are joined by a human-accepted real System-setting change in both
  directions with the original setting restored. Other targets are explicitly
  unexecuted/unsupported.
- B9 has deterministic hidden/inactive/reduced/modal/minimized/suspended/
  destruction checks plus a successful native reduced render. The attended
  focused Cocoa course accepts Full greeting/calm/blink, static Reduced, and a
  return to calm Full without queued playback or repeated greeting.
- B10 has actual process memory, exact decoded pixels, conservative retained/
  conversion accounting, bounded timer/paint intervals, inactive-sibling
  suppression, one decode, and unchanged diff-work counters. It remains
  partial because the fixed 16-KiB overhead allowance is not a measured
  allocation and the process-wide offscreen Mach samples do not isolate
  conversion or native-window backend costs.
- B11 passes: accepted 1.0/1.25/2.0 fixture renders and accessibility/focus
  evidence now join exact human accepts for real-display native appearance,
  placement/controls, actual-speed Full/Reduced motion, and System motion.
- B12 now has aligned mutable docs, package inventory, selected-graph evidence,
  this reconciliation, a consumed independent review, and the exact attended
  decisions. It remains partial until B10, BPR-0769, and formal-status
  disposition are resolved.

## Attended decision and formal closeout

BPR-0768 uses a fresh external attended-review state. Four independent
candidates bind the exact developer executable and unchanged accepted art to
native appearance, placement/controls, actual-speed Full/Reduced motion, and
the real macOS motion-setting transition. Static captures and automated results
are supporting evidence, not substitutes for those answers.

The first prepared candidate was released unanswered after independent review
found that it combined separable criteria. Its session is historical only. The
corrected [sanitized manifest](dif-verification-closeout-v001/attended-manifest.json)
has one question per candidate for native appearance, placement/controls,
Full/Reduced motion, and actual macOS System-motion observation. Each binds the
same exact executable and a distinct isolated fixture state. Fresh external
session `ef04771118304d90b3b73259e0604e58` retained these exact fingerprints:

| Candidate | Fingerprint |
|---|---|
| `BPR-0768-native-appearance-d14-v002` | `8f8d7b7b923e068d772149f60e54215f6d870b0052f1ecbe6e2330c7811b715e` |
| `BPR-0768-native-placement-d14-v002` | `45b03f0ff28503f647f40a4eb5d718b9f26f8811b154a7ec137a69e423f876ae` |
| `BPR-0768-native-motion-d14-v002` | `94235df2567d102f6951802e619123f67a8fc90f0d23805a93a22cd86ae62c2d` |
| `BPR-0768-system-motion-observer-d14-v002` | `b8cc61ad6e02d9126d15df688f57217ecf67b9384dc5f4c79b6953d90c98969b` |

No prior decision was copied forward. The human accepted native appearance,
Full/Reduced motion, placement/controls, and the real System-motion transition
at sequences 13 through 16 with empty notes. Sequence 17 handed the session
back with `pending: []`. The [sanitized verdict](dif-verification-closeout-v001/human-verdict.json)
binds the decision IDs, criteria, fingerprints, executable digest, and stopped
external state without retaining the loopback token. These decisions satisfy
BPR-0768 and B4/B9/B11; they do not measure B10's allocation costs.

## Independent review

The d14 reviewer found no production regression in migration, resource
selection, lifetime, or cue identity. It did find two blocking evidence defects:
the first attended candidate combined separable questions, and B10 described
conservative retained/conversion arithmetic as exact measured accounting. The
candidate was released unanswered and replaced by the four questions above;
B10 now remains partial with its fixed 16-KiB allowance and offscreen/backend
limits explicit. The reviewer also observed that the test ceiling was too loose,
the Full inactive sibling lacked an assertion, and mutable appearance prose
still described the historical prop concern as future runtime work. Revision
`0df6f24c` tightens the ceiling and asserts the sibling; the current appearance
guide states that the accepted runtime is already prop-free. The same reviewer
reinspected those corrections and reported no remaining blocking or
non-blocking finding. It retained actual allocation overhead/conversion/backend
measurements, the attended results, and formal closeout as explicit limits.

RFC-BPR-0030 and RFC-BPR-0031 remain `Proposed`. BPR-0768's human/native
requirement is complete, but B10 still lacks separately measured actual
Qt/container overhead, transient conversion peak, and native backend cost.
BPR-0769 and formal status therefore remain open until that technical criterion
is satisfied; the attended decisions do not waive it.

## Execution record

The d14 measurement worker owned only
`tests/end_to_end/profile_multi_window_public_path_tests.cpp`; three read-only
scout/utility slices inventoried native/resource, docs/package, and attended
surfaces. No delegated writer committed. All children returned. Generated role
configuration and self-reported banners are retained as routing evidence only;
effective serving telemetry and runtime child-allowlist enforcement were not
exposed. No GitHub/GHES mutation, credential access, art generation, push, or
publication occurred.

Renewed dispatch `d15` consumed the exact four human decisions, stopped the
attended host, and retained the external session as durable historical evidence.
No source or selected artifact changed, so the prior focused automated checks
and independent review remain valid. B10 is the only unresolved technical
acceptance row before BPR-0769 can adjudicate formal closeout.
